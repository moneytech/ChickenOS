#ifndef C_OS_MM_VM_H
#define C_OS_MM_VM_H
#include <common.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <kernel/interrupt.h>
#include <chicken/boot.h>
typedef uintptr_t phys_addr_t;
typedef uintptr_t virt_addr_t;

#include <mm/paging.h>

#define PAGE_SIZE 4096
#define PAGE_MASK 0xFFFFF000
#define PAGE_OFFSET (~PAGE_MASK)
#define PAGE_SHIFT 12

#define PAGE_COUNT(len) (((len) / PAGE_SIZE) + ((len) & PAGE_OFFSET ? 1 : 0))

#define PD_SIZE   4096

#define PAGE_VIOLATION 0x01
#define PAGE_WRITE	   0x02
#define PAGE_USER	   0x04

//#define PTE_PRESENT 0x00000001

#define KERNEL_SEG 0x10


#define PHYS_BASE 0xC0000000
#define HEAP_BASE 0x09000000
#define V2P(p) ((void *)((char *)(p) - PHYS_BASE))
#define P2V(p) ((void *)((char *)(p) + PHYS_BASE))

//If we implement joining memregions, we just decrease refcounts on
struct memregion {
	uintptr_t addr_start, addr_end;
	uintptr_t requested_start, requested_end;
	int pages;
	size_t len;
	//if cow and page fault write, we memcpy and insert new physical page
	//on region deallocation, if cow is still set we do nothing, otherwise
	//palloc_free()
	//TODO consolidate these into flags, writing them out now
	atomic_int ref_count;

	int present;
	int cow;
	int flags;
	int prot;

	struct file *file;
	struct inode *inode;
	off_t file_offset;
	size_t file_size;

	struct memregion *next;
	struct memregion *prev;
};

struct mm {
	void *pd;
	struct memregion *regions;
	//Tree here
	void * brk;
	uintptr_t sbrk;
};

struct frame {
	union {
		uintptr_t phys_addr;
		void *phys_ptr;
	};
	union {
		uintptr_t virt_addr;
		void *virt_ptr;
	};
	atomic_int ref_count;
} __attribute__((packed));

/* mm/vm.c */
void vm_init(struct kernel_boot_info *info);
void vm_page_fault(registers_t *regs, uintptr_t addr, int flags);
struct mm *mm_alloc();
void mm_init(struct mm *mm);
struct mm *mm_clone(struct mm *old);
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset);

/* mm/regions.c */
struct memregion *region_clone(struct memregion *original);
//int memregion_add(struct mm *mm, uintptr_t address, size_t len, int prot,
//						int flags, struct inode *inode, off_t offset, size_t file_len, void *data);
int memregion_fault(struct mm *mm, uintptr_t address, int prot);
int memregion_map_data(struct mm *mm, uintptr_t address, size_t len, int prot, int flags,
		void *data);
int memregion_map_file(struct mm *mm, uintptr_t address, size_t len, int prot,
		int flags, struct inode *inode, off_t offset, size_t size);
//struct memregion *memregion_new();
void mm_clear(struct mm *mm);
int verify_pointer(const void *ptr, size_t len, int rw);

/* mm/frame.c */
void frame_init(uintptr_t mem_size);
struct frame *frame_get(void *ptr);
void frame_put(struct frame *frame);
void *palloc_user();

/* mm/palloc.c */
void palloc_init(uint32_t page_count, uintptr_t placement);
void *pallocn(uint32_t count);
void *palloc();
void *palloc_len(size_t len);
void palloc_free(void *addr);
int  pallocn_free(void *addr, int pages);
#endif
