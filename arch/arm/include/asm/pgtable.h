/*
 *  arch/arm/include/asm/pgtable.h
 *
 *  Copyright (C) 1995-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASMARM_PGTABLE_H
#define _ASMARM_PGTABLE_H

#include <linux/const.h>
#include <asm/proc-fns.h>

#ifndef CONFIG_MMU

#include <asm-generic/4level-fixup.h>
#include <asm/pgtable-nommu.h>

#else

#include <asm-generic/pgtable-nopud.h>
#include <asm/memory.h>
#include <asm/pgtable-hwdef.h>


#include <asm/tlbflush.h>

#ifdef CONFIG_ARM_LPAE
#include <asm/pgtable-3level.h>
#else
#include <asm/pgtable-2level.h>
#endif

/*
 * Just any arbitrary offset to the start of the vmalloc VM area: the
 * current 8MB value just means that there will be a 8MB "hole" after the
 * physical memory until the kernel virtual memory starts.  That means that
 * any out-of-bounds emory accesses will hopefully be caught.
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 */
#define VMALLOC_OFFSET		(8*1024*1024) // 0x0080_0000, 8MB
// sanity_check_meminfo() 함수에서 high_memory에 값을 저장
// 2015-09-19;
#define VMALLOC_START		(((unsigned long)high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
// 2016-06-25
#define VMALLOC_END		0xff000000UL

#define LIBRARY_TEXT_START	0x0c000000

#ifndef __ASSEMBLY__
extern void __pte_error(const char *file, int line, pte_t);
extern void __pmd_error(const char *file, int line, pmd_t);
extern void __pgd_error(const char *file, int line, pgd_t);

#define pte_ERROR(pte)		__pte_error(__FILE__, __LINE__, pte)
#define pmd_ERROR(pmd)		__pmd_error(__FILE__, __LINE__, pmd)
#define pgd_ERROR(pgd)		__pgd_error(__FILE__, __LINE__, pgd)

/*
 * This is the lowest virtual address we can permit any user space
 * mapping to be mapped at.  This is particularly important for
 * non-high vector CPUs.
 */
#define FIRST_USER_ADDRESS	(PAGE_SIZE * 2)

/*
 * Use TASK_SIZE as the ceiling argument for free_pgtables() and
 * free_pgd_range() to avoid freeing the modules pmd when LPAE is enabled (pmd
 * page shared between user and kernel).
 */
#ifdef CONFIG_ARM_LPAE
#define USER_PGTABLES_CEILING	TASK_SIZE
#endif

/*
 * The pgprot_* and protection_map entries will be fixed up in runtime
 * to include the cachable and bufferable bits based on memory policy,
 * as well as any architecture dependent bits like global/ASID and SMP
 * shared mapping bits.
 */
#define _L_PTE_DEFAULT	L_PTE_PRESENT | L_PTE_YOUNG

extern pgprot_t		pgprot_user;
extern pgprot_t		pgprot_kernel;
extern pgprot_t		pgprot_hyp_device;
extern pgprot_t		pgprot_s2;
extern pgprot_t		pgprot_s2_device;

// 2015-01-31, 
// _MOD_PROT(pgprot_kernel, L_PTE_XN)
#define _MOD_PROT(p, b)	__pgprot(pgprot_val(p) | (b))

#define PAGE_NONE		_MOD_PROT(pgprot_user, L_PTE_XN | L_PTE_RDONLY | L_PTE_NONE)
#define PAGE_SHARED		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_XN)
#define PAGE_SHARED_EXEC	_MOD_PROT(pgprot_user, L_PTE_USER)
#define PAGE_COPY		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define PAGE_COPY_EXEC		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY)
#define PAGE_READONLY		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define PAGE_READONLY_EXEC	_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY)
#define PAGE_KERNEL		_MOD_PROT(pgprot_kernel, L_PTE_XN)
#define PAGE_KERNEL_EXEC	pgprot_kernel
#define PAGE_HYP		_MOD_PROT(pgprot_kernel, L_PTE_HYP)
#define PAGE_HYP_DEVICE		_MOD_PROT(pgprot_hyp_device, L_PTE_HYP)
#define PAGE_S2			_MOD_PROT(pgprot_s2, L_PTE_S2_RDONLY)
#define PAGE_S2_DEVICE		_MOD_PROT(pgprot_s2_device, L_PTE_S2_RDWR)

#define __PAGE_NONE		__pgprot(_L_PTE_DEFAULT | L_PTE_RDONLY | L_PTE_XN | L_PTE_NONE)
#define __PAGE_SHARED		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_XN)
#define __PAGE_SHARED_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER)
#define __PAGE_COPY		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_COPY_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY)
#define __PAGE_READONLY		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_READONLY_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY)

#define __pgprot_modify(prot,mask,bits)		\
	__pgprot((pgprot_val(prot) & ~(mask)) | (bits))

#define pgprot_noncached(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_UNCACHED)

#define pgprot_writecombine(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_BUFFERABLE)

#define pgprot_stronglyordered(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_UNCACHED)

#ifdef CONFIG_ARM_DMA_MEM_BUFFERABLE
#define pgprot_dmacoherent(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_BUFFERABLE | L_PTE_XN)
#define __HAVE_PHYS_MEM_ACCESS_PROT
struct file;
extern pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
				     unsigned long size, pgprot_t vma_prot);
#else
#define pgprot_dmacoherent(prot) \
	__pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_UNCACHED | L_PTE_XN)
#endif

#endif /* __ASSEMBLY__ */

/*
 * The table below defines the page protection levels that we insert into our
 * Linux page table version.  These get translated into the best that the
 * architecture can perform.  Note that on most ARM hardware:
 *  1) We cannot do execute protection
 *  2) If we could do execute protection, then read is implied
 *  3) write implies read permissions
 */
#define __P000  __PAGE_NONE
#define __P001  __PAGE_READONLY
#define __P010  __PAGE_COPY
#define __P011  __PAGE_COPY
#define __P100  __PAGE_READONLY_EXEC
#define __P101  __PAGE_READONLY_EXEC
#define __P110  __PAGE_COPY_EXEC
#define __P111  __PAGE_COPY_EXEC

#define __S000  __PAGE_NONE
#define __S001  __PAGE_READONLY
#define __S010  __PAGE_SHARED
#define __S011  __PAGE_SHARED
#define __S100  __PAGE_READONLY_EXEC
#define __S101  __PAGE_READONLY_EXEC
#define __S110  __PAGE_SHARED_EXEC
#define __S111  __PAGE_SHARED_EXEC

#ifndef __ASSEMBLY__
/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
// 2015-10-17
extern struct page *empty_zero_page;
#define ZERO_PAGE(vaddr)	(empty_zero_page)

// PTRS_PER_PGD 현 시스템에서는 2048
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

/* to find an entry in a page-table-directory */
// PGDIR_SHIFT : global page 에 해당하는 값을 추출하기 위한 값
// 현재 구조에서는 2단계 페이징 시스템을 사용하기 때문에 해당 값은 21
// pgd size는 2MB이기 때문에 2MB만큼 shift한것.
// http://www.iamroot.org/xe/Kernel_10_ARM/176798
// 2015-08-22
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT/*21*/)

// mm->pgd == swapper_pg_dir
// == &swapper_pg_dir[pgd_index(addr)]
// 
// 2015-08-22
// 2016-04-09
// 왠지.. 함수의 네이밍이 잘못된 것 같다.
// 실제 얻을 수 있는 값은 오프셋이 아니라 실제 접근 가능한 메모리 주소
#define pgd_offset(mm, addr)	((mm)->pgd + pgd_index(addr))

/* to find an entry in a kernel page-table-directory */
// 2015-09-19;
// 2017-08-12
// pgd_offset_k(0);
#define pgd_offset_k(addr)	pgd_offset(&init_mm, addr)

// return (pmd == 0)
// 2016-04-16
#define pmd_none(pmd)		(!pmd_val(pmd))
// 2015-08-22
#define pmd_present(pmd)	(pmd_val(pmd))

// 2015-08-22
// 가상주소를 pte_t형으로 리턴
// 2016-04-16
static inline pte_t *pmd_page_vaddr(pmd_t pmd)
{
	// pmd.pmd 값을 PAGE_MASK 단위로 ALIGN
	// (S32) -> 32비트 머신인가, 64비트 머신인가에 따라 
	// PAGE_MASK값이 다를 수 있기 때문에
	// 명시적으로 마스크에 대한 값을 표시
	return __va(pmd_val(pmd) & PHYS_MASK/* 0xFFFF_FFFF */ & (s32)PAGE_MASK/* 0xFFFF_F000 */);
}

#define pmd_page(pmd)		pfn_to_page(__phys_to_pfn(pmd_val(pmd) & PHYS_MASK))

#ifndef CONFIG_HIGHPTE  // Not set
// 2015-08-22
// argument가 pointer가 아니라, *(pmd)이다.
#define __pte_map(pmd)		pmd_page_vaddr(*(pmd))
// 2015-08-22
// 2015-09-19;
// 2017-08-12
#define __pte_unmap(pte)	do { } while (0)
#else
#define __pte_map(pmd)		(pte_t *)kmap_atomic(pmd_page(*(pmd)))
// 2017-04-15
#define __pte_unmap(pte)	kunmap_atomic(pte)
#endif

// pte bit(20:12) 값 추출
// 2015-08-22
// 2016-04-16
#define pte_index(addr)		(((addr) >> PAGE_SHIFT/*12*/) & (PTRS_PER_PTE - 1)/*0x01FF*/)
// vaddr -> pmd[0] 4kb align + vaddr[20:12]
// 2016-04-16
// pte_offset_kernel(pmd, address)
#define pte_offset_kernel(pmd,addr)	(pmd_page_vaddr(*(pmd)) + pte_index(addr))

// 2015-08-22
// 2015-10-03
// __pte_map(pmd)는 가상주소 + pte_index(addr)
// 2017-08-12
#define pte_offset_map(pmd,addr)	(__pte_map(pmd) + pte_index(addr))
// 2015-08-22
// 2015-11-07
#define pte_unmap(pte)			__pte_unmap(pte)
// 2015-08-22
// 아래 연산을통해 추론해 보면, pte는 물리주소임을 알 수 있다.
// 2015-10-03
#define pte_pfn(pte)		((pte_val(pte) & PHYS_MASK/* 0xFFFF_FFFF*/) >> PAGE_SHIFT/*12*/)
// 물리 주소와, PTE mask값을 or하면, pte가 된다.
// 2015-10-24;
// 2016-04-16
#define pfn_pte(pfn,prot)	__pte(__pfn_to_phys(pfn) | pgprot_val(prot))

// 2015-09-19;
#define pte_page(pte)		pfn_to_page(pte_pfn(pte))
// 2015-01-31
// 2015-10-24;
// mk_pte(new, vma->vm_page_prot)
// 2016-04-16
// mk_pte(page, prot)
#define mk_pte(page,prot)	pfn_pte(page_to_pfn(page), prot)

// 2015-08-22
// Data cache clear by MVA to PoC, VMSA
// 2015-12-26
// 2017-04-15
#define pte_clear(mm,addr,ptep)	set_pte_ext(ptep, __pte(0), 0)

// 2016-04-16
#define pte_none(pte)		(!pte_val(pte))
// 2015-08-22
// 2015-10-03
#define pte_present(pte)	(pte_val(pte) & L_PTE_PRESENT/*1*/)
// 2015-10-10 쓰기 가능 확인;
// pte_write(pteval);
#define pte_write(pte)		(!(pte_val(pte) & L_PTE_RDONLY/* 1 << 7 */))
// 2015-09-05
// pte_dirty(pteval)
#define pte_dirty(pte)		(pte_val(pte) & L_PTE_DIRTY) // pteval & (pteval_t)1 << 6
// 2015-10-17
#define pte_young(pte)		(pte_val(pte) & L_PTE_YOUNG)
// 2015-10-03
#define pte_exec(pte)		(!(pte_val(pte) & L_PTE_XN/*1<<9*/))
#define pte_special(pte)	(0)

// 2015-10-03
// 2015-10-10;
#define pte_present_user(pte)  (pte_present(pte) && (pte_val(pte) & L_PTE_USER/*1 << 8*/))

#if __LINUX_ARM_ARCH__ < 6
static inline void __sync_icache_dcache(pte_t pteval)
{
}
#else
// 2015-10-03
extern void __sync_icache_dcache(pte_t pteval);
#endif

// 2015-10-03
// 2015-10-10;
// set_pte_at(mm, address, pte, swp_pte);
// 2015-12-26
// 2016-04-16
// set_pte_at(&init_mm, addr, pte, mk_pte(page, prot))
// 2017-08-26
//  set_pte_at(mm, address, ptep, pte_wrprotect(old_pte));
static inline void set_pte_at(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t pteval)
{
	unsigned long ext = 0;

    // addr이 user영역이라면?
	if (addr < TASK_SIZE && pte_present_user(pteval)) {
		__sync_icache_dcache(pteval);
		ext |= PTE_EXT_NG;
	}

	set_pte_ext(ptep, pteval, ext);
}

#define PTE_BIT_FUNC(fn,op) \
static inline pte_t pte_##fn(pte_t pte) { pte_val(pte) op; return pte; }

PTE_BIT_FUNC(wrprotect, |= L_PTE_RDONLY);  // pte_wrprotect();
/*
 * static inline pte_t pte_wrprotect(pte_t pte)
 * {
 *  pte_val(pte) |= L_PTE_RDONLY; 
 *  return pte;
 * }
 */
PTE_BIT_FUNC(mkwrite,   &= ~L_PTE_RDONLY); // pte_mkwrite();
PTE_BIT_FUNC(mkclean,   &= ~L_PTE_DIRTY);  // pte_mkclean();
PTE_BIT_FUNC(mkdirty,   |= L_PTE_DIRTY);   // pte_mkdirty();
PTE_BIT_FUNC(mkold,     &= ~L_PTE_YOUNG);  // pte_mkold();
PTE_BIT_FUNC(mkyoung,   |= L_PTE_YOUNG);   // pte_mkyoung();

static inline pte_t pte_mkspecial(pte_t pte) { return pte; }

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	const pteval_t mask = L_PTE_XN | L_PTE_RDONLY | L_PTE_USER |
		L_PTE_NONE | L_PTE_VALID;
	pte_val(pte) = (pte_val(pte) & ~mask) | (pgprot_val(newprot) & mask);
	return pte;
}

/*
 * Encode and decode a swap entry.  Swap entries are stored in the Linux
 * page tables as follows:
 *
 *   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
 *   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *   <--------------- offset ----------------------> < type -> 0 0 0
 *
 * This gives us up to 31 swap files and 64GB per swap file.  Note that
 * the offset field is always non-zero.
 */
#define __SWP_TYPE_SHIFT	3
#define __SWP_TYPE_BITS		5
#define __SWP_TYPE_MASK		((1 << __SWP_TYPE_BITS) - 1) // 0x1F
#define __SWP_OFFSET_SHIFT	(__SWP_TYPE_BITS/*5*/ + __SWP_TYPE_SHIFT/*3*/) // 8

// 2015-10-24;
#define __swp_type(x)		(((x).val >> __SWP_TYPE_SHIFT/*3*/) & __SWP_TYPE_MASK)
// 2015-10-24;
#define __swp_offset(x)		((x).val >> __SWP_OFFSET_SHIFT/*8*/)
// 2015-10-10;
// __swp_entry(swp_type(entry), swp_offset(entry));
#define __swp_entry(type,offset) ((swp_entry_t) { ((type) << __SWP_TYPE_SHIFT/*3*/) | ((offset) << __SWP_OFFSET_SHIFT/*8*/) })

// 2015-10-24;
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
// 2015-10-10;
#define __swp_entry_to_pte(swp)	((pte_t) { (swp).val })

/*
 * It is an error for the kernel to have more swap files than we can
 * encode in the PTEs.  This ensures that we know when MAX_SWAPFILES
 * is increased beyond what we presently support.
 */
#define MAX_SWAPFILES_CHECK() BUILD_BUG_ON(MAX_SWAPFILES_SHIFT > __SWP_TYPE_BITS)

/*
 * Encode and decode a file entry.  File entries are stored in the Linux
 * page tables as follows:
 *
 *   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
 *   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *   <----------------------- offset ------------------------> 1 0 0
 */
// 2015-10-10;
#define pte_file(pte)		(pte_val(pte) & L_PTE_FILE)
#define pte_to_pgoff(x)		(pte_val(x) >> 3)
// 2015-10-17
// 위의 그림을 참고할 것
#define pgoff_to_pte(x)		__pte(((x) << 3) | L_PTE_FILE)

#define PTE_FILE_MAX_BITS	29

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
/* FIXME: this is not correct */
#define kern_addr_valid(addr)	(1)

#include <asm-generic/pgtable.h>

/*
 * We provide our own arch_get_unmapped_area to cope with VIPT caches.
 */
#define HAVE_ARCH_UNMAPPED_AREA
#define HAVE_ARCH_UNMAPPED_AREA_TOPDOWN

// 2016-05-28
#define pgtable_cache_init() do { } while (0)

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_MMU */

#endif /* _ASMARM_PGTABLE_H */
