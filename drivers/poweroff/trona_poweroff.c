// SPDX-License-Identifier: GPL-2.0-only
/* Trona MT6358 shutdown, based on Amazon's mt6358/mtk_rtc_hal.c and pmic.c.
 * Deliberately board-specific. Loading only reads registers; all writes occur
 * after a normal Linux shutdown has synchronized and unmounted filesystems.
 * Preserve the already initialized oscillator configuration.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mfd/mt6397/rtc.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>

#define TRONA_RTC_BASE 0x588
#define MT6358_TOP_TMA_KEY 0x3a8
#define MT6358_PPCCTL0 0xa08
#define MT6358_CHR_CON4 0xa90
#define MT6358_PSEQ_ELR11 0xa62
#define RTC_PROT 0x34
#define RTC_BBPU_CLR BIT(1)
#define RTC_BBPU_PWREN BIT(0)
#define RTC_BBPU_RELOAD BIT(5)

static struct device *rtc_device;
static struct mt6397_rtc *rtc;
static struct sys_off_handler *prepare_handler, *off_handler;
static bool prepared;
static bool inhibit_charger_boot;
module_param(inhibit_charger_boot, bool, 0644);
MODULE_PARM_DESC(inhibit_charger_boot, "Disable charger-triggered startup at shutdown (Trona test)");

static int trigger(void)
{
	u32 value;
	int ret = regmap_write(rtc->regmap, rtc->addr_base + rtc->data->wrtgr, 1);
	if (ret)
		return ret;
	return regmap_read_poll_timeout(rtc->regmap, rtc->addr_base + RTC_BBPU,
			value, !(value & RTC_BBPU_CBUSY), 10, 500000);
}

static int write_rtc(unsigned int offset, unsigned int value)
{
	int ret = regmap_write(rtc->regmap, rtc->addr_base + offset, value);
	return ret ? ret : trigger();
}

static int prepare_poweroff(struct sys_off_data *data)
{
	u32 value;
	int ret, lock_ret;
	prepared = false;
	mutex_lock(&rtc->lock);
	ret = write_rtc(RTC_PROT, 0x586a);
	if (!ret) ret = write_rtc(RTC_PROT, 0x9136);
	/* Vendor firmware leaves its two-second automatic reboot armed. */
	if (!ret) ret = regmap_update_bits(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
					 BIT(8) | BIT(6), 0);
	if (!ret) ret = trigger();
	/* Vendor low-power shutdown sequence: clear spare/alarm latch. */
	if (!ret) ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_BBPU,
				    RTC_BBPU_KEY | RTC_BBPU_CLR | RTC_BBPU_PWREN);
	if (!ret) ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_AL_MASK,
				    RTC_AL_MASK_DOW);
	if (!ret) ret = trigger();
	if (!ret) ret = regmap_read_poll_timeout(rtc->regmap,
			 rtc->addr_base + RTC_BBPU, value,
			 !(value & RTC_BBPU_CLR), 10, 500000);
	if (!ret) {
		ret = regmap_write(rtc->regmap, MT6358_TOP_TMA_KEY, 0x9ca7);
		if (!ret) ret = regmap_update_bits(rtc->regmap, MT6358_PSEQ_ELR11,
						 BIT(10), 0);
		lock_ret = regmap_write(rtc->regmap, MT6358_TOP_TMA_KEY, 0);
		if (!ret) ret = lock_ret;
	}
	if (!ret) ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_BBPU, &value);
	if (!ret) ret = write_rtc(RTC_BBPU, value | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	/* Exact vendor pmic_charger_auto_on(false) sequence. Opt-in until tested.
	 * This selects the charger power-on latch; it does not change charging
	 * voltage, current, or thermal protection in the separate MT6370.
	 */
	if (!ret && inhibit_charger_boot) {
		ret = regmap_update_bits(rtc->regmap, MT6358_CHR_CON4, BIT(0), BIT(0));
		if (!ret) ret = regmap_update_bits(rtc->regmap, MT6358_CHR_CON4, BIT(1), BIT(1));
		if (!ret) ret = regmap_read(rtc->regmap, MT6358_CHR_CON4, &value);
		if (!ret && (value & 3) != 3) ret = -EIO;
		if (!ret) pr_info("trona_poweroff: charger auto-on inhibited\n");
	}
	mutex_unlock(&rtc->lock);
	if (ret)
		pr_err("trona_poweroff: prepare failed: %d; power hold unchanged\n", ret);
	else {
		prepared = true;
		pr_info("trona_poweroff: RTC prepared, automatic reboot disabled\n");
	}
	return NOTIFY_DONE;
}

static int poweroff(struct sys_off_data *data)
{
	int ret;
	if (!prepared)
		return NOTIFY_DONE;
	pr_emerg("trona_poweroff: releasing MT6358 PWRHOLD\n");
	ret = regmap_update_bits(rtc->regmap, MT6358_PPCCTL0, BIT(0), 0);
	if (ret)
		pr_emerg("trona_poweroff: PWRHOLD write failed: %d\n", ret);
	mdelay(1000);
	pr_emerg("trona_poweroff: power remains after one second\n");
	return NOTIFY_DONE;
}

static int __init trona_poweroff_init(void)
{
	u32 hold;
	int ret;
	if (!of_machine_is_compatible("amazon,trona"))
		return -ENODEV;
	rtc_device = bus_find_device_by_name(&platform_bus_type, NULL, "mt6358-rtc");
	if (!rtc_device)
		return -ENODEV;
	rtc = dev_get_drvdata(rtc_device);
	if (!rtc || rtc->addr_base != TRONA_RTC_BASE ||
	    rtc->data->wrtgr != RTC_WRTGR_MT6358) {
		ret = -ENODEV;
		goto put;
	}
	ret = regmap_read(rtc->regmap, MT6358_PPCCTL0, &hold);
	if (ret) goto put;
	if (!(hold & BIT(0))) {
		ret = -EINVAL;
		goto put;
	}
	prepare_handler = register_sys_off_handler(SYS_OFF_MODE_POWER_OFF_PREPARE,
			SYS_OFF_PRIO_HIGH, prepare_poweroff, NULL);
	if (IS_ERR(prepare_handler)) {
		ret = PTR_ERR(prepare_handler);
		goto put;
	}
	off_handler = register_sys_off_handler(SYS_OFF_MODE_POWER_OFF,
			SYS_OFF_PRIO_HIGH, poweroff, NULL);
	if (IS_ERR(off_handler)) {
		ret = PTR_ERR(off_handler);
		unregister_sys_off_handler(prepare_handler);
		goto put;
	}
	pr_info("trona_poweroff: registered; PWRHOLD=%#x, no registers changed\n", hold);
	return 0;
put:
	put_device(rtc_device);
	return ret;
}

static void __exit trona_poweroff_exit(void)
{
	unregister_sys_off_handler(off_handler);
	unregister_sys_off_handler(prepare_handler);
	put_device(rtc_device);
}
module_init(trona_poweroff_init);
module_exit(trona_poweroff_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Amazon Trona MT6358 orderly poweroff");
