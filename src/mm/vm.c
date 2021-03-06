/*	ChickenOS - mm/vm.c
 *	Handles paging, gdt, and page allocation
 *  Heap allocation is implemented with liballoc
 *  which uses pages from the page allocator
 */
#include <common.h>
#include <mm/vm.h>
#include <kernel/hw.h>
#include <kernel/thread.h>
#include <stdio.h>
#include <multiboot.h>

uint32_t mem_size;

void vm_page_fault_dump(registers_t *regs, uintptr_t addr, int flags)
{
	thread_t *cur = thread_current();
	printf("Page fault in %s space @ %X PID %i eip %x\n",
			(flags & PAGE_USER) ? "user" : "kernel",
			addr, cur->pid, regs->eip);
	printf("%s\t", (flags & PAGE_WRITE) ? "write" : "read");
	printf("%s\n", (flags & PAGE_VIOLATION) ? "protection violation" : "page not present");
	printf("\nREGS:\n");
	dump_regs(regs);
	printf("\n");
}

void vm_page_fault(registers_t *regs, uintptr_t addr, int flags)
{
	thread_t *cur = thread_current();
	enum intr_status status = interrupt_disable();

	//vm_page_fault_dump(regs, addr, flags);
	//TODO: Check if this is a swapped out or mmaped or COW etc
	if(memregion_fault(cur->mm, addr, flags) == 0)
		return;

	if(flags & PAGE_USER)
	{
		vm_page_fault_dump(regs, addr, flags);

		//TODO: send sigsegv to thread
		//signal(cur, SIGSEGV);
		thread_current()->status = THREAD_DEAD;
		interrupt_set(status);

		//FIXME: should reschedule here instead of busy waiting
		//		I think the best bet is to return, and have checks to see
		//		if thread died before returning
		printf("Page fault in user space\n");
		while(1)
			;

		thread_yield();
		thread_exit(1);
	}
	else
	{
		vm_page_fault_dump(regs, addr, flags);
		PANIC("Page fault in kernel space!");
	}

	PANIC("Unhandled page fault");
}

//FIXME Taken from linux, only applicable to i386?
#define TASK_UNMAPPED_BASE (PHYS_BASE/3)

void *mmap_base = (void *)0x5000000;
//Move this to the region code?
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset)
{
	(void)addr;
	(void)length;
	(void)prot;
	(void)flags;
	(void)fd;
	(void)pgoffset;
	PANIC("FUCKING THING SUCKS\n");
	printf("Addr %p, length %x prot %x flags %x fd %i pgoffset %i\n",
					addr, length, prot, flags, fd, pgoffset);

	if(addr == NULL)
	{
		//Starting with mmap base
		//look through threads memregions
		//if
		addr = mmap_base;

	}
//	void *new = palloc(length / PAGE_SIZE);
//	pagedir_t pd = thread_current()->pd;
//	pagedir_insert_pagen(pd, (uintptr_t)new, (uintptr_t)mmap_base, 0x7, length/PAGE_SIZE);

	addr = mmap_base;
	mmap_base += length;

	return addr;//(void*)-1;//NULL;
}

struct mm *mm_alloc()
{
	struct mm *new = kcalloc(sizeof(*new), 1);

	new->pd = pagedir_alloc();

	return new;
}

struct mm *mm_clone(struct mm *old)
{
	struct mm *new = kcalloc(sizeof(*new), 1);
	new->pd = pagedir_clone(old->pd);
	new->regions = region_clone(old->regions);

	return new;
}

void mm_clear(struct mm *mm)
{
	(void)mm;
	//Iterate through regions and remove them
	//	reduce reference counts for physical pages
	//swap to a new pagedirectory and throw away everything else
	//pagedir_free(mm->pd);
}

void mm_free(struct mm *mm)
{
	(void)mm;
}

//Currently used in execve()
void mm_init(struct mm *mm)
{
	//This initial allocation is probably not needed,
	//but saves a page fault on initial stack setup
	void *user_stack = palloc();
	memset(user_stack, 0, 4096);

	//XXX: This is wrong, we should unmap everything mapped using regions,
	//instead of just blanking the regions and adding a new pagedirectory
	mm->pd = pagedir_alloc();
	mm->regions = NULL;

	memregion_map_data(mm, PHYS_BASE - PAGE_SIZE, PAGE_SIZE,
			PROT_GROWSDOWN, MAP_GROWSDOWN | MAP_FIXED, user_stack);
	memregion_map_data(mm, HEAP_BASE, PAGE_SIZE,
			PROT_GROWSUP, MAP_PRIVATE | MAP_FIXED, NULL);
}

void vm_init(struct kernel_boot_info *info)
{
	uint32_t page_count = info->mem_size/PAGE_SIZE;
	mem_size = info->mem_size;

	//palloc_init should take the info struct
	palloc_init(page_count, (uintptr_t)info->placement);
	//paging_init should take the info struct
	paging_init(mem_size);

	//frame_init should take the info struct
	frame_init(info->mem_size);
}
