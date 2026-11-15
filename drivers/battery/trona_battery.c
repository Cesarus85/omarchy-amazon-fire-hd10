// SPDX-License-Identifier: GPL-2.0-only
/* Measurement-only Trona battery integration. Stock MT6358 latch protocol,
 * shunt calibration and CosMX curves; existing IIO drivers serialize ADC access.
 * No writes to charger controls, limits, protection, or coulomb reset bits.
 * Initial state of charge is a temperature/load-compensated curve estimate;
 * subsequent changes use the physical coulomb counter, including fractional uAh.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6397/rtc.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/jiffies.h>
#include "trona_battery_math.h"

static struct device *rtc_dev, *charger_adc_dev, *pmic_adc_dev;
static struct platform_device *board_dev;
static struct regmap *pmic, *charger;
static struct iio_channel voltage_chan, vbus_chan, temp_chan;
static struct power_supply *battery_psy, *usb_psy;
static DEFINE_MUTEX(state_lock);
static struct delayed_work sample_work;
static bool initialized, stopping;
static int voltage_uv, vbus_uv, current_ua, temp_dc, charge_uah, full_uah;
static int online, input_limit_ua, battery_status, variant, sample_error;
static long long previous_ticks, fraction;
static unsigned long last_sample, relaxed_since;

static int adc_name_match(struct device *dev, const void *name)
{
 struct iio_dev *adc = dev_to_iio_dev(dev);
 return adc->name && !strcmp(adc->name, name);
}
static int find_channel(struct device *dev, const char *name, struct iio_channel *chan)
{
 struct iio_dev *adc = dev_to_iio_dev(dev);
 int i;
 for (i=0;i<adc->num_channels;i++)
  if (adc->channels[i].datasheet_name && !strcmp(adc->channels[i].datasheet_name,name)) {
   chan->indio_dev=adc; chan->channel=&adc->channels[i]; return 0;
  }
 return -ENODEV;
}
/* MT6370 exposes labels through read_label, not datasheet_name. These are
 * its published IIO channel indices; verify both index and ADC mux address. */
static int find_mt6370_channel(int index, int mux, struct iio_channel *chan)
{
 struct iio_dev *adc=dev_to_iio_dev(charger_adc_dev);
 int i;
 for(i=0;i<adc->num_channels;i++)
  if(adc->channels[i].channel==index && adc->channels[i].address==mux && adc->channels[i].type==IIO_VOLTAGE) {
   chan->indio_dev=adc;chan->channel=&adc->channels[i];return 0;
  }
 return -ENODEV;
}
static int read_counter(unsigned int *fg_current, unsigned int *car)
{
 unsigned int ready, lo, hi;
 int ret, cleanup;
 ret=regmap_update_bits(pmic,0xd0a,0x000f,0x0001);
 if (!ret) ret=regmap_read_poll_timeout(pmic,0xd0a,ready,ready & BIT(15),100,10000);
 if (!ret) ret=regmap_read(pmic,0xd8a,fg_current);
 if (!ret) ret=regmap_read(pmic,0xd14,&lo);
 if (!ret) ret=regmap_read(pmic,0xd16,&hi);
 cleanup=regmap_update_bits(pmic,0xd0a,0x000f,0x0008);
 if (!cleanup) cleanup=regmap_read_poll_timeout(pmic,0xd0a,ready,!(ready & BIT(15)),100,10000);
 if (!ret) ret=cleanup;
 cleanup=regmap_update_bits(pmic,0xd0a,0x000f,0);
 if (!ret) ret=cleanup;
 if (!ret) *car=(hi<<16)|lo;
 return ret;
}
static int update_sample(void)
{
 unsigned int fg_raw, car, cv_reg, chg_stat, in_reg;
 int ret, mv, bus_mv, temp_raw, measured_temp, measured_current, new_variant;
 int estimate, target_full;
 long long ticks, numerator, delta;
 ret=read_counter(&fg_raw,&car);
 if (ret) return ret;
 ret=iio_read_channel_processed(&voltage_chan,&mv);
 if (ret) return ret;
 ret=iio_read_channel_processed(&vbus_chan,&bus_mv);
 if (ret) return ret;
 ret=iio_read_channel_raw(&temp_chan,&temp_raw);
 if (ret < 0) return ret;
 ret=regmap_read(charger,0x114,&cv_reg);
 if (!ret) ret=regmap_read(charger,0x14a,&chg_stat);
 if (!ret) ret=regmap_read(charger,0x113,&in_reg);
 if (ret) return ret;
 if (mv<2500 || mv>4600 || bus_mv<0 || bus_mv>14000 || temp_raw>4095) return -ERANGE;
 measured_temp=trona_temp_deci_c(temp_raw);
 if (measured_temp == -10000) return -ENODATA;
 /* Native driver currently exports full-scale, not per-LSB scale for this
  * PMIC channel. The stock BAT_TEMP path is 12-bit, 2 * 1.8 V, 90k/47k NTC.
  * Only this known channel's raw value is converted here.
  */
 cv_reg=3900000+((cv_reg>>1)&0x7f)*10000;
 if (cv_reg==4350000) new_variant=0;
 else if (cv_reg==4200000) new_variant=1;
 else if (cv_reg==4000000) new_variant=2;
 else return -EOPNOTSUPP;
 measured_current=trona_current_ua(fg_raw);
 ticks=trona_fg_ticks(car);
 estimate=trona_estimate_charge(new_variant,measured_temp,mv*1000,measured_current,&target_full);
 if (!initialized || new_variant!=variant) {
  charge_uah=estimate; fraction=0; relaxed_since=0;
 } else {
  numerator=trona_tick_delta(ticks,previous_ticks)*108507+fraction;
  delta=numerator/2048000;
  fraction=numerator-delta*2048000;
  if (delta>1000000 || delta< -1000000) {
   dev_warn(&board_dev->dev,"counter discontinuity; re-estimating charge\n");
   charge_uah=estimate; fraction=0;
  } else charge_uah=clamp_t(long long,(long long)charge_uah+delta,0,target_full);
  /* Correct slow drift only after ten minutes with very small net current. */
  if (measured_current>-10000 && measured_current<10000) {
   if (!relaxed_since) relaxed_since=jiffies;
   if (time_after(jiffies,relaxed_since+600*HZ))
    charge_uah+=clamp(estimate-charge_uah,-target_full/1000,target_full/1000);
  } else relaxed_since=0;
 }
 previous_ticks=ticks; variant=new_variant; full_uah=target_full;
 voltage_uv=mv*1000; vbus_uv=bus_mv*1000; current_ua=measured_current; temp_dc=measured_temp;
 online=bus_mv>=4000;
 input_limit_ua=100000+((in_reg>>2)&0x3f)*50000;
 if (!online || current_ua< -15000) battery_status=POWER_SUPPLY_STATUS_DISCHARGING;
 else if (((chg_stat>>6)&3)==2 && voltage_uv>=(int)cv_reg-100000) {
  battery_status=POWER_SUPPLY_STATUS_FULL; charge_uah=full_uah;
 } else if (current_ua>15000) battery_status=POWER_SUPPLY_STATUS_CHARGING;
 else battery_status=POWER_SUPPLY_STATUS_NOT_CHARGING;
 charge_uah=clamp(charge_uah,0,full_uah);
 initialized=true; last_sample=jiffies;
 return 0;
}
static enum power_supply_property battery_properties[]={
 POWER_SUPPLY_PROP_PRESENT,POWER_SUPPLY_PROP_STATUS,POWER_SUPPLY_PROP_TECHNOLOGY,
 POWER_SUPPLY_PROP_SCOPE,POWER_SUPPLY_PROP_MODEL_NAME,POWER_SUPPLY_PROP_MANUFACTURER,
 POWER_SUPPLY_PROP_VOLTAGE_NOW,POWER_SUPPLY_PROP_CURRENT_NOW,POWER_SUPPLY_PROP_TEMP,
 POWER_SUPPLY_PROP_CAPACITY,POWER_SUPPLY_PROP_CHARGE_NOW,POWER_SUPPLY_PROP_CHARGE_FULL,
 POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW
};
static int battery_get(struct power_supply *psy,enum power_supply_property prop,union power_supply_propval *val)
{
 int ret=0;
 mutex_lock(&state_lock);
 if (!initialized || time_after(jiffies,last_sample+60*HZ)) {ret=-ENODATA;goto done;}
 switch(prop) {
 case POWER_SUPPLY_PROP_PRESENT:val->intval=1;break;
 case POWER_SUPPLY_PROP_STATUS:val->intval=battery_status;break;
 case POWER_SUPPLY_PROP_TECHNOLOGY:val->intval=POWER_SUPPLY_TECHNOLOGY_LION;break;
 case POWER_SUPPLY_PROP_SCOPE:val->intval=POWER_SUPPLY_SCOPE_SYSTEM;break;
 case POWER_SUPPLY_PROP_MODEL_NAME:val->strval="Fire HD10 CosMX (estimated capacity)";break;
 case POWER_SUPPLY_PROP_MANUFACTURER:val->strval="CosMX";break;
 case POWER_SUPPLY_PROP_VOLTAGE_NOW:val->intval=voltage_uv;break;
 case POWER_SUPPLY_PROP_CURRENT_NOW:val->intval=current_ua;break;
 case POWER_SUPPLY_PROP_TEMP:val->intval=temp_dc;break;
 case POWER_SUPPLY_PROP_CAPACITY:val->intval=DIV_ROUND_CLOSEST((long long)charge_uah*100,full_uah);break;
 case POWER_SUPPLY_PROP_CHARGE_NOW:val->intval=charge_uah;break;
 case POWER_SUPPLY_PROP_CHARGE_FULL:val->intval=full_uah;break;
 case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:val->intval=6500000;break;
 case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
  if(current_ua>=-10000)ret=-ENODATA;
  else val->intval=(long long)charge_uah*3600/-current_ua;
  break;
 default:ret=-EINVAL;
 }
done:mutex_unlock(&state_lock);return ret;
}
static enum power_supply_property usb_properties[]={POWER_SUPPLY_PROP_ONLINE,POWER_SUPPLY_PROP_VOLTAGE_NOW,POWER_SUPPLY_PROP_CURRENT_MAX};
static int usb_get(struct power_supply *psy,enum power_supply_property prop,union power_supply_propval *val)
{
 int ret=0;
 mutex_lock(&state_lock);
 if (!initialized || time_after(jiffies,last_sample+60*HZ)) {ret=-ENODATA;goto done;}
 switch(prop){
 case POWER_SUPPLY_PROP_ONLINE:val->intval=online;break;
 case POWER_SUPPLY_PROP_VOLTAGE_NOW:val->intval=vbus_uv;break;
 case POWER_SUPPLY_PROP_CURRENT_MAX:val->intval=input_limit_ua;break;
 default:ret=-EINVAL;
 }
done:mutex_unlock(&state_lock);return ret;
}
static const struct power_supply_desc battery_desc={.name="BAT0",.type=POWER_SUPPLY_TYPE_BATTERY,
 .properties=battery_properties,.num_properties=ARRAY_SIZE(battery_properties),.get_property=battery_get};
static const struct power_supply_desc usb_desc={.name="trona-usb-power",.type=POWER_SUPPLY_TYPE_USB,
 .properties=usb_properties,.num_properties=ARRAY_SIZE(usb_properties),.get_property=usb_get};
static void poll_battery(struct work_struct *work)
{
 mutex_lock(&state_lock);sample_error=update_sample();mutex_unlock(&state_lock);
 if(!sample_error){power_supply_changed(battery_psy);power_supply_changed(usb_psy);}
 if(!READ_ONCE(stopping))queue_delayed_work(system_freezable_wq,&sample_work,10*HZ);
}
static ssize_t estimator_show(struct device *dev,struct device_attribute *attr,char *buf)
{
 ssize_t n;
 mutex_lock(&state_lock);
 n=sysfs_emit(buf,"profile=%d charge_uah=%d full_uah=%d ticks=%lld last_error=%d initial=load-and-temperature-compensated-OCV tracking=hardware-coulomb-counter\n",variant,charge_uah,full_uah,previous_ticks,sample_error);
 mutex_unlock(&state_lock);return n;
}
static DEVICE_ATTR_RO(estimator);
static int __init trona_battery_init(void)
{
 struct device_node *idme;
 struct mt6397_rtc *rtc;
 struct power_supply_config config={};
 const char *id;int length,ret;
 if(!of_machine_is_compatible("amazon,trona"))return -ENODEV;
 idme=of_find_node_by_path("/idme/productid2");
 if(!idme)return -ENODEV;
 id=of_get_property(idme,"value",&length);
 ret=(!id || length<4 || memcmp(id,"1456",4))?-ENODEV:0;
 of_node_put(idme);if(ret)return ret;
 rtc_dev=bus_find_device_by_name(&platform_bus_type,NULL,"mt6358-rtc");
 if(!rtc_dev)return -ENODEV;
 rtc=dev_get_drvdata(rtc_dev);
 if(!rtc || rtc->addr_base!=0x588){ret=-ENODEV;goto put;}
 pmic=rtc->regmap;
 charger_adc_dev=bus_find_device(&iio_bus_type,NULL,"mt6370-adc",adc_name_match);
 pmic_adc_dev=bus_find_device(&iio_bus_type,NULL,"MT6358",adc_name_match);
 if(!charger_adc_dev || !pmic_adc_dev){ret=-EPROBE_DEFER;goto put;}
 ret=find_mt6370_channel(3,4,&voltage_chan);
 if(!ret)ret=find_mt6370_channel(0,1,&vbus_chan);
 if(!ret)ret=find_channel(pmic_adc_dev,"batt_temp",&temp_chan);
 if(ret)goto put;
 charger=dev_get_regmap(charger_adc_dev->parent->parent,NULL);
 if(!charger){ret=-ENODEV;goto put;}
 board_dev=platform_device_register_simple("trona-battery",-1,NULL,0);
 if(IS_ERR(board_dev)){ret=PTR_ERR(board_dev);goto put;}
 ret=update_sample();if(ret)goto unregister_dev;
 battery_psy=power_supply_register(&board_dev->dev,&battery_desc,&config);
 if(IS_ERR(battery_psy)){ret=PTR_ERR(battery_psy);goto unregister_dev;}
 usb_psy=power_supply_register(&board_dev->dev,&usb_desc,&config);
 if(IS_ERR(usb_psy)){ret=PTR_ERR(usb_psy);goto unregister_battery;}
 ret=device_create_file(&board_dev->dev,&dev_attr_estimator);
 if(ret)goto unregister_usb;
 INIT_DELAYED_WORK(&sample_work,poll_battery);
 queue_delayed_work(system_freezable_wq,&sample_work,10*HZ);
 dev_info(&board_dev->dev,"CosMX gauge: %d mV, %d mA, %d/10 C; capacity is an estimate\n",voltage_uv/1000,current_ua/1000,temp_dc);
 return 0;
unregister_usb:power_supply_unregister(usb_psy);
unregister_battery:power_supply_unregister(battery_psy);
unregister_dev:platform_device_unregister(board_dev);
put:put_device(pmic_adc_dev);put_device(charger_adc_dev);put_device(rtc_dev);return ret;
}
static void __exit trona_battery_exit(void)
{
 WRITE_ONCE(stopping,true);cancel_delayed_work_sync(&sample_work);
 device_remove_file(&board_dev->dev,&dev_attr_estimator);
 power_supply_unregister(usb_psy);power_supply_unregister(battery_psy);
 platform_device_unregister(board_dev);
 put_device(pmic_adc_dev);put_device(charger_adc_dev);put_device(rtc_dev);
}
module_init(trona_battery_init);module_exit(trona_battery_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Trona CosMX battery measurement and coulomb tracking, charger read-only");
