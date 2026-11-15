/* SPDX-License-Identifier: GPL-2.0-only */
/* Linux 6.18 station callbacks. MT6631 does not implement MLO. */
static int trona_add_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool pair, const u8 *mac, struct key_params *p)
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_cfg80211_add_key(w, d, idx, pair, mac, p);
}
static int trona_get_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool pair, const u8 *mac, void *cookie,
 void (*callback)(void *, struct key_params *))
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_cfg80211_get_key(w, d, idx, pair, mac, cookie, callback);
}
static int trona_del_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool pair, const u8 *mac)
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_cfg80211_del_key(w, d, idx, pair, mac);
}
static int trona_default_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool unicast, bool multicast)
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_cfg80211_set_default_key(w, d, idx, unicast, multicast);
}
static void trona_mgmt_regs(struct wiphy *w, struct wireless_dev *d,
 struct mgmt_frame_regs *r)
{
 mtk_cfg80211_mgmt_frame_register(w, d, IEEE80211_STYPE_PROBE_REQ,
  !!(r->interface_stypes & BIT(IEEE80211_STYPE_PROBE_REQ >> 4)));
 mtk_cfg80211_mgmt_frame_register(w, d, IEEE80211_STYPE_ACTION,
  !!(r->interface_stypes & BIT(IEEE80211_STYPE_ACTION >> 4)));
}
#if CFG_SUPPORT_TDLS
static int trona_tdls_mgmt(struct wiphy *w, struct net_device *d,
 const u8 *peer, int link, u8 action, u8 token, u16 status,
 u32 cap, bool initiator, const u8 *buf, size_t len)
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_cfg80211_tdls_mgmt(w, d, peer, action, token, status, cap, initiator, buf, len);
}
#endif
