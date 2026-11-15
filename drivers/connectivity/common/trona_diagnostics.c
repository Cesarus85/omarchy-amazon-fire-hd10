// SPDX-License-Identifier: GPL-2.0-only
/* Native replacements for vendor diagnostics and the standard SDIO CMD52 API. */
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/sdio.h>
#include <linux/sched/signal.h>
#include <linux/sched/debug.h>
#include <linux/stacktrace.h>
#include "connectivity_build_in_adapter.h"
#include "vendor-headers/aee.h"

void connectivity_export_show_stack(struct task_struct *task, unsigned long *sp)
{
 unsigned long entries[32];
 unsigned int count;
 if (!task) task = current;
 count = stack_trace_save_tsk(task, entries, ARRAY_SIZE(entries), 0);
 stack_trace_print(entries, count, 0);
}
EXPORT_SYMBOL(connectivity_export_show_stack);
void connectivity_export_dump_thread_state(const char *name)
{
 struct task_struct *task, *found = NULL;
 if (!name) return;
 rcu_read_lock();
 for_each_process(task) {
  if (!strncmp(task->comm, name, TASK_COMM_LEN)) {
   get_task_struct(task); found = task; break;
  }
 }
 rcu_read_unlock();
 if (found) { sched_show_task(found); put_task_struct(found); }
}
EXPORT_SYMBOL(connectivity_export_dump_thread_state);
void connectivity_export_dump_gpio_info(int start, int end)
{
 int pin;
 if (start < 0 || end > 191 || end < start) return;
 for (pin = start; pin <= end; pin++)
  if (gpio_is_valid(pin)) pr_info("Trona GPIO%d level=%d\n", pin, gpio_get_value(pin));
}
EXPORT_SYMBOL(connectivity_export_dump_gpio_info);
int connectivity_export_mmc_io_rw_direct(struct mmc_card *card, int write,
 unsigned int fn, unsigned int addr, u8 in, u8 *out)
{
 struct mmc_command cmd = {};
 int ret;
 if (!card || fn > 7 || addr > 0x1ffff) return -EINVAL;
 cmd.opcode = SD_IO_RW_DIRECT;
 cmd.arg = (write ? BIT(31) : 0) | (fn << 28) |
           (write && out ? BIT(27) : 0) | (addr << 9) | in;
 cmd.flags = MMC_RSP_R5 | MMC_CMD_AC;
 ret = mmc_wait_for_cmd(card->host, &cmd, 0);
 if (ret) return ret;
 if (cmd.resp[0] & R5_ERROR) return -EIO;
 if (cmd.resp[0] & R5_FUNCTION_NUMBER) return -EINVAL;
 if (cmd.resp[0] & R5_OUT_OF_RANGE) return -ERANGE;
 if (out) *out = cmd.resp[0] & 0xff;
 return 0;
}
void aee_kernel_warning_api(const char *file, const int line, const int db_opt,
 const char *module, const char *fmt, ...)
{
 va_list args;
 struct va_format vaf;
 va_start(args, fmt); vaf.fmt = fmt; vaf.va = &args;
 pr_err("Trona radio warning %s:%d %s: %pV\n", file, line, module, &vaf);
 va_end(args);
}
void aee_kernel_dal_api(const char *file, const int line, const char *msg)
{
 pr_err("Trona radio diagnostic %s:%d: %s\n", file, line, msg);
}
void aed_combo_exception_api(const int *log, int log_size, const int *phy,
 int phy_size, const char *detail, const int db_opt)
{
 pr_err("Trona radio exception: %s (log=%d bytes, physical dump=%d bytes)\n",
        detail, log_size, phy_size);
 if (log && log_size > 0)
  print_hex_dump(KERN_ERR, "radio log: ", DUMP_PREFIX_OFFSET, 16, 1,
                 log, min(log_size, 256), true);
}
