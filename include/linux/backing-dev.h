/*
 * include/linux/backing-dev.h
 *
 * low-level device information and state which is propagated up through
 * to high-level code.
 */

#ifndef _LINUX_BACKING_DEV_H
#define _LINUX_BACKING_DEV_H

#include <linux/percpu_counter.h>
#include <linux/log2.h>
#include <linux/flex_proportions.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/writeback.h>
#include <linux/atomic.h>
#include <linux/sysctl.h>
#include <linux/workqueue.h>

struct page;
struct device;
struct dentry;

/*
 * Bits in backing_dev_info.state
 */
enum bdi_state {
	BDI_wb_alloc,		/* Default embedded wb allocated */
	BDI_async_congested,	/* The async (write) queue is getting full */
	BDI_sync_congested,	/* The sync queue is getting full */
	BDI_registered,		/* bdi_register() was done */
	BDI_writeback_running,	/* Writeback is in progress */
	BDI_unused,		/* Available bits start here */
};

typedef int (congested_fn)(void *, int);

// 2015-08-15
// 2017-06-24
enum bdi_stat_item {
	BDI_RECLAIMABLE,
	BDI_WRITEBACK,
	BDI_DIRTIED,
	BDI_WRITTEN,
	NR_BDI_STAT_ITEMS
};

// 2015-08-15
#define BDI_STAT_BATCH (8*(1+/*1*/ilog2(nr_cpu_ids/*2*/)))  // 16

struct bdi_writeback {
	struct backing_dev_info *bdi;	/* our parent bdi */
	unsigned int nr;

	unsigned long last_old_flush;	/* last old data flush */

	struct delayed_work dwork;	/* work item used for writeback */
	struct list_head b_dirty;	/* dirty inodes */
	struct list_head b_io;		/* parked for writeback */
	struct list_head b_more_io;	/* parked for more writeback */
	spinlock_t list_lock;		/* protects the b_* lists */
};

// 2015-07-04;
// 2015-09-05;
// 2016-01-16;
struct backing_dev_info {
	struct list_head bdi_list;
	unsigned long ra_pages;	/* max readahead in PAGE_CACHE_SIZE units */
	unsigned long state;	/* Always use atomic bitops on this */
	unsigned int capabilities; /* Device capabilities */
	congested_fn *congested_fn; /* Function pointer if device is md/dm */
	void *congested_data;	/* Pointer to aux data for congested func */

	char *name;

	struct percpu_counter bdi_stat[NR_BDI_STAT_ITEMS];

	unsigned long bw_time_stamp;	/* last time write bw is updated */
	unsigned long dirtied_stamp;
	unsigned long written_stamp;	/* pages written at bw_time_stamp */
	unsigned long write_bandwidth;	/* the estimated write bandwidth */
	unsigned long avg_write_bandwidth; /* further smoothed write bw */

	/*
	 * The base dirty throttle rate, re-calculated on every 200ms.
	 * All the bdi tasks' dirty rate will be curbed under it.
	 * @dirty_ratelimit tracks the estimated @balanced_dirty_ratelimit
	 * in small steps and is much more smooth/stable than the latter.
	 */
	unsigned long dirty_ratelimit;
	unsigned long balanced_dirty_ratelimit;

	struct fprop_local_percpu completions;
	int dirty_exceeded;

	unsigned int min_ratio;
	unsigned int max_ratio, max_prop_frac;

	struct bdi_writeback wb;  /* default writeback info for this bdi */
	spinlock_t wb_lock;	  /* protects work_list & wb.dwork scheduling */

	struct list_head work_list;

	struct device *dev;

	struct timer_list laptop_mode_wb_timer;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debug_dir;
	struct dentry *debug_stats;
#endif
};

int bdi_init(struct backing_dev_info *bdi);
void bdi_destroy(struct backing_dev_info *bdi);

__printf(3, 4)
int bdi_register(struct backing_dev_info *bdi, struct device *parent,
		const char *fmt, ...);
int bdi_register_dev(struct backing_dev_info *bdi, dev_t dev);
void bdi_unregister(struct backing_dev_info *bdi);
int bdi_setup_and_register(struct backing_dev_info *, char *, unsigned int);
void bdi_start_writeback(struct backing_dev_info *bdi, long nr_pages,
			enum wb_reason reason);
void bdi_start_background_writeback(struct backing_dev_info *bdi);
void bdi_writeback_workfn(struct work_struct *work);
int bdi_has_dirty_io(struct backing_dev_info *bdi);
void bdi_wakeup_thread_delayed(struct backing_dev_info *bdi);
void bdi_lock_two(struct bdi_writeback *wb1, struct bdi_writeback *wb2);

extern spinlock_t bdi_lock;
extern struct list_head bdi_list;

extern struct workqueue_struct *bdi_wq;

// 2015-09-05;
// 리스트가 비워지지 않음을 확인
static inline int wb_has_dirty_io(struct bdi_writeback *wb)
{
	return !list_empty(&wb->b_dirty) ||
	       !list_empty(&wb->b_io) ||
	       !list_empty(&wb->b_more_io);
}

// 2015-08-15
// 2015-12-26, BDI_RECLAIMABLE, -1
static inline void __add_bdi_stat(struct backing_dev_info *bdi,
		enum bdi_stat_item item, s64 amount)
{
	__percpu_counter_add(&bdi->bdi_stat[item], amount/*-1*/, BDI_STAT_BATCH/*16*/);
}

// 2015-09-05;
static inline void __inc_bdi_stat(struct backing_dev_info *bdi,
		enum bdi_stat_item item)
{
	__add_bdi_stat(bdi, item, 1);
}

static inline void inc_bdi_stat(struct backing_dev_info *bdi,
		enum bdi_stat_item item)
{
	unsigned long flags;

	local_irq_save(flags);
	__inc_bdi_stat(bdi, item);
	local_irq_restore(flags);
}

// 2015-08-15
// 2015-12-26, BDI_RECLAIMABLE
static inline void __dec_bdi_stat(struct backing_dev_info *bdi,
		enum bdi_stat_item item)
{
	__add_bdi_stat(bdi, item, -1);
}

// 2015-08-15
// 2015-12-26
// dec_bdi_stat(mapping->backing_dev_info, BDI_RECLAIMABLE);
// -1을 add한다.
static inline void dec_bdi_stat(struct backing_dev_info *bdi,
		enum bdi_stat_item item)
{
	unsigned long flags;

	local_irq_save(flags);
	__dec_bdi_stat(bdi, item);
	local_irq_restore(flags);
}

static inline s64 bdi_stat(struct backing_dev_info *bdi,
		enum bdi_stat_item item)
{
	return percpu_counter_read_positive(&bdi->bdi_stat[item]);
}

static inline s64 __bdi_stat_sum(struct backing_dev_info *bdi,
		enum bdi_stat_item item)
{
	return percpu_counter_sum_positive(&bdi->bdi_stat[item]);
}

static inline s64 bdi_stat_sum(struct backing_dev_info *bdi,
		enum bdi_stat_item item)
{
	s64 sum;
	unsigned long flags;

	local_irq_save(flags);
	sum = __bdi_stat_sum(bdi, item);
	local_irq_restore(flags);

	return sum;
}

extern void bdi_writeout_inc(struct backing_dev_info *bdi);

/*
 * maximal error of a stat counter.
 */
static inline unsigned long bdi_stat_error(struct backing_dev_info *bdi)
{
#ifdef CONFIG_SMP
	return nr_cpu_ids * BDI_STAT_BATCH;
#else
	return 1;
#endif
}

int bdi_set_min_ratio(struct backing_dev_info *bdi, unsigned int min_ratio);
int bdi_set_max_ratio(struct backing_dev_info *bdi, unsigned int max_ratio);

/*
 * Flags in backing_dev_info::capability
 *
 * The first three flags control whether dirty pages will contribute to the
 * VM's accounting and whether writepages() should be called for dirty pages
 * (something that would not, for example, be appropriate for ramfs)
 *
 * WARNING: these flags are closely related and should not normally be
 * used separately.  The BDI_CAP_NO_ACCT_AND_WRITEBACK combines these
 * three flags into a single convenience macro.
 *
 * BDI_CAP_NO_ACCT_DIRTY:  Dirty pages shouldn't contribute to accounting
 * BDI_CAP_NO_WRITEBACK:   Don't write pages back
 * BDI_CAP_NO_ACCT_WB:     Don't automatically account writeback pages
 *
 * These flags let !MMU mmap() govern direct device mapping vs immediate
 * copying more easily for MAP_PRIVATE, especially for ROM filesystems.
 *
 * BDI_CAP_MAP_COPY:       Copy can be mapped (MAP_PRIVATE)
 * BDI_CAP_MAP_DIRECT:     Can be mapped directly (MAP_SHARED)
 * BDI_CAP_READ_MAP:       Can be mapped for reading
 * BDI_CAP_WRITE_MAP:      Can be mapped for writing
 * BDI_CAP_EXEC_MAP:       Can be mapped for execution
 *
 * BDI_CAP_SWAP_BACKED:    Count shmem/tmpfs objects as swap-backed.
 *
 * BDI_CAP_STRICTLIMIT:    Keep number of dirty pages below bdi threshold.
 */
#define BDI_CAP_NO_ACCT_DIRTY	0x00000001
#define BDI_CAP_NO_WRITEBACK	0x00000002 // 2015-05-09;
#define BDI_CAP_MAP_COPY	0x00000004
#define BDI_CAP_MAP_DIRECT	0x00000008
#define BDI_CAP_READ_MAP	0x00000010
#define BDI_CAP_WRITE_MAP	0x00000020
#define BDI_CAP_EXEC_MAP	0x00000040
#define BDI_CAP_NO_ACCT_WB	0x00000080
#define BDI_CAP_SWAP_BACKED	0x00000100 
#define BDI_CAP_STABLE_WRITES	0x00000200
#define BDI_CAP_STRICTLIMIT	0x00000400

#define BDI_CAP_VMFLAGS \
	(BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP | BDI_CAP_EXEC_MAP)

// 2015-07-04;
// 2017-06-24
#define BDI_CAP_NO_ACCT_AND_WRITEBACK \
	(BDI_CAP_NO_WRITEBACK | BDI_CAP_NO_ACCT_DIRTY | BDI_CAP_NO_ACCT_WB)

#if defined(VM_MAYREAD) && \
	(BDI_CAP_READ_MAP != VM_MAYREAD || \
	 BDI_CAP_WRITE_MAP != VM_MAYWRITE || \
	 BDI_CAP_EXEC_MAP != VM_MAYEXEC)
#error please change backing_dev_info::capabilities flags
#endif

extern struct backing_dev_info default_backing_dev_info;
extern struct backing_dev_info noop_backing_dev_info;

int writeback_in_progress(struct backing_dev_info *bdi);

// 2015-12-05
// bdi_congested(bdi, 1 << BDI_async_congested)
// 2015-12-19;
static inline int bdi_congested(struct backing_dev_info *bdi, int bdi_bits)
{
	if (bdi->congested_fn)
        // 2015-12-05 함수포인터가 NULL이 아니라면 함수호출
        // 함수포인터에 연결된 함수가 블록디바이스라 호출한다로 마무리함
		return bdi->congested_fn(bdi->congested_data, bdi_bits);
	return (bdi->state & bdi_bits);
}

static inline int bdi_read_congested(struct backing_dev_info *bdi)
{
	return bdi_congested(bdi, 1 << BDI_sync_congested);
}

// 2015-12-05
// bdi_write_congested(mapping->backing_dev_info)
// 2015-12-19;
static inline int bdi_write_congested(struct backing_dev_info *bdi)
{
	return bdi_congested(bdi, 1 << BDI_async_congested);
}

static inline int bdi_rw_congested(struct backing_dev_info *bdi)
{
	return bdi_congested(bdi, (1 << BDI_sync_congested) |
				  (1 << BDI_async_congested));
}

enum {
	BLK_RW_ASYNC	= 0,
	BLK_RW_SYNC	= 1,
};

void clear_bdi_congested(struct backing_dev_info *bdi, int sync);
void set_bdi_congested(struct backing_dev_info *bdi, int sync);
long congestion_wait(int sync, long timeout);
long wait_iff_congested(struct zone *zone, int sync, long timeout);
int pdflush_proc_obsolete(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos);

static inline bool bdi_cap_stable_pages_required(struct backing_dev_info *bdi)
{
	return bdi->capabilities & BDI_CAP_STABLE_WRITES;
}

// 2015-09-05;
// 2015-12-26
static inline bool bdi_cap_writeback_dirty(struct backing_dev_info *bdi)
{
	return !(bdi->capabilities & BDI_CAP_NO_WRITEBACK/* Don't writeback page */);
}

// 2015-08-15
// 0이면 true 리턴
//
// 2015-09-05;
static inline bool bdi_cap_account_dirty(struct backing_dev_info *bdi)
{
	return !(bdi->capabilities & BDI_CAP_NO_ACCT_DIRTY);
}

static inline bool bdi_cap_account_writeback(struct backing_dev_info *bdi)
{
	/* Paranoia: BDI_CAP_NO_WRITEBACK implies BDI_CAP_NO_ACCT_WB */
	return !(bdi->capabilities & (BDI_CAP_NO_ACCT_WB |
				      BDI_CAP_NO_WRITEBACK));
}

static inline bool bdi_cap_swap_backed(struct backing_dev_info *bdi)
{
	return bdi->capabilities & BDI_CAP_SWAP_BACKED;
}

static inline bool mapping_cap_writeback_dirty(struct address_space *mapping)
{
	return bdi_cap_writeback_dirty(mapping->backing_dev_info);
}

// 2015-08-15
// 2015-09-05;
// 2015-12-26
static inline bool mapping_cap_account_dirty(struct address_space *mapping)
{
	return bdi_cap_account_dirty(mapping->backing_dev_info);
}

static inline bool mapping_cap_swap_backed(struct address_space *mapping)
{
	return bdi_cap_swap_backed(mapping->backing_dev_info);
}

static inline int bdi_sched_wait(void *word)
{
	schedule();
	return 0;
}

#endif		/* _LINUX_BACKING_DEV_H */
