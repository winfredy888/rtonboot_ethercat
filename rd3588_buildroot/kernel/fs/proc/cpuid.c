#include <linux/cpufreq.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

extern unsigned int system_serial_low;
extern unsigned int system_serial_high;

static int cpuid_proc_show(struct seq_file *m, void *v)
{

	seq_printf(m, "%08x%08x\n",
                   system_serial_high, system_serial_low);
	return 0;
}

static int __init proc_cpuid_init(void)
{
        proc_create_single("cpuid", 0, NULL, cpuid_proc_show);
        return 0;
}
fs_initcall(proc_cpuid_init);

