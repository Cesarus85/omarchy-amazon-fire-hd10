// SPDX-License-Identifier: GPL-2.0-only
/* Native Linux HCI transport over the Trona WMT/STP radio channel.
 * STP API: original Amazon/MediaTek connectivity sources in ../common.
 * This uses the queued STP channel, not the obsolete Android/BlueZ shortcut.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/skbuff.h>
#include <linux/unaligned.h>
#include <linux/workqueue.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

extern int mtk_wcn_wmt_func_on(int type);
extern int mtk_wcn_wmt_func_off(int type);
extern int mtk_wcn_stp_is_ready(void);
extern int mtk_wcn_stp_send_data(unsigned char *data, unsigned int len, unsigned char type);
extern int mtk_wcn_stp_receive_data(unsigned char *data, unsigned int len, unsigned char type);
extern int mtk_wcn_stp_register_event_cb(int type, void (*callback)(void));
extern int mtk_wcn_stp_register_tx_event_cb(int type, void (*callback)(void));
extern void mtk_wcn_stp_set_bluez(int enabled);

struct trona_bt {
 struct hci_dev *hdev;
 struct platform_device *pdev;
 struct work_struct rx_work;
 struct delayed_work tx_work;
 struct sk_buff_head txq;
 struct sk_buff *rx;
 unsigned int target;
 bool header, opened;
 bdaddr_t factory_address;
};
static struct trona_bt radio;

static void rx_ready(void)
{
 if (READ_ONCE(radio.opened)) schedule_work(&radio.rx_work);
}
static void tx_ready(void)
{
 if (READ_ONCE(radio.opened)) mod_delayed_work(system_wq, &radio.tx_work, 0);
}
static int consume_rx(struct trona_bt *bt, const u8 *data, unsigned int length)
{
 while (length) {
  unsigned int n, payload;
  if (!bt->rx) {
   u8 type = *data++;
   length--;
   switch (type) {
   case HCI_EVENT_PKT: bt->target = HCI_EVENT_HDR_SIZE; break;
   case HCI_ACLDATA_PKT: bt->target = HCI_ACL_HDR_SIZE; break;
   case HCI_SCODATA_PKT: bt->target = HCI_SCO_HDR_SIZE; break;
   default: return -EILSEQ;
   }
   bt->rx = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_KERNEL);
   if (!bt->rx) return -ENOMEM;
   hci_skb_pkt_type(bt->rx) = type;
   bt->header = true;
  }
  n = min(length, bt->target - bt->rx->len);
  skb_put_data(bt->rx, data, n);
  data += n; length -= n;
  if (bt->rx->len < bt->target) continue;
  if (bt->header) {
   if (hci_skb_pkt_type(bt->rx) == HCI_EVENT_PKT) payload = bt->rx->data[1];
   else if (hci_skb_pkt_type(bt->rx) == HCI_ACLDATA_PKT) payload = get_unaligned_le16(bt->rx->data + 2);
   else payload = bt->rx->data[2];
   if (payload > skb_tailroom(bt->rx)) return -EMSGSIZE;
   bt->target += payload;
   bt->header = false;
   if (payload) continue;
  }
  if (bt->rx->len == bt->target) {
   struct sk_buff *packet = bt->rx;
   bt->rx = NULL;
   hci_recv_frame(bt->hdev, packet);
  }
 }
 return 0;
}
static void receive_work(struct work_struct *work)
{
 struct trona_bt *bt = container_of(work, struct trona_bt, rx_work);
 u8 data[1024];
 int n, ret;
 while (READ_ONCE(bt->opened)) {
  n = mtk_wcn_stp_receive_data(data, sizeof(data), 0);
  if (n <= 0) break;
  bt->hdev->stat.byte_rx += n;
  ret = consume_rx(bt, data, n);
  if (ret) {
   bt_dev_err(bt->hdev, "STP HCI receive error %d", ret);
   bt->hdev->stat.err_rx++;
   kfree_skb(bt->rx); bt->rx = NULL;
  }
 }
}
static void transmit_work(struct work_struct *work)
{
 struct trona_bt *bt = container_of(to_delayed_work(work), struct trona_bt, tx_work);
 struct sk_buff *skb;
 while (READ_ONCE(bt->opened) && (skb = skb_dequeue(&bt->txq))) {
  int n = mtk_wcn_stp_send_data(skb->data, skb->len, 0);
  if (!n) {
   skb_queue_head(&bt->txq, skb);
   mod_delayed_work(system_wq, &bt->tx_work, msecs_to_jiffies(10));
   return;
  }
  if (n == skb->len) bt->hdev->stat.byte_tx += n;
  else { bt->hdev->stat.err_tx++; bt_dev_err(bt->hdev, "STP HCI transmit failed: %d", n); }
  kfree_skb(skb);
 }
}
static int trona_open(struct hci_dev *hdev)
{
 struct trona_bt *bt = hci_get_drvdata(hdev);
 int ret;
 if (!mtk_wcn_wmt_func_on(0)) return -EIO;
 if (!mtk_wcn_stp_is_ready()) { ret = -EAGAIN; goto off; }
 mtk_wcn_stp_set_bluez(0);
 ret = mtk_wcn_stp_register_event_cb(0, rx_ready);
 if (ret) goto off;
 ret = mtk_wcn_stp_register_tx_event_cb(0, tx_ready);
 if (ret) { mtk_wcn_stp_register_event_cb(0, NULL); goto off; }
 WRITE_ONCE(bt->opened, true);
 schedule_work(&bt->rx_work);
 return 0;
off:
 mtk_wcn_wmt_func_off(0);
 return ret;
}
static int trona_flush(struct hci_dev *hdev)
{
 struct trona_bt *bt = hci_get_drvdata(hdev);
 cancel_delayed_work_sync(&bt->tx_work);
 skb_queue_purge(&bt->txq);
 return 0;
}
static int trona_close(struct hci_dev *hdev)
{
 struct trona_bt *bt = hci_get_drvdata(hdev);
 int ret;
 WRITE_ONCE(bt->opened, false);
 mtk_wcn_stp_register_tx_event_cb(0, NULL);
 cancel_delayed_work_sync(&bt->tx_work);
 skb_queue_purge(&bt->txq);
 /* The serialized WMT response quiesces hardware before removing RX callback. */
 ret = mtk_wcn_wmt_func_off(0) ? 0 : -EIO;
 mtk_wcn_stp_register_event_cb(0, NULL);
 cancel_work_sync(&bt->rx_work);
 kfree_skb(bt->rx); bt->rx = NULL;
 return ret;
}
static int trona_send(struct hci_dev *hdev, struct sk_buff *skb)
{
 struct trona_bt *bt = hci_get_drvdata(hdev);
 int ret;
 if (!READ_ONCE(bt->opened)) return -ENETDOWN;
 if (skb_queue_len(&bt->txq) >= 256) return -ENOBUFS;
 ret = skb_cow_head(skb, 1);
 if (ret) return ret;
 switch (hci_skb_pkt_type(skb)) {
 case HCI_COMMAND_PKT: hdev->stat.cmd_tx++; break;
 case HCI_ACLDATA_PKT: hdev->stat.acl_tx++; break;
 case HCI_SCODATA_PKT: hdev->stat.sco_tx++; break;
 default: return -EINVAL;
 }
 *(u8 *)skb_push(skb, 1) = hci_skb_pkt_type(skb);
 skb_queue_tail(&bt->txq, skb);
 tx_ready();
 return 0;
}
static int trona_set_address(struct hci_dev *hdev, const bdaddr_t *address)
{
 struct sk_buff *response;
 response = __hci_cmd_sync(hdev, 0xfc1a, sizeof(*address), address, HCI_INIT_TIMEOUT);
 if (IS_ERR(response)) return PTR_ERR(response);
 kfree_skb(response);
 return 0;
}
static int trona_setup(struct hci_dev *hdev)
{
 struct trona_bt *bt = hci_get_drvdata(hdev);
 return trona_set_address(hdev, &bt->factory_address);
}
static int trona_post_init(struct hci_dev *hdev)
{
 /* The stock MT6631 firmware omits the legacy LE commands from Read Local
  * Supported Commands, although scanning and LE HID connections work.
  * Linux consequently enables only LTK requests (0x10), hiding advertising
  * reports and connection completion. Restore the five legacy LE events
  * after each core initialization; do not invent a supported-command bitmap.
  * This transport is restricted to amazon,trona and its stock firmware.
  */
 const u8 events[8] = { 0x1f };
 int ret;

 if (!lmp_le_capable(hdev)) return 0;
 ret = __hci_cmd_sync_status(hdev, HCI_OP_LE_SET_EVENT_MASK,
                            sizeof(events), events, HCI_INIT_TIMEOUT);
 if (!ret) bt_dev_info(hdev, "Trona legacy LE events enabled");
 return ret;
}
static int __init trona_init(void)
{
 struct device_node *node;
 const u8 *address;
 int i, length, ret;
 if (!of_machine_is_compatible("amazon,trona")) return -ENODEV;
 node = of_find_node_by_path("/idme/bt_mac_addr");
 if (!node) return -ENODEV;
 address = of_get_property(node, "value", &length);
 if (!address || length != 13 || address[12]) { of_node_put(node); return -EINVAL; }
 for (i = 0; i < 6; i++) {
  int hi = hex_to_bin(address[i * 2]), lo = hex_to_bin(address[i * 2 + 1]);
  if (hi < 0 || lo < 0) { of_node_put(node); return -EINVAL; }
  radio.factory_address.b[5-i] = (hi << 4) | lo;
 }
 of_node_put(node);
 node = of_find_compatible_node(NULL, NULL, "mediatek,mt8183-consys");
 if (!node) return -ENODEV;
 radio.pdev = of_find_device_by_node(node);
 of_node_put(node);
 if (!radio.pdev) return -EPROBE_DEFER;
 radio.hdev = hci_alloc_dev();
 if (!radio.hdev) { ret = -ENOMEM; goto put_parent; }
 INIT_WORK(&radio.rx_work, receive_work);
 INIT_DELAYED_WORK(&radio.tx_work, transmit_work);
 skb_queue_head_init(&radio.txq);
 hci_set_drvdata(radio.hdev, &radio);
 radio.hdev->bus = HCI_UART;
 SET_HCIDEV_DEV(radio.hdev, &radio.pdev->dev);
 radio.hdev->open = trona_open;
 radio.hdev->close = trona_close;
 radio.hdev->flush = trona_flush;
 radio.hdev->send = trona_send;
 radio.hdev->setup = trona_setup;
 radio.hdev->post_init = trona_post_init;
 radio.hdev->set_bdaddr = trona_set_address;
 hci_set_quirk(radio.hdev, HCI_QUIRK_NON_PERSISTENT_SETUP);
 ret = hci_register_dev(radio.hdev);
 if (!ret) return 0;
 hci_free_dev(radio.hdev);
put_parent:
 put_device(&radio.pdev->dev);
 return ret;
}
static void __exit trona_exit(void)
{
 hci_unregister_dev(radio.hdev);
 cancel_work_sync(&radio.rx_work);
 cancel_delayed_work_sync(&radio.tx_work);
 hci_free_dev(radio.hdev);
 put_device(&radio.pdev->dev);
}
module_init(trona_init);
module_exit(trona_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Native Trona Bluetooth HCI over WMT/STP");
