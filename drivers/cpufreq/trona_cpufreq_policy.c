// SPDX-License-Identifier: GPL-2.0-only
/* Trona's staged 800 mV / 793..1248 MHz frequency-only bring-up.
 * The generic CV^2f energy model rounds every lower OPP to equal/worse
 * work-per-joule cost, marking all of them inefficient. Governors then skip
 * all lower frequencies. Allow peak-power-oriented frequency selection for
 * this exact provisional table, keeping the EM and thermal cooling intact.
 * This is NOT voltage scaling and does not claim improved energy per job.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/limits.h>
static struct cpufreq_policy *policies[2];
static bool previous[2];
static bool applied[2];
static const unsigned int frequencies[]={793000,910000,1014000,1131000,1248000};
static int validate(struct cpufreq_policy *p)
{
 struct cpufreq_frequency_table *entry;int i=0;
 struct device *cpu=get_cpu_device(p->cpu);
 if(!p->freq_table || p->cpuinfo.min_freq!=793000 || p->cpuinfo.max_freq!=1248000)return -EINVAL;
 cpufreq_for_each_valid_entry(entry,p->freq_table){
  struct dev_pm_opp *opp;unsigned long voltage;
  if(i>=ARRAY_SIZE(frequencies) || entry->frequency!=frequencies[i++])return -EINVAL;
  opp=dev_pm_opp_find_freq_exact(cpu,(unsigned long)entry->frequency*1000,true);
  if(IS_ERR(opp))return PTR_ERR(opp);
  voltage=dev_pm_opp_get_voltage(opp);dev_pm_opp_put(opp);
  if(voltage!=800000)return -EINVAL;
 }
 return i==ARRAY_SIZE(frequencies)?0:-EINVAL;
}
static void restore(void)
{
 int i;
 for(i=0;i<2;i++)if(policies[i]){
  if(applied[i]){
   down_write(&policies[i]->rwsem);
   WRITE_ONCE(policies[i]->efficiencies_available,previous[i]);
   policies[i]->cached_target_freq=UINT_MAX;
   up_write(&policies[i]->rwsem);
   cpufreq_update_policy(policies[i]->cpu);
  }
  cpufreq_cpu_put(policies[i]);policies[i]=NULL;
 }
}
static int __init trona_policy_init(void)
{
 int i,ret;
 if(!of_machine_is_compatible("amazon,trona"))return -ENODEV;
 for(i=0;i<2;i++){
  policies[i]=cpufreq_cpu_get(i*4);
  if(!policies[i]){ret=-EPROBE_DEFER;goto fail;}
  down_read(&policies[i]->rwsem);ret=validate(policies[i]);up_read(&policies[i]->rwsem);
  if(ret)goto fail;
 }
 for(i=0;i<2;i++){
  down_write(&policies[i]->rwsem);
  previous[i]=policies[i]->efficiencies_available;
  WRITE_ONCE(policies[i]->efficiencies_available,false);
  policies[i]->cached_target_freq=UINT_MAX;applied[i]=true;
  up_write(&policies[i]->rwsem);
  cpufreq_update_policy(policies[i]->cpu);
 }
 pr_info("Trona: allow all five validated 800 mV CPU frequencies; thermal EM retained\n");
 return 0;
fail:restore();return ret;
}
static void __exit trona_policy_exit(void){restore();}
module_init(trona_policy_init);module_exit(trona_policy_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Trona frequency-only OPP selection policy");
