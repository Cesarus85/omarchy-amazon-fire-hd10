/* Trona adaptation: Linux 6.18 HI556 + Amazon Trona GPL register tables
 * from android_kernel_amazon_mt8183 commit 7ec747b4. 24MHz, 2592x1944,
 * two lanes at 880Mb/s; retain V4L2 controls and runtime power management. */
// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 Intel Corporation.

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/unaligned.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define HI556_REG_VALUE_08BIT		1
#define HI556_REG_VALUE_16BIT		2
#define HI556_REG_VALUE_24BIT		3

#define HI556_LINK_FREQ_437MHZ		440000000ULL
#define HI556_MCLK			24000000
#define HI556_DATA_LANES		2
#define HI556_RGB_DEPTH			10

#define HI556_REG_CHIP_ID		0x0f16
#define HI556_CHIP_ID			0x0556

#define HI556_REG_MODE_SELECT		0x0a00
#define HI556_MODE_STANDBY		0x0000
#define HI556_MODE_STREAMING		0x0100

/* vertical-timings from sensor */
#define HI556_REG_FLL			0x0006
#define HI556_FLL_30FPS			0x0823
#define HI556_FLL_30FPS_MIN		0x0823
#define HI556_FLL_MAX			0x7fff

/* horizontal-timings from sensor */
#define HI556_REG_LLP			0x0008

/* Exposure controls from sensor */
#define HI556_REG_EXPOSURE		0x0074
#define HI556_EXPOSURE_MIN		6
#define HI556_EXPOSURE_MAX_MARGIN	2
#define HI556_EXPOSURE_STEP		1

/* Analog gain controls from sensor */
#define HI556_REG_ANALOG_GAIN		0x0077
#define HI556_ANAL_GAIN_MIN		0
#define HI556_ANAL_GAIN_MAX		240
#define HI556_ANAL_GAIN_STEP		1

/* Digital gain controls from sensor */
#define HI556_REG_MWB_GR_GAIN		0x0078
#define HI556_REG_MWB_GB_GAIN		0x007a
#define HI556_REG_MWB_R_GAIN		0x007c
#define HI556_REG_MWB_B_GAIN		0x007e
#define HI556_DGTL_GAIN_MIN		0
#define HI556_DGTL_GAIN_MAX		2048
#define HI556_DGTL_GAIN_STEP		1
#define HI556_DGTL_GAIN_DEFAULT		256

/* Test Pattern Control */
#define HI556_REG_ISP			0X0a05
#define HI556_REG_ISP_TPG_EN		0x01
#define HI556_REG_TEST_PATTERN		0x0201

/* HI556 native and active pixel array size. */
#define HI556_NATIVE_WIDTH		2592U
#define HI556_NATIVE_HEIGHT		1944U
#define HI556_PIXEL_ARRAY_LEFT		0U
#define HI556_PIXEL_ARRAY_TOP		0U
#define HI556_PIXEL_ARRAY_WIDTH	2592U
#define HI556_PIXEL_ARRAY_HEIGHT	1944U

enum {
	HI556_LINK_FREQ_437MHZ_INDEX,
};

struct hi556_reg {
	u16 address;
	u16 val;
};

struct hi556_reg_list {
	u32 num_of_regs;
	const struct hi556_reg *regs;
};

struct hi556_link_freq_config {
	const struct hi556_reg_list reg_list;
};

struct hi556_mode {
	/* Frame width in pixels */
	u32 width;

	/* Frame height in pixels */
	u32 height;

	/* Analog crop rectangle. */
	struct v4l2_rect crop;

	/* Horizontal timining size */
	u32 llp;

	/* Default vertical timining size */
	u32 fll_def;

	/* Min vertical timining size */
	u32 fll_min;

	/* Link frequency needed for this resolution */
	u32 link_freq_index;

	/* Sensor register settings for this resolution */
	const struct hi556_reg_list reg_list;
};

#define to_hi556(_sd) container_of(_sd, struct hi556, sd)

//SENSOR_INITIALIZATION
static const struct hi556_reg mipi_data_rate_874mbps[] = {
 { 0x0e00, 0x0102 },
 { 0x0e02, 0x0102 },
 { 0x0e0c, 0x0100 },
 { 0x2000, 0x4031 },
 { 0x2002, 0x8400 },
 { 0x2004, 0x12b0 },
 { 0x2006, 0xe292 },
 { 0x2008, 0x12b0 },
 { 0x200a, 0xe2b0 },
 { 0x200c, 0x12b0 },
 { 0x200e, 0xfce4 },
 { 0x2010, 0x40b2 },
 { 0x2012, 0x0250 },
 { 0x2014, 0x8070 },
 { 0x2016, 0x40b2 },
 { 0x2018, 0x01b8 },
 { 0x201a, 0x8072 },
 { 0x201c, 0x93d2 },
 { 0x201e, 0x00bd },
 { 0x2020, 0x248b },
 { 0x2022, 0x0900 },
 { 0x2024, 0x7312 },
 { 0x2026, 0x43d2 },
 { 0x2028, 0x00bd },
 { 0x202a, 0x12b0 },
 { 0x202c, 0xe60e },
 { 0x202e, 0x12b0 },
 { 0x2030, 0xe64c },
 { 0x2032, 0x12b0 },
 { 0x2034, 0xeb28 },
 { 0x2036, 0x12b0 },
 { 0x2038, 0xe662 },
 { 0x203a, 0x12b0 },
 { 0x203c, 0xe878 },
 { 0x203e, 0x12b0 },
 { 0x2040, 0xfc3a },
 { 0x2042, 0x12b0 },
 { 0x2044, 0xfc62 },
 { 0x2046, 0x12b0 },
 { 0x2048, 0xfc7a },
 { 0x204a, 0x12b0 },
 { 0x204c, 0xe2ca },
 { 0x204e, 0x12b0 },
 { 0x2050, 0xe67c },
 { 0x2052, 0x12b0 },
 { 0x2054, 0xe7dc },
 { 0x2056, 0xb3a2 },
 { 0x2058, 0x0a84 },
 { 0x205a, 0x2402 },
 { 0x205c, 0x12b0 },
 { 0x205e, 0xe9c8 },
 { 0x2060, 0x4382 },
 { 0x2062, 0x7322 },
 { 0x2064, 0x4392 },
 { 0x2066, 0x7326 },
 { 0x2068, 0x12b0 },
 { 0x206a, 0xf946 },
 { 0x206c, 0x12b0 },
 { 0x206e, 0xe438 },
 { 0x2070, 0x12b0 },
 { 0x2072, 0xe4a0 },
 { 0x2074, 0x12b0 },
 { 0x2076, 0xe4ee },
 { 0x2078, 0x12b0 },
 { 0x207a, 0xe534 },
 { 0x207c, 0x12b0 },
 { 0x207e, 0xe586 },
 { 0x2080, 0x12b0 },
 { 0x2082, 0xe6ca },
 { 0x2084, 0x12b0 },
 { 0x2086, 0xe6f0 },
 { 0x2088, 0x12b0 },
 { 0x208a, 0xe722 },
 { 0x208c, 0x4392 },
 { 0x208e, 0x731c },
 { 0x2090, 0x12b0 },
 { 0x2092, 0xea52 },
 { 0x2094, 0x12b0 },
 { 0x2096, 0xfc8e },
 { 0x2098, 0x12b0 },
 { 0x209a, 0xe5a2 },
 { 0x209c, 0x93c2 },
 { 0x209e, 0x8294 },
 { 0x20a0, 0x2009 },
 { 0x20a2, 0x0b00 },
 { 0x20a4, 0x7302 },
 { 0x20a6, 0x0258 },
 { 0x20a8, 0x4382 },
 { 0x20aa, 0x7902 },
 { 0x20ac, 0x0900 },
 { 0x20ae, 0x7308 },
 { 0x20b0, 0x12b0 },
 { 0x20b2, 0xf946 },
 { 0x20b4, 0x12b0 },
 { 0x20b6, 0xe342 },
 { 0x20b8, 0x12b0 },
 { 0x20ba, 0xe370 },
 { 0x20bc, 0x12b0 },
 { 0x20be, 0xfd10 },
 { 0x20c0, 0x4292 },
 { 0x20c2, 0x8292 },
 { 0x20c4, 0x7114 },
 { 0x20c6, 0x421f },
 { 0x20c8, 0x7316 },
 { 0x20ca, 0xc312 },
 { 0x20cc, 0x100f },
 { 0x20ce, 0x821f },
 { 0x20d0, 0x8298 },
 { 0x20d2, 0x4f82 },
 { 0x20d4, 0x7334 },
 { 0x20d6, 0x0f00 },
 { 0x20d8, 0x7302 },
 { 0x20da, 0x12b0 },
 { 0x20dc, 0xe6b8 },
 { 0x20de, 0x43b2 },
 { 0x20e0, 0x7a06 },
 { 0x20e2, 0x12b0 },
 { 0x20e4, 0xfcb0 },
 { 0x20e6, 0x9392 },
 { 0x20e8, 0x7114 },
 { 0x20ea, 0x2011 },
 { 0x20ec, 0x0b00 },
 { 0x20ee, 0x7302 },
 { 0x20f0, 0x0258 },
 { 0x20f2, 0x4382 },
 { 0x20f4, 0x7902 },
 { 0x20f6, 0x0800 },
 { 0x20f8, 0x7118 },
 { 0x20fa, 0x12b0 },
 { 0x20fc, 0xeb4c },
 { 0x20fe, 0x0900 },
 { 0x2100, 0x7112 },
 { 0x2102, 0x12b0 },
 { 0x2104, 0xe3e6 },
 { 0x2106, 0x0b00 },
 { 0x2108, 0x7302 },
 { 0x210a, 0x0036 },
 { 0x210c, 0x3fec },
 { 0x210e, 0x0b00 },
 { 0x2110, 0x7302 },
 { 0x2112, 0x0036 },
 { 0x2114, 0x4392 },
 { 0x2116, 0x7902 },
 { 0x2118, 0x4292 },
 { 0x211a, 0x7100 },
 { 0x211c, 0x82be },
 { 0x211e, 0x12b0 },
 { 0x2120, 0xe74a },
 { 0x2122, 0x12b0 },
 { 0x2124, 0xea8e },
 { 0x2126, 0x12b0 },
 { 0x2128, 0xead4 },
 { 0x212a, 0x12b0 },
 { 0x212c, 0xe3e6 },
 { 0x212e, 0x930f },
 { 0x2130, 0x27da },
 { 0x2132, 0x12b0 },
 { 0x2134, 0xfcc8 },
 { 0x2136, 0x3fb0 },
 { 0x2138, 0x12b0 },
 { 0x213a, 0xe5be },
 { 0x213c, 0x12b0 },
 { 0x213e, 0xe5e6 },
 { 0x2140, 0x3f70 },
 { 0x2142, 0x4030 },
 { 0x2144, 0xf770 },
 { 0x2146, 0x12b0 },
 { 0x2148, 0xee16 },
 { 0x214a, 0x12b0 },
 { 0x214c, 0xee5c },
 { 0x214e, 0x12b0 },
 { 0x2150, 0xeeac },
 { 0x2152, 0x12b0 },
 { 0x2154, 0xeeea },
 { 0x2156, 0x12b0 },
 { 0x2158, 0xef06 },
 { 0x215a, 0x12b0 },
 { 0x215c, 0xef28 },
 { 0x215e, 0x12b0 },
 { 0x2160, 0xef98 },
 { 0x2162, 0x12b0 },
 { 0x2164, 0xfb2a },
 { 0x2166, 0x12b0 },
 { 0x2168, 0xfa60 },
 { 0x216a, 0x12b0 },
 { 0x216c, 0xfb4c },
 { 0x216e, 0x12b0 },
 { 0x2170, 0xf294 },
 { 0x2172, 0xd392 },
 { 0x2174, 0x7102 },
 { 0x2176, 0x4130 },
 { 0x2178, 0x120b },
 { 0x217a, 0x120a },
 { 0x217c, 0x1209 },
 { 0x217e, 0x1208 },
 { 0x2180, 0x1207 },
 { 0x2182, 0x4f0c },
 { 0x2184, 0x4e08 },
 { 0x2186, 0x4d47 },
 { 0x2188, 0x425f },
 { 0x218a, 0x00de },
 { 0x218c, 0x4f4e },
 { 0x218e, 0x430f },
 { 0x2190, 0x421a },
 { 0x2192, 0x8308 },
 { 0x2194, 0x421b },
 { 0x2196, 0x830a },
 { 0x2198, 0x8e0a },
 { 0x219a, 0x7f0b },
 { 0x219c, 0x4a0e },
 { 0x219e, 0x4b0f },
 { 0x21a0, 0x4c3a },
 { 0x21a2, 0x4c3b },
 { 0x21a4, 0x4a0c },
 { 0x21a6, 0x4b0d },
 { 0x21a8, 0x8e0c },
 { 0x21aa, 0x7f0d },
 { 0x21ac, 0x284a },
 { 0x21ae, 0x425f },
 { 0x21b0, 0x00de },
 { 0x21b2, 0x4f4a },
 { 0x21b4, 0x430b },
 { 0x21b6, 0xb0f2 },
 { 0x21b8, 0x0010 },
 { 0x21ba, 0x00dd },
 { 0x21bc, 0x2040 },
 { 0x21be, 0x4349 },
 { 0x21c0, 0x421e },
 { 0x21c2, 0x7560 },
 { 0x21c4, 0x421f },
 { 0x21c6, 0x7578 },
 { 0x21c8, 0x12b0 },
 { 0x21ca, 0xefc4 },
 { 0x21cc, 0x4a0c },
 { 0x21ce, 0x4b0d },
 { 0x21d0, 0x8e0c },
 { 0x21d2, 0x7f0d },
 { 0x21d4, 0x2c01 },
 { 0x21d6, 0x4359 },
 { 0x21d8, 0x434f },
 { 0x21da, 0x93c2 },
 { 0x21dc, 0x8294 },
 { 0x21de, 0x2001 },
 { 0x21e0, 0x435f },
 { 0x21e2, 0xf94f },
 { 0x21e4, 0x434e },
 { 0x21e6, 0x93c8 },
 { 0x21e8, 0x0000 },
 { 0x21ea, 0x2001 },
 { 0x21ec, 0x435e },
 { 0x21ee, 0xfe4f },
 { 0x21f0, 0x4fc8 },
 { 0x21f2, 0x0000 },
 { 0x21f4, 0x9347 },
 { 0x21f6, 0x2013 },
 { 0x21f8, 0x4a82 },
 { 0x21fa, 0x7540 },
 { 0x21fc, 0x4b82 },
 { 0x21fe, 0x7574 },
 { 0x2200, 0x934f },
 { 0x2202, 0x2428 },
 { 0x2204, 0x93c2 },
 { 0x2206, 0x00cd },
 { 0x2208, 0x2407 },
 { 0x220a, 0x425f },
 { 0x220c, 0x806d },
 { 0x220e, 0x5f82 },
 { 0x2210, 0x7542 },
 { 0x2212, 0x5f82 },
 { 0x2214, 0x7544 },
 { 0x2216, 0x3c1e },
 { 0x2218, 0x425f },
 { 0x221a, 0x806c },
 { 0x221c, 0x3ff8 },
 { 0x221e, 0x4a82 },
 { 0x2220, 0x754a },
 { 0x2222, 0x4b82 },
 { 0x2224, 0x7576 },
 { 0x2226, 0x934f },
 { 0x2228, 0x2415 },
 { 0x222a, 0x93c2 },
 { 0x222c, 0x00cd },
 { 0x222e, 0x2412 },
 { 0x2230, 0x425f },
 { 0x2232, 0x806d },
 { 0x2234, 0x5f82 },
 { 0x2236, 0x754c },
 { 0x2238, 0x5f82 },
 { 0x223a, 0x754e },
 { 0x223c, 0x3c0b },
 { 0x223e, 0x435f },
 { 0x2240, 0x3fd7 },
 { 0x2242, 0x421e },
 { 0x2244, 0x8308 },
 { 0x2246, 0x421f },
 { 0x2248, 0x830a },
 { 0x224a, 0x8a0e },
 { 0x224c, 0x7b0f },
 { 0x224e, 0x4e0a },
 { 0x2250, 0x4f0b },
 { 0x2252, 0x3fb1 },
 { 0x2254, 0x4137 },
 { 0x2256, 0x4138 },
 { 0x2258, 0x4139 },
 { 0x225a, 0x413a },
 { 0x225c, 0x413b },
 { 0x225e, 0x4130 },
 { 0x2260, 0x425f },
 { 0x2262, 0x00d8 },
 { 0x2264, 0xf37f },
 { 0x2266, 0x421e },
 { 0x2268, 0x0086 },
 { 0x226a, 0x12b0 },
 { 0x226c, 0xefc4 },
 { 0x226e, 0x4e82 },
 { 0x2270, 0x8308 },
 { 0x2272, 0x4f82 },
 { 0x2274, 0x830a },
 { 0x2276, 0x4292 },
 { 0x2278, 0x0088 },
 { 0x227a, 0x7316 },
 { 0x227c, 0x425f },
 { 0x227e, 0x806f },
 { 0x2280, 0x4f82 },
 { 0x2282, 0x7542 },
 { 0x2284, 0x93c2 },
 { 0x2286, 0x00cc },
 { 0x2288, 0x244d },
 { 0x228a, 0x521f },
 { 0x228c, 0x828c },
 { 0x228e, 0x4f82 },
 { 0x2290, 0x7544 },
 { 0x2292, 0x93c2 },
 { 0x2294, 0x00cd },
 { 0x2296, 0x240d },
 { 0x2298, 0x425f },
 { 0x229a, 0x806f },
 { 0x229c, 0x425e },
 { 0x229e, 0x806e },
 { 0x22a0, 0x4f0d },
 { 0x22a2, 0x5e0d },
 { 0x22a4, 0x4d82 },
 { 0x22a6, 0x754c },
 { 0x22a8, 0x521f },
 { 0x22aa, 0x828e },
 { 0x22ac, 0x5e0f },
 { 0x22ae, 0x4f82 },
 { 0x22b0, 0x754e },
 { 0x22b2, 0x425f },
 { 0x22b4, 0x00f3 },
 { 0x22b6, 0xf37f },
 { 0x22b8, 0x403d },
 { 0x22ba, 0x82ee },
 { 0x22bc, 0x421e },
 { 0x22be, 0x00f4 },
 { 0x22c0, 0x12b0 },
 { 0x22c2, 0xefd0 },
 { 0x22c4, 0x93c2 },
 { 0x22c6, 0x00cd },
 { 0x22c8, 0x2023 },
 { 0x22ca, 0x434d },
 { 0x22cc, 0x403e },
 { 0x22ce, 0x8306 },
 { 0x22d0, 0x403f },
 { 0x22d2, 0x82ee },
 { 0x22d4, 0x12b0 },
 { 0x22d6, 0xf978 },
 { 0x22d8, 0x93c2 },
 { 0x22da, 0x00cd },
 { 0x22dc, 0x2011 },
 { 0x22de, 0x4382 },
 { 0x22e0, 0x733e },
 { 0x22e2, 0x93c2 },
 { 0x22e4, 0x00cd },
 { 0x22e6, 0x240b },
 { 0x22e8, 0x421e },
 { 0x22ea, 0x82ee },
 { 0x22ec, 0x421f },
 { 0x22ee, 0x82f0 },
 { 0x22f0, 0x821e },
 { 0x22f2, 0x82f2 },
 { 0x22f4, 0x721f },
 { 0x22f6, 0x82f4 },
 { 0x22f8, 0x2c02 },
 { 0x22fa, 0x4392 },
 { 0x22fc, 0x733e },
 { 0x22fe, 0x4130 },
 { 0x2300, 0x435d },
 { 0x2302, 0x403e },
 { 0x2304, 0x8307 },
 { 0x2306, 0x403f },
 { 0x2308, 0x82f2 },
 { 0x230a, 0x12b0 },
 { 0x230c, 0xf978 },
 { 0x230e, 0x3fe7 },
 { 0x2310, 0x425f },
 { 0x2312, 0x00f2 },
 { 0x2314, 0xf37f },
 { 0x2316, 0x403d },
 { 0x2318, 0x82f2 },
 { 0x231a, 0x421e },
 { 0x231c, 0x00f0 },
 { 0x231e, 0x12b0 },
 { 0x2320, 0xefd0 },
 { 0x2322, 0x3fd3 },
 { 0x2324, 0x521f },
 { 0x2326, 0x828e },
 { 0x2328, 0x3fb2 },
 { 0x232a, 0x421e },
 { 0x232c, 0x7540 },
 { 0x232e, 0x421f },
 { 0x2330, 0x7574 },
 { 0x2332, 0x12b0 },
 { 0x2334, 0xefc4 },
 { 0x2336, 0x4e82 },
 { 0x2338, 0x8346 },
 { 0x233a, 0x4f82 },
 { 0x233c, 0x8348 },
 { 0x233e, 0x93c2 },
 { 0x2340, 0x00c1 },
 { 0x2342, 0x2403 },
 { 0x2344, 0x42d2 },
 { 0x2346, 0x834b },
 { 0x2348, 0x8310 },
 { 0x234a, 0x4130 },
 { 0x234c, 0x4292 },
 { 0x234e, 0x7320 },
 { 0x2350, 0x8344 },
 { 0x2352, 0x93c2 },
 { 0x2354, 0x00c1 },
 { 0x2356, 0x2001 },
 { 0x2358, 0x4130 },
 { 0x235a, 0x12b0 },
 { 0x235c, 0xfb64 },
 { 0x235e, 0x12b0 },
 { 0x2360, 0xfbe8 },
 { 0x2362, 0x3ffa },
 { 0x2364, 0x120b },
 { 0x2366, 0x120a },
 { 0x2368, 0x1209 },
 { 0x236a, 0x1208 },
 { 0x236c, 0x421e },
 { 0x236e, 0x7314 },
 { 0x2370, 0x421f },
 { 0x2372, 0x7336 },
 { 0x2374, 0x12b0 },
 { 0x2376, 0xefc4 },
 { 0x2378, 0x4e0c },
 { 0x237a, 0x4f0d },
 { 0x237c, 0x4e82 },
 { 0x237e, 0x8338 },
 { 0x2380, 0x4f82 },
 { 0x2382, 0x833a },
 { 0x2384, 0x421f },
 { 0x2386, 0x0196 },
 { 0x2388, 0x4f0e },
 { 0x238a, 0x430f },
 { 0x238c, 0x4c0a },
 { 0x238e, 0x4d0b },
 { 0x2390, 0x8e0a },
 { 0x2392, 0x7f0b },
 { 0x2394, 0x4a0e },
 { 0x2396, 0x4b0f },
 { 0x2398, 0x803e },
 { 0x239a, 0x0102 },
 { 0x239c, 0x730f },
 { 0x239e, 0x281b },
 { 0x23a0, 0x4039 },
 { 0x23a2, 0x0101 },
 { 0x23a4, 0x8329 },
 { 0x23a6, 0x4982 },
 { 0x23a8, 0x7320 },
 { 0x23aa, 0x421e },
 { 0x23ac, 0x7540 },
 { 0x23ae, 0x421f },
 { 0x23b0, 0x7574 },
 { 0x23b2, 0x12b0 },
 { 0x23b4, 0xefc4 },
 { 0x23b6, 0x4e82 },
 { 0x23b8, 0x8332 },
 { 0x23ba, 0x4f82 },
 { 0x23bc, 0x8334 },
 { 0x23be, 0x4348 },
 { 0x23c0, 0x5039 },
 { 0x23c2, 0xfffa },
 { 0x23c4, 0x490c },
 { 0x23c6, 0x430d },
 { 0x23c8, 0x5e0c },
 { 0x23ca, 0x6f0d },
 { 0x23cc, 0x8a0c },
 { 0x23ce, 0x7b0d },
 { 0x23d0, 0x2804 },
 { 0x23d2, 0x4358 },
 { 0x23d4, 0x3c02 },
 { 0x23d6, 0x4a09 },
 { 0x23d8, 0x3fe5 },
 { 0x23da, 0x48c2 },
 { 0x23dc, 0x834b },
 { 0x23de, 0x4138 },
 { 0x23e0, 0x4139 },
 { 0x23e2, 0x413a },
 { 0x23e4, 0x413b },
 { 0x23e6, 0x4130 },
 { 0x23e8, 0x120b },
 { 0x23ea, 0x120a },
 { 0x23ec, 0x93c2 },
 { 0x23ee, 0x8294 },
 { 0x23f0, 0x2021 },
 { 0x23f2, 0x93c2 },
 { 0x23f4, 0x8310 },
 { 0x23f6, 0x2404 },
 { 0x23f8, 0x43b2 },
 { 0x23fa, 0x7574 },
 { 0x23fc, 0x43b2 },
 { 0x23fe, 0x7540 },
 { 0x2400, 0x93c2 },
 { 0x2402, 0x834b },
 { 0x2404, 0x2417 },
 { 0x2406, 0x421e },
 { 0x2408, 0x8332 },
 { 0x240a, 0x421f },
 { 0x240c, 0x8334 },
 { 0x240e, 0x532e },
 { 0x2410, 0x630f },
 { 0x2412, 0x4e82 },
 { 0x2414, 0x8332 },
 { 0x2416, 0x4f82 },
 { 0x2418, 0x8334 },
 { 0x241a, 0x421a },
 { 0x241c, 0x8338 },
 { 0x241e, 0x421b },
 { 0x2420, 0x833a },
 { 0x2422, 0x4a0c },
 { 0x2424, 0x4b0d },
 { 0x2426, 0x8e0c },
 { 0x2428, 0x7f0d },
 { 0x242a, 0x2c04 },
 { 0x242c, 0x4a82 },
 { 0x242e, 0x8332 },
 { 0x2430, 0x4b82 },
 { 0x2432, 0x8334 },
 { 0x2434, 0x413a },
 { 0x2436, 0x413b },
 { 0x2438, 0x4130 },
 { 0x243a, 0x93c2 },
 { 0x243c, 0x00c3 },
 { 0x243e, 0x240e },
 { 0x2440, 0x93c2 },
 { 0x2442, 0x00c1 },
 { 0x2444, 0x200b },
 { 0x2446, 0x43d2 },
 { 0x2448, 0x834a },
 { 0x244a, 0x93d2 },
 { 0x244c, 0x00bf },
 { 0x244e, 0x2403 },
 { 0x2450, 0x43c2 },
 { 0x2452, 0x834c },
 { 0x2454, 0x4130 },
 { 0x2456, 0x43d2 },
 { 0x2458, 0x834c },
 { 0x245a, 0x4130 },
 { 0x245c, 0x43c2 },
 { 0x245e, 0x834a },
 { 0x2460, 0x3ff4 },
 { 0x2462, 0x93c2 },
 { 0x2464, 0x834c },
 { 0x2466, 0x2408 },
 { 0x2468, 0xb3e2 },
 { 0x246a, 0x00c2 },
 { 0x246c, 0x2403 },
 { 0x246e, 0x43d2 },
 { 0x2470, 0x00c8 },
 { 0x2472, 0x4130 },
 { 0x2474, 0x43c2 },
 { 0x2476, 0x00c8 },
 { 0x2478, 0x4130 },
 { 0x247a, 0x93c2 },
 { 0x247c, 0x834a },
 { 0x247e, 0x2406 },
 { 0x2480, 0x403f },
 { 0x2482, 0x00c3 },
 { 0x2484, 0x4fe2 },
 { 0x2486, 0x8330 },
 { 0x2488, 0xc3df },
 { 0x248a, 0x0000 },
 { 0x248c, 0x4130 },
 { 0x248e, 0x93c2 },
 { 0x2490, 0x834a },
 { 0x2492, 0x2403 },
 { 0x2494, 0x42d2 },
 { 0x2496, 0x8330 },
 { 0x2498, 0x00c3 },
 { 0x249a, 0x421e },
 { 0x249c, 0x7314 },
 { 0x249e, 0x421f },
 { 0x24a0, 0x7336 },
 { 0x24a2, 0x12b0 },
 { 0x24a4, 0xefc4 },
 { 0x24a6, 0x4e82 },
 { 0x24a8, 0x831c },
 { 0x24aa, 0x4f82 },
 { 0x24ac, 0x831e },
 { 0x24ae, 0x4130 },
 { 0x24b0, 0x93c2 },
 { 0x24b2, 0x834c },
 { 0x24b4, 0x2408 },
 { 0x24b6, 0xb3e2 },
 { 0x24b8, 0x00c2 },
 { 0x24ba, 0x2403 },
 { 0x24bc, 0x43c2 },
 { 0x24be, 0x00c8 },
 { 0x24c0, 0x4130 },
 { 0x24c2, 0x43d2 },
 { 0x24c4, 0x00c8 },
 { 0x24c6, 0x4130 },
 { 0x24c8, 0x93c2 },
 { 0x24ca, 0x834c },
 { 0x24cc, 0x2408 },
 { 0x24ce, 0xb3e2 },
 { 0x24d0, 0x00c2 },
 { 0x24d2, 0x2403 },
 { 0x24d4, 0x43d2 },
 { 0x24d6, 0x00c8 },
 { 0x24d8, 0x3c02 },
 { 0x24da, 0x43c2 },
 { 0x24dc, 0x00c8 },
 { 0x24de, 0x43c2 },
 { 0x24e0, 0x8294 },
 { 0x24e2, 0x4130 },
 { 0x24e4, 0x43c2 },
 { 0x24e6, 0x834b },
 { 0x24e8, 0x4130 },
 { 0x24ea, 0x403e },
 { 0x24ec, 0x00c2 },
 { 0x24ee, 0x421f },
 { 0x24f0, 0x7312 },
 { 0x24f2, 0xf07f },
 { 0x24f4, 0x000c },
 { 0x24f6, 0x5f4f },
 { 0x24f8, 0x5f4f },
 { 0x24fa, 0xdfce },
 { 0x24fc, 0x0000 },
 { 0x24fe, 0xf0fe },
 { 0x2500, 0x000f },
 { 0x2502, 0x0000 },
 { 0x2504, 0x4130 },
 { 0x2506, 0x4f82 },
 { 0x2508, 0x7334 },
 { 0x250a, 0x0f00 },
 { 0x250c, 0x7300 },
 { 0x250e, 0x4130 },
 { 0x2510, 0x421f },
 { 0x2512, 0x0196 },
 { 0x2514, 0x503f },
 { 0x2516, 0x0006 },
 { 0x2518, 0x4f82 },
 { 0x251a, 0x832e },
 { 0x251c, 0x93c2 },
 { 0x251e, 0x00c1 },
 { 0x2520, 0x242d },
 { 0x2522, 0x425e },
 { 0x2524, 0x00c2 },
 { 0x2526, 0xc35e },
 { 0x2528, 0x425f },
 { 0x252a, 0x8310 },
 { 0x252c, 0xdf4e },
 { 0x252e, 0x4ec2 },
 { 0x2530, 0x00c2 },
 { 0x2532, 0x934f },
 { 0x2534, 0x2006 },
 { 0x2536, 0x9382 },
 { 0x2538, 0x730a },
 { 0x253a, 0x27fd },
 { 0x253c, 0xd3d2 },
 { 0x253e, 0x00c2 },
 { 0x2540, 0x3c1f },
 { 0x2542, 0x12b0 },
 { 0x2544, 0xfd86 },
 { 0x2546, 0x12b0 },
 { 0x2548, 0xfdbc },
 { 0x254a, 0x421f },
 { 0x254c, 0x8342 },
 { 0x254e, 0x933f },
 { 0x2550, 0x380a },
 { 0x2552, 0x932f },
 { 0x2554, 0x3803 },
 { 0x2556, 0x12b0 },
 { 0x2558, 0xff36 },
 { 0x255a, 0x3ff0 },
 { 0x255c, 0x12b0 },
 { 0x255e, 0xff52 },
 { 0x2560, 0x12b0 },
 { 0x2562, 0xff9c },
 { 0x2564, 0x3feb },
 { 0x2566, 0x12b0 },
 { 0x2568, 0xfe06 },
 { 0x256a, 0x12b0 },
 { 0x256c, 0xfe36 },
 { 0x256e, 0x12b0 },
 { 0x2570, 0xfe68 },
 { 0x2572, 0x12b0 },
 { 0x2574, 0xfec0 },
 { 0x2576, 0x12b0 },
 { 0x2578, 0xfefe },
 { 0x257a, 0x3fe0 },
 { 0x257c, 0x0900 },
 { 0x257e, 0x7328 },
 { 0x2580, 0x12b0 },
 { 0x2582, 0xffc2 },
 { 0x2584, 0x4130 },
 { 0x2586, 0x421e },
 { 0x2588, 0x7314 },
 { 0x258a, 0x421f },
 { 0x258c, 0x7336 },
 { 0x258e, 0x12b0 },
 { 0x2590, 0xefc4 },
 { 0x2592, 0x4e82 },
 { 0x2594, 0x832a },
 { 0x2596, 0x4f82 },
 { 0x2598, 0x832c },
 { 0x259a, 0xb3e2 },
 { 0x259c, 0x00c2 },
 { 0x259e, 0x2407 },
 { 0x25a0, 0x9382 },
 { 0x25a2, 0x7318 },
 { 0x25a4, 0x27fd },
 { 0x25a6, 0x9392 },
 { 0x25a8, 0x7318 },
 { 0x25aa, 0x27fd },
 { 0x25ac, 0x4130 },
 { 0x25ae, 0x9392 },
 { 0x25b0, 0x7318 },
 { 0x25b2, 0x27fd },
 { 0x25b4, 0x9382 },
 { 0x25b6, 0x7318 },
 { 0x25b8, 0x27fd },
 { 0x25ba, 0x4130 },
 { 0x25bc, 0x120b },
 { 0x25be, 0x120a },
 { 0x25c0, 0x421e },
 { 0x25c2, 0x7300 },
 { 0x25c4, 0x421f },
 { 0x25c6, 0x733c },
 { 0x25c8, 0x12b0 },
 { 0x25ca, 0xefc4 },
 { 0x25cc, 0x4e82 },
 { 0x25ce, 0x8324 },
 { 0x25d0, 0x4f82 },
 { 0x25d2, 0x8326 },
 { 0x25d4, 0x421c },
 { 0x25d6, 0x8344 },
 { 0x25d8, 0x430d },
 { 0x25da, 0x421a },
 { 0x25dc, 0x831c },
 { 0x25de, 0x421b },
 { 0x25e0, 0x831e },
 { 0x25e2, 0x8c0a },
 { 0x25e4, 0x7d0b },
 { 0x25e6, 0x8a0e },
 { 0x25e8, 0x7b0f },
 { 0x25ea, 0x2c04 },
 { 0x25ec, 0x4292 },
 { 0x25ee, 0x8324 },
 { 0x25f0, 0x8342 },
 { 0x25f2, 0x3c06 },
 { 0x25f4, 0x421b },
 { 0x25f6, 0x8324 },
 { 0x25f8, 0x821b },
 { 0x25fa, 0x831c },
 { 0x25fc, 0x4b82 },
 { 0x25fe, 0x8342 },
 { 0x2600, 0x413a },
 { 0x2602, 0x413b },
 { 0x2604, 0x4130 },
 { 0x2606, 0x4382 },
 { 0x2608, 0x8336 },
 { 0x260a, 0x50b2 },
 { 0x260c, 0xfffd },
 { 0x260e, 0x8342 },
 { 0x2610, 0x4382 },
 { 0x2612, 0x7336 },
 { 0x2614, 0x43a2 },
 { 0x2616, 0x7314 },
 { 0x2618, 0x403e },
 { 0x261a, 0x7542 },
 { 0x261c, 0x4ea2 },
 { 0x261e, 0x833e },
 { 0x2620, 0x403f },
 { 0x2622, 0x7544 },
 { 0x2624, 0x4fa2 },
 { 0x2626, 0x8340 },
 { 0x2628, 0x429e },
 { 0x262a, 0x8320 },
 { 0x262c, 0x0000 },
 { 0x262e, 0x429f },
 { 0x2630, 0x8322 },
 { 0x2632, 0x0000 },
 { 0x2634, 0x4130 },
 { 0x2636, 0x421e },
 { 0x2638, 0x8324 },
 { 0x263a, 0x421f },
 { 0x263c, 0x8326 },
 { 0x263e, 0x821e },
 { 0x2640, 0x8346 },
 { 0x2642, 0x721f },
 { 0x2644, 0x8348 },
 { 0x2646, 0x2c0a },
 { 0x2648, 0x4382 },
 { 0x264a, 0x7574 },
 { 0x264c, 0x4382 },
 { 0x264e, 0x7540 },
 { 0x2650, 0x421f },
 { 0x2652, 0x831c },
 { 0x2654, 0x821f },
 { 0x2656, 0x8346 },
 { 0x2658, 0x4f82 },
 { 0x265a, 0x8336 },
 { 0x265c, 0x12b0 },
 { 0x265e, 0xfcea },
 { 0x2660, 0x0b00 },
 { 0x2662, 0x7302 },
 { 0x2664, 0x0578 },
 { 0x2666, 0x4130 },
 { 0x2668, 0x4392 },
 { 0x266a, 0x7318 },
 { 0x266c, 0x4292 },
 { 0x266e, 0x833e },
 { 0x2670, 0x7542 },
 { 0x2672, 0x4292 },
 { 0x2674, 0x8340 },
 { 0x2676, 0x7544 },
 { 0x2678, 0x421e },
 { 0x267a, 0x8342 },
 { 0x267c, 0x4e0f },
 { 0x267e, 0x5f0f },
 { 0x2680, 0x7f0f },
 { 0x2682, 0xe33f },
 { 0x2684, 0x521e },
 { 0x2686, 0x8332 },
 { 0x2688, 0x621f },
 { 0x268a, 0x8334 },
 { 0x268c, 0x4e82 },
 { 0x268e, 0x8332 },
 { 0x2690, 0x4f82 },
 { 0x2692, 0x8334 },
 { 0x2694, 0x421d },
 { 0x2696, 0x8336 },
 { 0x2698, 0x522d },
 { 0x269a, 0x4d0c },
 { 0x269c, 0x4c0d },
 { 0x269e, 0x5d0d },
 { 0x26a0, 0x7d0d },
 { 0x26a2, 0xe33d },
 { 0x26a4, 0x8c0e },
 { 0x26a6, 0x7d0f },
 { 0x26a8, 0x2c04 },
 { 0x26aa, 0x4c82 },
 { 0x26ac, 0x8332 },
 { 0x26ae, 0x4d82 },
 { 0x26b0, 0x8334 },
 { 0x26b2, 0x4292 },
 { 0x26b4, 0x8334 },
 { 0x26b6, 0x7574 },
 { 0x26b8, 0x4292 },
 { 0x26ba, 0x8332 },
 { 0x26bc, 0x7540 },
 { 0x26be, 0x4130 },
 { 0x26c0, 0x421e },
 { 0x26c2, 0x8342 },
 { 0x26c4, 0x4e0f },
 { 0x26c6, 0x5f0f },
 { 0x26c8, 0x7f0f },
 { 0x26ca, 0xe33f },
 { 0x26cc, 0x521e },
 { 0x26ce, 0x832a },
 { 0x26d0, 0x621f },
 { 0x26d2, 0x832c },
 { 0x26d4, 0x4e82 },
 { 0x26d6, 0x832a },
 { 0x26d8, 0x4f82 },
 { 0x26da, 0x832c },
 { 0x26dc, 0x421c },
 { 0x26de, 0x832e },
 { 0x26e0, 0x430d },
 { 0x26e2, 0x8c0e },
 { 0x26e4, 0x7d0f },
 { 0x26e6, 0x2c04 },
 { 0x26e8, 0x4c82 },
 { 0x26ea, 0x832a },
 { 0x26ec, 0x4d82 },
 { 0x26ee, 0x832c },
 { 0x26f0, 0x4292 },
 { 0x26f2, 0x832c },
 { 0x26f4, 0x7336 },
 { 0x26f6, 0x4292 },
 { 0x26f8, 0x832a },
 { 0x26fa, 0x7314 },
 { 0x26fc, 0x4130 },
 { 0x26fe, 0x4292 },
 { 0x2700, 0x7320 },
 { 0x2702, 0x8328 },
 { 0x2704, 0x421e },
 { 0x2706, 0x8336 },
 { 0x2708, 0x4e0f },
 { 0x270a, 0x821f },
 { 0x270c, 0x8342 },
 { 0x270e, 0x4f82 },
 { 0x2710, 0x833c },
 { 0x2712, 0x421d },
 { 0x2714, 0x8328 },
 { 0x2716, 0x8f0d },
 { 0x2718, 0x4d82 },
 { 0x271a, 0x8328 },
 { 0x271c, 0x922d },
 { 0x271e, 0x3402 },
 { 0x2720, 0x42a2 },
 { 0x2722, 0x8328 },
 { 0x2724, 0x4292 },
 { 0x2726, 0x8328 },
 { 0x2728, 0x7320 },
 { 0x272a, 0x931e },
 { 0x272c, 0x3803 },
 { 0x272e, 0x4e0f },
 { 0x2730, 0x12b0 },
 { 0x2732, 0xfd06 },
 { 0x2734, 0x4130 },
 { 0x2736, 0x0b00 },
 { 0x2738, 0x7302 },
 { 0x273a, 0x0578 },
 { 0x273c, 0x4392 },
 { 0x273e, 0x7318 },
 { 0x2740, 0x4292 },
 { 0x2742, 0x8334 },
 { 0x2744, 0x7574 },
 { 0x2746, 0x4292 },
 { 0x2748, 0x8332 },
 { 0x274a, 0x7540 },
 { 0x274c, 0x12b0 },
 { 0x274e, 0xfcea },
 { 0x2750, 0x4130 },
 { 0x2752, 0x120b },
 { 0x2754, 0x120a },
 { 0x2756, 0x403a },
 { 0x2758, 0x7314 },
 { 0x275a, 0x403b },
 { 0x275c, 0x7336 },
 { 0x275e, 0x4a2e },
 { 0x2760, 0x4b2f },
 { 0x2762, 0x12b0 },
 { 0x2764, 0xefc4 },
 { 0x2766, 0x4e0c },
 { 0x2768, 0x4f0d },
 { 0x276a, 0x421e },
 { 0x276c, 0x8342 },
 { 0x276e, 0x4e0f },
 { 0x2770, 0x5f0f },
 { 0x2772, 0x7f0f },
 { 0x2774, 0xe33f },
 { 0x2776, 0x5e0c },
 { 0x2778, 0x6f0d },
 { 0x277a, 0x4d8b },
 { 0x277c, 0x0000 },
 { 0x277e, 0x4c8a },
 { 0x2780, 0x0000 },
 { 0x2782, 0x421c },
 { 0x2784, 0x8332 },
 { 0x2786, 0x421d },
 { 0x2788, 0x8334 },
 { 0x278a, 0x5e0c },
 { 0x278c, 0x6f0d },
 { 0x278e, 0x4d82 },
 { 0x2790, 0x7574 },
 { 0x2792, 0x4c82 },
 { 0x2794, 0x7540 },
 { 0x2796, 0x413a },
 { 0x2798, 0x413b },
 { 0x279a, 0x4130 },
 { 0x279c, 0x421e },
 { 0x279e, 0x7300 },
 { 0x27a0, 0x421f },
 { 0x27a2, 0x733c },
 { 0x27a4, 0x12b0 },
 { 0x27a6, 0xefc4 },
 { 0x27a8, 0x803e },
 { 0x27aa, 0x0003 },
 { 0x27ac, 0x730f },
 { 0x27ae, 0x2ff6 },
 { 0x27b0, 0x12b0 },
 { 0x27b2, 0xfcea },
 { 0x27b4, 0x0b00 },
 { 0x27b6, 0x7300 },
 { 0x27b8, 0x0002 },
 { 0x27ba, 0x0b00 },
 { 0x27bc, 0x7302 },
 { 0x27be, 0x0320 },
 { 0x27c0, 0x4130 },
 { 0x27c2, 0x4292 },
 { 0x27c4, 0x7542 },
 { 0x27c6, 0x8320 },
 { 0x27c8, 0x4292 },
 { 0x27ca, 0x7544 },
 { 0x27cc, 0x8322 },
 { 0x27ce, 0x4130 },
 { 0x27fe, 0xe000 },
 { 0x3000, 0x60f8 },
 { 0x3002, 0x187f },
 { 0x3004, 0x7060 },
 { 0x3006, 0x0114 },
 { 0x3008, 0x60b0 },
 { 0x300a, 0x1473 },
 { 0x300c, 0x0013 },
 { 0x300e, 0x140f },
 { 0x3010, 0x0040 },
 { 0x3012, 0x100f },
 { 0x3014, 0x60f8 },
 { 0x3016, 0x187f },
 { 0x3018, 0x7060 },
 { 0x301a, 0x0114 },
 { 0x301c, 0x60b0 },
 { 0x301e, 0x1473 },
 { 0x3020, 0x0013 },
 { 0x3022, 0x140f },
 { 0x3024, 0x0040 },
 { 0x3026, 0x000f },
 { 0x0b00, 0x0000 },
 { 0x0b02, 0x0045 },
 { 0x0b04, 0xb405 },
 { 0x0b06, 0xc403 },
 { 0x0b08, 0x0081 },
 { 0x0b0a, 0x8252 },
 { 0x0b0c, 0xf814 },
 { 0x0b0e, 0xc618 },
 { 0x0b10, 0xa828 },
 { 0x0b12, 0x002c },
 { 0x0b14, 0x4068 },
 { 0x0b16, 0x0000 },
 { 0x0f30, 0x6e25 },
 { 0x0f32, 0x7067 },
 { 0x0954, 0x0009 },
 { 0x0956, 0x0000 },
 { 0x0958, 0xbb80 },
 { 0x095a, 0x5140 },
 { 0x0c00, 0x1110 },
 { 0x0c02, 0x0011 },
 { 0x0c04, 0x0000 },
 { 0x0c06, 0x0200 },
 { 0x0c10, 0x0040 },
 { 0x0c12, 0x0040 },
 { 0x0c14, 0x0040 },
 { 0x0c16, 0x0040 },
 { 0x0a10, 0x4000 },
 { 0x3068, 0xffff },
 { 0x306a, 0xffff },
 { 0x006c, 0x0300 },
 { 0x005e, 0x0200 },
 { 0x000e, 0x0100 },
 { 0x0e0a, 0x0001 },
 { 0x004a, 0x0100 },
 { 0x004c, 0x0000 },
 { 0x000c, 0x0022 },
 { 0x0008, 0x0b00 },
 { 0x005a, 0x0202 },
 { 0x0012, 0x000e },
 { 0x0018, 0x0a31 },
 { 0x0022, 0x0008 },
 { 0x0028, 0x0017 },
 { 0x0024, 0x0028 },
 { 0x002a, 0x002d },
 { 0x0026, 0x0030 },
 { 0x002c, 0x07c7 },
 { 0x002e, 0x1111 },
 { 0x0030, 0x1111 },
 { 0x0032, 0x1111 },
 { 0x0006, 0x0823 },
 { 0x0116, 0x07b6 },
 { 0x0a22, 0x0000 },
 { 0x0a12, 0x0a20 },
 { 0x0a14, 0x0798 },
 { 0x003e, 0x0000 },
 { 0x0074, 0x080e },
 { 0x0070, 0x0407 },
 { 0x0002, 0x0000 },
 { 0x0a02, 0x0000 },
 { 0x0a24, 0x0100 },
 { 0x0046, 0x0000 },
 { 0x0076, 0x0000 },
 { 0x0060, 0x0000 },
 { 0x0062, 0x0530 },
 { 0x0064, 0x0500 },
 { 0x0066, 0x0530 },
 { 0x0068, 0x0500 },
 { 0x0122, 0x0300 },
 { 0x015a, 0xff08 },
 { 0x0804, 0x0200 },
 { 0x005c, 0x0100 },
 { 0x0a1a, 0x0800 },
 { 0x004c, 0x0000 },
 { 0x004e, 0x0100 },
 { 0x0040, 0x0000 },
 { 0x0042, 0x0100 },
 { 0x003e, 0x0000 },
};

static const struct hi556_reg mode_2592x1944_regs[] = {
 { 0x0b0a, 0x8252 },
 { 0x0f30, 0x6e25 },
 { 0x0f32, 0x7067 },
 { 0x004a, 0x0100 },
 { 0x004c, 0x0000 },
 { 0x000c, 0x0022 },
 { 0x0008, 0x0b00 },
 { 0x005a, 0x0202 },
 { 0x0012, 0x000e },
 { 0x0018, 0x0a31 },
 { 0x0022, 0x0008 },
 { 0x0028, 0x0017 },
 { 0x0024, 0x0028 },
 { 0x002a, 0x002d },
 { 0x0026, 0x0030 },
 { 0x002c, 0x07c7 },
 { 0x002e, 0x1111 },
 { 0x0030, 0x1111 },
 { 0x0032, 0x1111 },
 { 0x0006, 0x0823 },
 { 0x0116, 0x07b6 },
 { 0x0a22, 0x0000 },
 { 0x0a12, 0x0a20 },
 { 0x0a14, 0x0798 },
 { 0x003e, 0x0000 },
 { 0x0074, 0x0821 },
 { 0x0070, 0x0411 },
 { 0x0804, 0x0200 },
 { 0x0a04, 0x014a },
 { 0x090c, 0x0fdc },
 { 0x090e, 0x002d },
 { 0x0040, 0x0000 },
 { 0x0042, 0x0100 },
 { 0x003e, 0x0000 },
 { 0x0126, 0x00f9 },
 { 0x0902, 0x4319 },
 { 0x0914, 0xc10a },
 { 0x0916, 0x071f },
 { 0x0918, 0x0408 },
 { 0x091a, 0x0c0d },
 { 0x091c, 0x0f09 },
 { 0x091e, 0x0a00 },
 { 0x0958, 0xbb80 },
};




static const char * const hi556_test_pattern_menu[] = {
	"Disabled",
	"Solid Colour",
	"100% Colour Bars",
	"Fade To Grey Colour Bars",
	"PN9",
	"Gradient Horizontal",
	"Gradient Vertical",
	"Check Board",
	"Slant Pattern",
};

static const s64 link_freq_menu_items[] = {
	HI556_LINK_FREQ_437MHZ,
};

static const struct hi556_link_freq_config link_freq_configs[] = {
	[HI556_LINK_FREQ_437MHZ_INDEX] = {
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mipi_data_rate_874mbps),
			.regs = mipi_data_rate_874mbps,
		}
	}
};

static const struct hi556_mode supported_modes[] = {
	{
		.width = HI556_PIXEL_ARRAY_WIDTH,
		.height = HI556_PIXEL_ARRAY_HEIGHT,
		.crop = {
			.left = HI556_PIXEL_ARRAY_LEFT,
			.top = HI556_PIXEL_ARRAY_TOP,
			.width = HI556_PIXEL_ARRAY_WIDTH,
			.height = HI556_PIXEL_ARRAY_HEIGHT
		},
		.fll_def = HI556_FLL_30FPS,
		.fll_min = HI556_FLL_30FPS_MIN,
		.llp = 0x0b00,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2592x1944_regs),
			.regs = mode_2592x1944_regs,
		},
		.link_freq_index = HI556_LINK_FREQ_437MHZ_INDEX,
	},
};

static const char * const hi556_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
	"dvdd",		/* Digital core power */
};

struct hi556 {
	struct device *dev;

	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;

	/* GPIOs, clocks, etc. */
	struct gpio_desc *reset_gpio;
	struct clk *clk;
	struct regulator_bulk_data supplies[ARRAY_SIZE(hi556_supply_names)];

	/* Current mode */
	const struct hi556_mode *cur_mode;

	/* To serialize asynchronous callbacks */
	struct mutex mutex;

	/* True if the device has been identified */
	bool identified;
};

static u64 to_pixel_rate(u32 f_index)
{
	u64 pixel_rate = link_freq_menu_items[f_index] * 2 * HI556_DATA_LANES;

	do_div(pixel_rate, HI556_RGB_DEPTH);

	return pixel_rate;
}

static int hi556_read_reg(struct hi556 *hi556, u16 reg, u16 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hi556->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[2];
	u8 data_buf[4] = {0};
	int ret;

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, addr_buf);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(addr_buf);
	msgs[0].buf = addr_buf;
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

static int hi556_write_reg(struct hi556 *hi556, u16 reg, u16 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hi556->sd);
	u8 buf[6];

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << 8 * (4 - len), buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int hi556_write_reg_list(struct hi556 *hi556,
				const struct hi556_reg_list *r_list)
{
	unsigned int i;
	int ret;

	for (i = 0; i < r_list->num_of_regs; i++) {
		ret = hi556_write_reg(hi556, r_list->regs[i].address,
				      HI556_REG_VALUE_16BIT,
				      r_list->regs[i].val);
		if (ret) {
			dev_err_ratelimited(hi556->dev,
					    "failed to write reg 0x%4.4x. error = %d\n",
					    r_list->regs[i].address, ret);
			return ret;
		}
	}

	return 0;
}

static int hi556_update_digital_gain(struct hi556 *hi556, u32 d_gain)
{
	int ret;

	ret = hi556_write_reg(hi556, HI556_REG_MWB_GR_GAIN,
			      HI556_REG_VALUE_16BIT, d_gain);
	if (ret)
		return ret;

	ret = hi556_write_reg(hi556, HI556_REG_MWB_GB_GAIN,
			      HI556_REG_VALUE_16BIT, d_gain);
	if (ret)
		return ret;

	ret = hi556_write_reg(hi556, HI556_REG_MWB_R_GAIN,
			      HI556_REG_VALUE_16BIT, d_gain);
	if (ret)
		return ret;

	return hi556_write_reg(hi556, HI556_REG_MWB_B_GAIN,
			       HI556_REG_VALUE_16BIT, d_gain);
}

static int hi556_test_pattern(struct hi556 *hi556, u32 pattern)
{
	int ret;
	u32 val;

	ret = hi556_read_reg(hi556, HI556_REG_ISP,
			     HI556_REG_VALUE_08BIT, &val);
	if (ret)
		return ret;

	val = pattern ? (val | HI556_REG_ISP_TPG_EN) :
		(val & ~HI556_REG_ISP_TPG_EN);

	ret = hi556_write_reg(hi556, HI556_REG_ISP,
			      HI556_REG_VALUE_08BIT, val);
	if (ret)
		return ret;

	val = pattern ? BIT(pattern - 1) : 0;

	return hi556_write_reg(hi556, HI556_REG_TEST_PATTERN,
			       HI556_REG_VALUE_08BIT, val);
}

static int hi556_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct hi556 *hi556 = container_of(ctrl->handler,
					     struct hi556, ctrl_handler);
	s64 exposure_max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	if (ctrl->id == V4L2_CID_VBLANK) {
		/* Update max exposure while meeting expected vblanking */
		exposure_max = hi556->cur_mode->height + ctrl->val -
			       HI556_EXPOSURE_MAX_MARGIN;
		__v4l2_ctrl_modify_range(hi556->exposure,
					 hi556->exposure->minimum,
					 exposure_max, hi556->exposure->step,
					 exposure_max);
	}

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(hi556->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = hi556_write_reg(hi556, HI556_REG_ANALOG_GAIN,
				      HI556_REG_VALUE_16BIT, ctrl->val);
		break;

	case V4L2_CID_DIGITAL_GAIN:
		ret = hi556_update_digital_gain(hi556, ctrl->val);
		break;

	case V4L2_CID_EXPOSURE:
		ret = hi556_write_reg(hi556, HI556_REG_EXPOSURE,
				      HI556_REG_VALUE_16BIT, ctrl->val);
		break;

	case V4L2_CID_VBLANK:
		/* Update FLL that meets expected vertical blanking */
		ret = hi556_write_reg(hi556, HI556_REG_FLL,
				      HI556_REG_VALUE_16BIT,
				      hi556->cur_mode->height + ctrl->val);
		break;

	case V4L2_CID_TEST_PATTERN:
		ret = hi556_test_pattern(hi556, ctrl->val);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(hi556->dev);

	return ret;
}

static const struct v4l2_ctrl_ops hi556_ctrl_ops = {
	.s_ctrl = hi556_set_ctrl,
};

static int hi556_init_controls(struct hi556 *hi556)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	s64 exposure_max, h_blank;
	int ret;

	ctrl_hdlr = &hi556->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	ctrl_hdlr->lock = &hi556->mutex;
	hi556->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr, &hi556_ctrl_ops,
						  V4L2_CID_LINK_FREQ,
					ARRAY_SIZE(link_freq_menu_items) - 1,
					0, link_freq_menu_items);
	if (hi556->link_freq)
		hi556->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	hi556->pixel_rate = v4l2_ctrl_new_std
			    (ctrl_hdlr, &hi556_ctrl_ops,
			     V4L2_CID_PIXEL_RATE, 0,
			     to_pixel_rate(HI556_LINK_FREQ_437MHZ_INDEX),
			     1,
			     to_pixel_rate(HI556_LINK_FREQ_437MHZ_INDEX));
	hi556->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &hi556_ctrl_ops,
					  V4L2_CID_VBLANK,
					  hi556->cur_mode->fll_min -
					  hi556->cur_mode->height,
					  HI556_FLL_MAX -
					  hi556->cur_mode->height, 1,
					  hi556->cur_mode->fll_def -
					  hi556->cur_mode->height);

	h_blank = hi556->cur_mode->llp - hi556->cur_mode->width;

	hi556->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &hi556_ctrl_ops,
					  V4L2_CID_HBLANK, h_blank, h_blank, 1,
					  h_blank);
	if (hi556->hblank)
		hi556->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(ctrl_hdlr, &hi556_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  HI556_ANAL_GAIN_MIN, HI556_ANAL_GAIN_MAX,
			  HI556_ANAL_GAIN_STEP, HI556_ANAL_GAIN_MIN);
	v4l2_ctrl_new_std(ctrl_hdlr, &hi556_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  HI556_DGTL_GAIN_MIN, HI556_DGTL_GAIN_MAX,
			  HI556_DGTL_GAIN_STEP, HI556_DGTL_GAIN_DEFAULT);
	exposure_max = hi556->cur_mode->fll_def - HI556_EXPOSURE_MAX_MARGIN;
	hi556->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &hi556_ctrl_ops,
					    V4L2_CID_EXPOSURE,
					    HI556_EXPOSURE_MIN, exposure_max,
					    HI556_EXPOSURE_STEP,
					    exposure_max);
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &hi556_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(hi556_test_pattern_menu) - 1,
				     0, 0, hi556_test_pattern_menu);
	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	hi556->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

static void hi556_assign_pad_format(const struct hi556_mode *mode,
				    struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->field = V4L2_FIELD_NONE;
}

static int hi556_identify_module(struct hi556 *hi556)
{
	int ret;
	u32 val;

	if (hi556->identified)
		return 0;

	ret = hi556_read_reg(hi556, HI556_REG_CHIP_ID,
			     HI556_REG_VALUE_16BIT, &val);
	if (ret)
		return ret;

	if (val != HI556_CHIP_ID) {
		dev_err(hi556->dev, "chip id mismatch: %x!=%x\n",
			HI556_CHIP_ID, val);
		return -ENXIO;
	}

	hi556->identified = true;

	return 0;
}

static const struct v4l2_rect *
__hi556_get_pad_crop(struct hi556 *hi556,
		     struct v4l2_subdev_state *sd_state,
		     unsigned int pad, enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_state_get_crop(sd_state, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &hi556->cur_mode->crop;
	}

	return NULL;
}

static int hi556_get_selection(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP: {
		struct hi556 *hi556 = to_hi556(sd);

		mutex_lock(&hi556->mutex);
		sel->r = *__hi556_get_pad_crop(hi556, sd_state, sel->pad,
						sel->which);
		mutex_unlock(&hi556->mutex);

		return 0;
	}

	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = HI556_NATIVE_WIDTH;
		sel->r.height = HI556_NATIVE_HEIGHT;

		return 0;

	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = HI556_PIXEL_ARRAY_TOP;
		sel->r.left = HI556_PIXEL_ARRAY_LEFT;
		sel->r.width = HI556_PIXEL_ARRAY_WIDTH;
		sel->r.height = HI556_PIXEL_ARRAY_HEIGHT;

		return 0;
	}

	return -EINVAL;
}

static int hi556_start_streaming(struct hi556 *hi556)
{
	const struct hi556_reg_list *reg_list;
	int link_freq_index, ret;

	ret = hi556_identify_module(hi556);
	if (ret)
		return ret;

	link_freq_index = hi556->cur_mode->link_freq_index;
	reg_list = &link_freq_configs[link_freq_index].reg_list;
	ret = hi556_write_reg_list(hi556, reg_list);
	if (ret) {
		dev_err(hi556->dev, "failed to set plls\n");
		return ret;
	}

	reg_list = &hi556->cur_mode->reg_list;
	ret = hi556_write_reg_list(hi556, reg_list);
	if (ret) {
		dev_err(hi556->dev, "failed to set mode\n");
		return ret;
	}

	ret = __v4l2_ctrl_handler_setup(hi556->sd.ctrl_handler);
	if (ret)
		return ret;

	ret = hi556_write_reg(hi556, HI556_REG_MODE_SELECT,
			      HI556_REG_VALUE_16BIT, HI556_MODE_STREAMING);

	if (ret) {
		dev_err(hi556->dev, "failed to set stream\n");
		return ret;
	}

	return 0;
}

static void hi556_stop_streaming(struct hi556 *hi556)
{
	if (hi556_write_reg(hi556, HI556_REG_MODE_SELECT,
			    HI556_REG_VALUE_16BIT, HI556_MODE_STANDBY))
		dev_err(hi556->dev, "failed to set stream\n");
}

static int hi556_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct hi556 *hi556 = to_hi556(sd);
	int ret = 0;

	mutex_lock(&hi556->mutex);
	if (enable) {
		ret = pm_runtime_resume_and_get(hi556->dev);
		if (ret < 0) {
			mutex_unlock(&hi556->mutex);
			return ret;
		}

		ret = hi556_start_streaming(hi556);
		if (ret) {
			hi556_stop_streaming(hi556);
			pm_runtime_put(hi556->dev);
		}
	} else {
		hi556_stop_streaming(hi556);
		pm_runtime_put(hi556->dev);
	}

	mutex_unlock(&hi556->mutex);

	return ret;
}

static int hi556_set_format(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct hi556 *hi556 = to_hi556(sd);
	const struct hi556_mode *mode;
	s32 vblank_def, h_blank;

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes), width,
				      height, fmt->format.width,
				      fmt->format.height);

	mutex_lock(&hi556->mutex);
	hi556_assign_pad_format(mode, &fmt->format);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) = fmt->format;
	} else {
		hi556->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(hi556->link_freq, mode->link_freq_index);
		__v4l2_ctrl_s_ctrl_int64(hi556->pixel_rate,
					 to_pixel_rate(mode->link_freq_index));

		/* Update limits and set FPS to default */
		vblank_def = mode->fll_def - mode->height;
		__v4l2_ctrl_modify_range(hi556->vblank,
					 mode->fll_min - mode->height,
					 HI556_FLL_MAX - mode->height, 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(hi556->vblank, vblank_def);

		h_blank = hi556->cur_mode->llp - hi556->cur_mode->width;

		__v4l2_ctrl_modify_range(hi556->hblank, h_blank, h_blank, 1,
					 h_blank);
	}

	mutex_unlock(&hi556->mutex);

	return 0;
}

static int hi556_get_format(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct hi556 *hi556 = to_hi556(sd);

	mutex_lock(&hi556->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt->format = *v4l2_subdev_state_get_format(sd_state,
							    fmt->pad);
	else
		hi556_assign_pad_format(hi556->cur_mode, &fmt->format);

	mutex_unlock(&hi556->mutex);

	return 0;
}

static int hi556_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int hi556_enum_frame_size(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SGRBG10_1X10)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int hi556_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct hi556 *hi556 = to_hi556(sd);
	struct v4l2_rect *try_crop;

	mutex_lock(&hi556->mutex);
	hi556_assign_pad_format(&supported_modes[0],
				v4l2_subdev_state_get_format(fh->state, 0));

	/* Initialize try_crop rectangle. */
	try_crop = v4l2_subdev_state_get_crop(fh->state, 0);
	try_crop->top = HI556_PIXEL_ARRAY_TOP;
	try_crop->left = HI556_PIXEL_ARRAY_LEFT;
	try_crop->width = HI556_PIXEL_ARRAY_WIDTH;
	try_crop->height = HI556_PIXEL_ARRAY_HEIGHT;

	mutex_unlock(&hi556->mutex);

	return 0;
}

static const struct v4l2_subdev_video_ops hi556_video_ops = {
	.s_stream = hi556_set_stream,
};

static const struct v4l2_subdev_pad_ops hi556_pad_ops = {
	.set_fmt = hi556_set_format,
	.get_fmt = hi556_get_format,
	.get_selection = hi556_get_selection,
	.enum_mbus_code = hi556_enum_mbus_code,
	.enum_frame_size = hi556_enum_frame_size,
};

static const struct v4l2_subdev_ops hi556_subdev_ops = {
	.video = &hi556_video_ops,
	.pad = &hi556_pad_ops,
};

static const struct media_entity_operations hi556_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops hi556_internal_ops = {
	.open = hi556_open,
};

static int hi556_check_hwcfg(struct device *dev)
{
	struct fwnode_handle *ep;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = 0;
	unsigned int i, j;

	/*
	 * Sometimes the fwnode graph is initialized by the bridge driver,
	 * wait for this.
	 */
	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return dev_err_probe(dev, -EPROBE_DEFER,
				     "waiting for fwnode graph endpoint\n");

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return dev_err_probe(dev, ret, "parsing endpoint failed\n");

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != 2) {
		dev_err(dev, "number of CSI2 data lanes %d is not supported\n",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto check_hwcfg_error;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(dev, "no link frequencies defined\n");
		ret = -EINVAL;
		goto check_hwcfg_error;
	}

	for (i = 0; i < ARRAY_SIZE(link_freq_menu_items); i++) {
		for (j = 0; j < bus_cfg.nr_of_link_frequencies; j++) {
			if (link_freq_menu_items[i] ==
				bus_cfg.link_frequencies[j])
				break;
		}

		if (j == bus_cfg.nr_of_link_frequencies) {
			dev_err(dev, "no link frequency %lld supported\n",
				link_freq_menu_items[i]);
			ret = -EINVAL;
			goto check_hwcfg_error;
		}
	}

check_hwcfg_error:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

static void hi556_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct hi556 *hi556 = to_hi556(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	pm_runtime_disable(hi556->dev);
	mutex_destroy(&hi556->mutex);
}

static int hi556_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hi556 *hi556 = to_hi556(sd);

	gpiod_set_value_cansleep(hi556->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(hi556_supply_names),
			       hi556->supplies);
	clk_disable_unprepare(hi556->clk);
	return 0;
}

static int hi556_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hi556 *hi556 = to_hi556(sd);
	int ret;

	ret = clk_prepare_enable(hi556->clk);
	if (ret)
		return ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(hi556_supply_names),
				    hi556->supplies);
	if (ret) {
		dev_err(dev, "failed to enable regulators: %d", ret);
		clk_disable_unprepare(hi556->clk);
		return ret;
	}

	if (hi556->reset_gpio) {
		/* Assert reset for at least 2ms on back to back off-on */
		usleep_range(2000, 2200);
		gpiod_set_value_cansleep(hi556->reset_gpio, 0);
	}

	usleep_range(5000, 5500);
	return 0;
}

static int hi556_probe(struct i2c_client *client)
{
	struct hi556 *hi556;
	unsigned long freq;
	bool full_power;
	int i, ret;

	ret = hi556_check_hwcfg(&client->dev);
	if (ret)
		return ret;

	hi556 = devm_kzalloc(&client->dev, sizeof(*hi556), GFP_KERNEL);
	if (!hi556)
		return -ENOMEM;

	hi556->dev = &client->dev;

	v4l2_i2c_subdev_init(&hi556->sd, client, &hi556_subdev_ops);

	hi556->reset_gpio = devm_gpiod_get_optional(hi556->dev, "reset",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(hi556->reset_gpio))
		return dev_err_probe(hi556->dev, PTR_ERR(hi556->reset_gpio),
				     "failed to get reset GPIO\n");

	hi556->clk = devm_v4l2_sensor_clk_get(hi556->dev, "clk");
	if (IS_ERR(hi556->clk))
		return dev_err_probe(hi556->dev, PTR_ERR(hi556->clk),
				     "failed to get clock\n");

	freq = clk_get_rate(hi556->clk);
	if (freq != HI556_MCLK)
		return dev_err_probe(hi556->dev, -EINVAL,
				     "external clock %lu is not supported\n",
				     freq);

	for (i = 0; i < ARRAY_SIZE(hi556_supply_names); i++)
		hi556->supplies[i].supply = hi556_supply_names[i];

	ret = devm_regulator_bulk_get(hi556->dev,
				      ARRAY_SIZE(hi556_supply_names),
				      hi556->supplies);
	if (ret)
		return dev_err_probe(hi556->dev, ret,
				     "failed to get regulators\n");

	full_power = acpi_dev_state_d0(hi556->dev);
	if (full_power) {
		/* Ensure non ACPI managed resources are enabled */
		ret = hi556_resume(hi556->dev);
		if (ret)
			return dev_err_probe(hi556->dev, ret,
					     "failed to power on sensor\n");

		ret = hi556_identify_module(hi556);
		if (ret) {
			dev_err(hi556->dev, "failed to find sensor: %d\n", ret);
			goto probe_error_power_off;
		}
	}

	mutex_init(&hi556->mutex);
	hi556->cur_mode = &supported_modes[0];
	ret = hi556_init_controls(hi556);
	if (ret) {
		dev_err(hi556->dev, "failed to init controls: %d\n", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	hi556->sd.internal_ops = &hi556_internal_ops;
	hi556->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	hi556->sd.entity.ops = &hi556_subdev_entity_ops;
	hi556->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	hi556->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&hi556->sd.entity, 1, &hi556->pad);
	if (ret) {
		dev_err(hi556->dev, "failed to init entity pads: %d\n", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&hi556->sd);
	if (ret < 0) {
		dev_err(hi556->dev, "failed to register V4L2 subdev: %d\n",
			ret);
		goto probe_error_media_entity_cleanup;
	}

	/* Set the device's state to active if it's in D0 state. */
	if (full_power)
		pm_runtime_set_active(hi556->dev);
	pm_runtime_enable(hi556->dev);
	pm_runtime_idle(hi556->dev);

	return 0;

probe_error_media_entity_cleanup:
	media_entity_cleanup(&hi556->sd.entity);

probe_error_v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(hi556->sd.ctrl_handler);
	mutex_destroy(&hi556->mutex);

probe_error_power_off:
	if (full_power)
		hi556_suspend(hi556->dev);

	return ret;
}

static DEFINE_RUNTIME_DEV_PM_OPS(hi556_pm_ops, hi556_suspend, hi556_resume,
				 NULL);

#ifdef CONFIG_ACPI
static const struct acpi_device_id hi556_acpi_ids[] = {
	{"INT3537"},
	{}
};

MODULE_DEVICE_TABLE(acpi, hi556_acpi_ids);
#endif

static const struct of_device_id hi556_of_ids[] = {
 { .compatible = "hynix,hi556" }, {}
};
MODULE_DEVICE_TABLE(of, hi556_of_ids);

static struct i2c_driver hi556_i2c_driver = {
	.driver = {
		.name = "hi556",
		.of_match_table = hi556_of_ids,
		.acpi_match_table = ACPI_PTR(hi556_acpi_ids),
		.pm = pm_sleep_ptr(&hi556_pm_ops),
	},
	.probe = hi556_probe,
	.remove = hi556_remove,
	.flags = I2C_DRV_ACPI_WAIVE_D0_PROBE,
};

module_i2c_driver(hi556_i2c_driver);

MODULE_AUTHOR("Shawn Tu");
MODULE_DESCRIPTION("Hynix HI556 sensor driver");
MODULE_LICENSE("GPL v2");
