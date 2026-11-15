# Native WLAN is the normal internet path. Explicit USB rescue opt-in:
# TRONA_USB_PROXY=1 source /etc/profile.d/trona-usb-proxy.sh
if [ "${TRONA_USB_PROXY:-0}" = 1 ]; then
  export http_proxy=http://127.0.0.1:18080
  export https_proxy=http://127.0.0.1:18080
  export no_proxy=localhost,127.0.0.1,169.254.77.2
fi
