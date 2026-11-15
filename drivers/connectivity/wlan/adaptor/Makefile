# SPDX-License-Identifier: GPL-2.0-only
obj-m += trona_wlan_adaptor.o
trona_wlan_adaptor-y := wmt_cdev_wifi.o fw_log_wifi.o
ccflags-y += -I$(src)/../../common/common_main/include -I$(src)/../../common/debug_utility
ccflags-y += -DCREATE_NODE_DYNAMIC=1 -Werror
ccflags-y += -I$(src)/../../common/vendor-headers
ccflags-y += -I$(src)/../../common/common_main/linux/include -I$(src)/../../common/common_main/core/include
ccflags-y += -DMTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT -DWMT_IDC_SUPPORT=0
