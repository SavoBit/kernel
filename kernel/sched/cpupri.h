#ifndef _LINUX_CPUPRI_H
#define _LINUX_CPUPRI_H

#include <linux/sched.h>
// 2016-07-01
#define CPUPRI_NR_PRIORITIES	(MAX_RT_PRIO + 2) // 100 + 2

// 2016-07-01
#define CPUPRI_INVALID -1
#define CPUPRI_IDLE     0
#define CPUPRI_NORMAL   1
/* values 2-101 are RT priorities 0-99 */

// 2016-06-25
// 2016-07-01
struct cpupri_vec {
	atomic_t	count;
	cpumask_var_t	mask;
};

// 2016-06-25
struct cpupri {
	struct cpupri_vec pri_to_cpu[CPUPRI_NR_PRIORITIES];
	int               cpu_to_pri[NR_CPUS];
};

#ifdef CONFIG_SMP
int  cpupri_find(struct cpupri *cp,
		 struct task_struct *p, struct cpumask *lowest_mask);
void cpupri_set(struct cpupri *cp, int cpu, int pri);
int cpupri_init(struct cpupri *cp);
void cpupri_cleanup(struct cpupri *cp);
#else
#define cpupri_set(cp, cpu, pri) do { } while (0)
#define cpupri_init() do { } while (0)
#endif

#endif /* _LINUX_CPUPRI_H */
