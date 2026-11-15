# SPDX-License-Identifier: GPL-2.0-only
# Amazon MT8183 BTIF sources, built as an external module for Trona Linux.
obj-m += trona_btif.o
trona_btif-y := common/mtk_btif.o common/mtk_btif_exp.o common/btif_dma_plat.o common/btif_plat.o
ccflags-y += -I$(src)/common/inc -I$(src)/common/plat_inc -I$(src)/../common/vendor-headers
ccflags-y += -DBTIF_DBG_SUPPORT=0 -DCONFIG_MACH_MT8183=1 -Werror
