// SPDX-License-Identifier: GPL-2.0
/* Experimental fixed decoder-only MMIO service; no permission-register access. */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/freezer.h>
static struct platform_device *pdev;
static struct clk *clk;
static void __iomem *m,*con,*dec;
static struct task_struct *worker;
static bool clock_active;
static unsigned long transactions, suspend_count, resume_count, errors;
module_param(clock_active, bool, 0444);
module_param(transactions, ulong, 0444);
module_param(suspend_count, ulong, 0444);
module_param(resume_count, ulong, 0444);
module_param(errors, ulong, 0444);
/* A hard symbol dependency pins this service until patched mtk_scp unloads. */
int trona_vdec_proxy_require(void);
int trona_vdec_proxy_require(void)
{
 return READ_ONCE(clock_active) ? 0 : -EIO;
}
EXPORT_SYMBOL_GPL(trona_vdec_proxy_require);
static int serve(void *arg)
{
 unsigned long last=jiffies;
 set_freezable();
 while(!kthread_should_stop()){
  u32 q=readl(m+4),done=readl(m+8),a,w,val; void __iomem *r=NULL;
  if(readl(m)!=0x50525859 || q==done){
   /* Userspace freezes first. Drain the final firmware exchange before
    * releasing our clock reference; SCP's own PM callback runs later.
    * Never touch TCM while this worker has relinquished its clock. */
   if(freezing(current) && time_after(jiffies,last+2)){
    clk_disable_unprepare(clk);clock_active=false;suspend_count++;
    if(kthread_freezable_should_stop(NULL))return 0;
    if(clk_prepare_enable(clk)){
     pr_err("trona_vdec_proxy: SCP clock resume failed\n");return -EIO;
    }
    clock_active=true;resume_count++;last=jiffies;
    continue;
   }
   if(time_after(jiffies,last+1))usleep_range(500,1000);else cpu_relax();
   continue;
  }
  last=jiffies; rmb(); a=readl(m+12);w=readl(m+20);val=readl(m+16);
  if(a>=0x16000000 && a<0x16000100)r=con+(a-0x16000000);
  if(a>=0x16020000 && a<0x16025000)r=dec+(a-0x16020000);
  if(!r || (a&3) || readl(m+24)!=4 || w>1 || q!=done+1){errors++;writel(6,m+28);}
  else if(w)writel(val,r);else writel(readl(r),m+16);
  wmb();writel(q,m+8);transactions++;cond_resched();
 }
 return 0;
}
static int __init proxy_init(void)
{
 struct device_node *np;int ret;
 if(!of_machine_is_compatible("amazon,trona"))return -ENODEV;
 np=of_find_node_by_path("/soc/scp@10500000");if(!np)return -ENODEV;
 pdev=of_find_device_by_node(np);of_node_put(np);if(!pdev)return -ENODEV;
 clk=clk_get(&pdev->dev,"main");if(IS_ERR(clk)){ret=PTR_ERR(clk);goto dev_out;}
 ret=clk_prepare_enable(clk);if(ret)goto clk_out;
 clock_active=true;
 m=ioremap(0x10557000,0x100);con=ioremap(0x16000000,0x100);dec=ioremap(0x16020000,0x5000);
 if(!m||!con||!dec){ret=-ENOMEM;goto maps_out;}
 worker=kthread_run(serve,NULL,"trona-vdec-proxy");
 if(IS_ERR(worker)){ret=PTR_ERR(worker);goto maps_out;}
 pr_info("trona_vdec_proxy: temporary decoder-only worker started\n");return 0;
 maps_out:if(m)iounmap(m);if(con)iounmap(con);if(dec)iounmap(dec);clk_disable_unprepare(clk);clock_active=false;
 clk_out:clk_put(clk);
 dev_out:put_device(&pdev->dev);return ret;
}
static void __exit proxy_exit(void)
{
 kthread_stop(worker);
 if(clock_active)pr_info("trona_vdec_proxy: req=%u done=%u error=%u pc=%08x count=%u ready=%u\n",readl(m+4),readl(m+8),readl(m+28),readl(m+32),readl(m+36),readl(m+56));
 iounmap(m);iounmap(con);iounmap(dec);if(clock_active)clk_disable_unprepare(clk);clk_put(clk);put_device(&pdev->dev);
}
module_init(proxy_init);module_exit(proxy_exit);MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Temporary Trona fixed decoder MMIO proxy experiment");
