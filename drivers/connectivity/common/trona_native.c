// SPDX-License-Identifier: GPL-2.0-only
/* Native Trona resource adapter. Hardware sequence/constants from shipped
 * Amazon MT8183 sources; resources owned through Linux frameworks.
 */
#include <linux/arm-smccc.h>
#include <linux/gpio/consumer.h>
#include <linux/mfd/mt6397/rtc.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/sizes.h>
#include "trona_native.h"

phys_addr_t gConEmiPhyBase;
unsigned long long gConEmiSize;
EXPORT_SYMBOL(gConEmiPhyBase);
EXPORT_SYMBOL(gConEmiSize);
static struct device *conn_dev, *rtc_dev;
static struct regmap *pmic;
static struct reset_control *mcu_reset;
static struct pinctrl *pins;
static struct pinctrl_state *active_pins, *idle_pins;
static struct gpio_desc *tcxo;
static bool powered;

int trona_conn_native_init(struct platform_device *pdev)
{
 struct device_node *mem;
 struct reserved_mem *rmem;
 struct mt6397_rtc *rtc;
 int ret;
 if (!of_machine_is_compatible("amazon,trona") || conn_dev) return -ENODEV;
 mem = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
 if (!mem) return -EINVAL;
 rmem = of_reserved_mem_lookup(mem);
 if (!rmem || !of_property_read_bool(mem, "no-map") ||
     rmem->size != SZ_2M || !IS_ALIGNED(rmem->base, SZ_2M) ||
     rmem->base < 0x40000000ULL || rmem->base + rmem->size > 0xc0000000ULL) {
  of_node_put(mem);
  return -EINVAL;
 }
 of_node_put(mem);
 if (!pdev->dev.pm_domain) return -EPROBE_DEFER;
 mcu_reset = devm_reset_control_get_exclusive(&pdev->dev, "mcu");
 if (IS_ERR(mcu_reset)) return PTR_ERR(mcu_reset);
 ret = reset_control_assert(mcu_reset);
 if (ret) return ret;
 pins = devm_pinctrl_get(&pdev->dev);
 if (IS_ERR(pins)) return PTR_ERR(pins);
 active_pins = pinctrl_lookup_state(pins, "conn-active");
 idle_pins = pinctrl_lookup_state(pins, "conn-idle");
 if (IS_ERR(active_pins)) return PTR_ERR(active_pins);
 if (IS_ERR(idle_pins)) return PTR_ERR(idle_pins);
 tcxo = devm_gpiod_get(&pdev->dev, "tcxo", GPIOD_IN);
 if (IS_ERR(tcxo)) return PTR_ERR(tcxo);
 ret = gpiod_get_value_cansleep(tcxo);
 /* Actual Trona reads 0: independent TCXO/VCN28. Do not guess co-clock setup. */
 if (ret != 0) return ret < 0 ? ret : -EOPNOTSUPP;
 rtc_dev = bus_find_device_by_name(&platform_bus_type, NULL, "mt6358-rtc");
 if (!rtc_dev) return -EPROBE_DEFER;
 rtc = dev_get_drvdata(rtc_dev);
 if (!rtc || rtc->addr_base != 0x588) {
  put_device(rtc_dev); rtc_dev = NULL;
  return -ENODEV;
 }
 pmic = rtc->regmap;
 conn_dev = &pdev->dev;
 gConEmiPhyBase = rmem->base;
 gConEmiSize = rmem->size;
 pm_runtime_enable(conn_dev);
 dev_info(conn_dev, "native Trona CONSYS: reserved %pa + %#llx, standalone TCXO\n",
          &gConEmiPhyBase, gConEmiSize);
 return 0;
}
void trona_conn_native_exit(void)
{
 if (conn_dev) {
  if (powered) pm_runtime_put_sync(conn_dev);
  pm_runtime_disable(conn_dev);
 }
 powered = false;
 conn_dev = NULL;
 pmic = NULL;
 if (rtc_dev) put_device(rtc_dev);
 rtc_dev = NULL;
 gConEmiPhyBase = 0; gConEmiSize = 0;
}
int trona_conn_pmic_write(unsigned int reg, unsigned int value)
{
 if (!pmic) return -ENODEV;
 if (reg != 0x1c5a && reg != 0x1d1e) return -EINVAL;
 return regmap_write(pmic, reg, value);
}
int trona_conn_pmic_update(unsigned int reg, unsigned int mask, unsigned int value)
{
 if (!pmic) return -ENODEV;
 if ((reg != 0x1d8a && reg != 0x1d90) || mask != BIT(1)) return -EINVAL;
 return regmap_update_bits(pmic, reg, mask, value);
}
int trona_conn_power(bool enable)
{
 int ret;
 if (!conn_dev) return -ENODEV;
 if (enable == powered) return 0;
 ret = enable ? pm_runtime_resume_and_get(conn_dev) : pm_runtime_put_sync(conn_dev);
 if (ret < 0) return ret;
 powered = enable;
 return 0;
}
int trona_conn_reset(bool assert)
{
 if (!conn_dev) return -ENODEV;
 return assert ? reset_control_assert(mcu_reset) : reset_control_deassert(mcu_reset);
}
int trona_conn_pinmux(bool enable)
{
 if (!conn_dev) return -ENODEV;
 return pinctrl_select_state(pins, enable ? active_pins : idle_pins);
}
int trona_conn_co_clock_type(void)
{
 return conn_dev ? gpiod_get_value_cansleep(tcxo) : -ENODEV;
}
int trona_conn_clockbuf(bool enable)
{
 /* Board's independent TCXO path must never request the PMIC co-clock. */
 if (conn_dev) dev_err(conn_dev, "unexpected PMIC co-clock request (%d)\n", enable);
 return -EOPNOTSUPP;
}
static u32 emi_read(u32 offset)
{
 struct arm_smccc_res res;
 arm_smccc_smc(0xc2000208, offset, 0, 0, 0, 0, 0, 0, &res);
 return res.a0;
}
int trona_conn_mpu_protect(void)
{
 struct arm_smccc_res res;
 u64 start, end;
 u32 apc[2] = {0, 0}, old_sa, old_ea, old_apc;
 int domain, group;
 if (!conn_dev || gConEmiSize != SZ_2M) return -ENODEV;
 start = gConEmiPhyBase + SZ_512K - SZ_64K;
 end = gConEmiPhyBase + gConEmiSize - 1;
 /* Stock permission matrix: domains 0 (AP) and 2 (CONN), all others forbidden. */
 for (domain = 0; domain < 16; domain++)
  apc[domain / 8] |= (domain == 0 || domain == 2 ? 0U : 5U) << ((domain % 8) * 3);
 apc[0] |= BIT(31); /* lock only this region, as shipped */
 old_sa = emi_read(0x168); old_ea = emi_read(0x268); old_apc = emi_read(0x368);
 /* Lock is a secure-monitor command bit, not returned in the APC register. */
 if (old_sa <= old_ea) {
  if (old_sa != (start - 0x40000000ULL) >> 16 ||
      old_ea != (end - 0x40000000ULL) >> 16 ||
      (old_apc & 0xffffff) != (apc[0] & 0xffffff) || emi_read(0x468) != apc[1]) return -EBUSY;
  return 0;
 }
 /* Refuse to overwrite any existing valid, different EMI region. */
 if (old_sa <= old_ea) return -EBUSY;
 for (group = 1; group >= 0; group--) {
  arm_smccc_smc(0xc2000209, (start >> 16) | (26U << 24),
                (end >> 16) | ((u32)group << 24), apc[group], 0, 0, 0, 0, &res);
  if ((long)res.a0 < 0) { dev_err(conn_dev, "EMI SMC group%d failed: %#lx\n", group, res.a0); return -EIO; }
 }
 if (emi_read(0x168) != (start - 0x40000000ULL) >> 16 ||
     emi_read(0x268) != (end - 0x40000000ULL) >> 16 ||
     (emi_read(0x368) & 0xffffff) != (apc[0] & 0xffffff) || emi_read(0x468) != apc[1]) return -EIO;
 dev_info(conn_dev, "EMI region26 protected and readback verified: %#llx-%#llx\n", start, end);
 return 0;
}
