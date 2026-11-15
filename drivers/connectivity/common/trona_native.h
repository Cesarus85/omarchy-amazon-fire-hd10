/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TRONA_CONNECTIVITY_NATIVE_H
#define TRONA_CONNECTIVITY_NATIVE_H
#include <linux/platform_device.h>
int trona_conn_native_init(struct platform_device *pdev);
void trona_conn_native_exit(void);
int trona_conn_pmic_write(unsigned int reg, unsigned int value);
int trona_conn_pmic_update(unsigned int reg, unsigned int mask, unsigned int value);
int trona_conn_mpu_protect(void);
int trona_conn_power(bool enable);
int trona_conn_reset(bool assert);
int trona_conn_pinmux(bool enable);
int trona_conn_co_clock_type(void);
int trona_conn_clockbuf(bool enable);
#endif
