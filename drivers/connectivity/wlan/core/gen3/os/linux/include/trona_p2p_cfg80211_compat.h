/* SPDX-License-Identifier: GPL-2.0-only */
/* Linux 6.18 station callbacks. MT6631 does not implement MLO. */
static int trona_p2p_add_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool pair, const u8 *mac, struct key_params *p)
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_p2p_cfg80211_add_key(w, d, idx, pair, mac, p);
}
static int trona_p2p_get_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool pair, const u8 *mac, void *cookie,
 void (*callback)(void *, struct key_params *))
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_p2p_cfg80211_get_key(w, d, idx, pair, mac, cookie, callback);
}
static int trona_p2p_del_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool pair, const u8 *mac)
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_p2p_cfg80211_del_key(w, d, idx, pair, mac);
}
static int trona_p2p_default_key(struct wiphy *w, struct net_device *d, int link,
 u8 idx, bool unicast, bool multicast)
{
 if (link >= 0) return -EOPNOTSUPP;
 return mtk_p2p_cfg80211_set_default_key(w, d, idx, unicast, multicast);
}
static void trona_p2p_mgmt_regs(struct wiphy *w, struct wireless_dev *d,
 struct mgmt_frame_regs *r)
{
 mtk_p2p_cfg80211_mgmt_frame_register(w, d, IEEE80211_STYPE_PROBE_REQ,
  !!(r->interface_stypes & BIT(IEEE80211_STYPE_PROBE_REQ >> 4)));
 mtk_p2p_cfg80211_mgmt_frame_register(w, d, IEEE80211_STYPE_ACTION,
  !!(r->interface_stypes & BIT(IEEE80211_STYPE_ACTION >> 4)));
}

static int trona_p2p_beacon(struct wiphy *w, struct net_device *d, struct cfg80211_ap_update *u)
{ return mtk_p2p_cfg80211_change_beacon(w, d, &u->beacon); }
static int trona_p2p_stop(struct wiphy *w, struct net_device *d, unsigned int link)
{ if (link) return -EOPNOTSUPP; return mtk_p2p_cfg80211_stop_ap(w, d); }
static int trona_p2p_params(struct wiphy *w, int radio, u32 changed)
{ if (radio >= 0) return -EOPNOTSUPP; return mtk_p2p_cfg80211_set_wiphy_params(w, changed); }
static int trona_p2p_bitrate(struct wiphy *w, struct net_device *d, unsigned int link,
 const u8 *peer, const struct cfg80211_bitrate_mask *mask)
{ if (link) return -EOPNOTSUPP; return mtk_p2p_cfg80211_set_bitrate_mask(w, d, peer, mask); }
static int trona_p2p_set_power(struct wiphy *w, struct wireless_dev *d, int radio,
 enum nl80211_tx_power_setting type, int mbm)
{ if (radio >= 0) return -EOPNOTSUPP; return mtk_p2p_cfg80211_set_txpower(w, d, type, mbm); }
static int trona_p2p_get_power(struct wiphy *w, struct wireless_dev *d, int radio,
 unsigned int link, int *dbm)
{ if (radio >= 0 || link) return -EOPNOTSUPP; return mtk_p2p_cfg80211_get_txpower(w, d, dbm); }
