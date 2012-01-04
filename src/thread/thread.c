/*	ChickenOS - thread.c
 *  Threading system
 *
 */
#include <common.h>
#include <types.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/interrupt.h>
#include <thread/tss.h>
#define STACK_SIZE 0x1000
#define STACK_PAGES STACK_SIZE/PAGE_SIZE

thread_t *kernel_thread;

LIST_HEAD(all_list);

pid_t pid_allocate();

char *_main_thread_name = "main";
void thread_init()
{
	asm volatile("cli");

	kernel_thread = thread_current();
	
	kernel_thread->pid = 0;
	kernel_thread->parent = 0;
//	kernel_thread->sp = (uint8_t *)((uint32_t)kernel_thread + 4096);//0xfeeddEEF;
	kernel_thread->pd = pagedir_new();
	kernel_thread->name = _main_thread_name;
	kernel_thread->status = THREAD_RUNNING;
	kernel_thread->magic = 0xfeedface;

	INIT_LIST_HEAD(&kernel_thread->list);	
	list_add_tail(&kernel_thread->all_list,&all_list);

	tss_init();
}

thread_t * thread_current()
{
	thread_t *tmp; 
	asm ("mov %%esp, %0": "=m"(tmp) );

	return (thread_t *) ((uint32_t)tmp & ~(STACK_SIZE -1));
}

/* throw an int 32, manually invoking the timer interrupt */
void thread_yield()
{
	asm volatile("int $32");
}


void thread_exit()
{
	asm volatile("cli");
	thread_t *cur = thread_current();
	//keep a tmp pointer to next process
	//which the scheduler uses to get the next process
	//need to set a value in the thread_t to tell
	//the scheduler to do this
	struct list_head *tmp = cur->list.next;
	list_del(&cur->list);
	cur->list.next = tmp;
	//if we aren't the original kernel thread, free the stack
	if(cur->pid == 0)
	{
		//FIXME: This needs to be done differently
//		pallocn_free(cur, STACK_PAGES);	

	}
	asm volatile("sti");
	thread_yield();

}
uint32_t thread_get_sp()
{
	uint32_t cur_sp;	
	asm ("mov %%esp, %0": "=m"(cur_sp));

	return cur_sp;
}


extern uint32_t get_eip();
extern void pagedir_insert_page(pagedir_t pd, 
	virt_addr_t phys, virt_addr_t virt,uint8_t flags);
void thread_usermode(void)
{
	uint32_t cur_esp,new_esp;
	thread_t *cur = thread_current();

	asm volatile("cli");

	void *new = palloc();

	kernel_thread = new;

	void* esp=(void *)( (uintptr_t)new + 4096);//thread_get_sp();

	kmemcpy(new, thread_current(),PAGE_SIZE);

	//puts new kernel stack in tss
	tss_update((uintptr_t)esp);

	printf("Entering user mode\n");

	console_set_color(BLACK,WHITE);
	
	cur->sp = esp - sizeof(registers_t) -4;

	thread_t *newt = new;

	pagedir_insert_page(newt->pd, (uintptr_t)cur, 
			(uintptr_t)PHYS_BASE - 0x1000, 0x7);

	INIT_LIST_HEAD(&newt->list);

	asm volatile("mov %%esp, %0":"=m"(cur_esp));	

	new_esp = (cur_esp & 0xfff) + (PHYS_BASE - 0x1000);

	asm volatile(
				"cli\n"
				"mov $0x23, %%ax\n"
				"mov %%ax, %%ds\n" 
				"mov %%ax, %%es\n" 
				"mov %%ax, %%fs\n" 
				"mov %%ax, %%gs\n" 
				"mov %0, %%eax\n"
				"push $0x23\n"
				"pushl %%eax\n"
				"push $0x200\n"
				"pushl $0x1b\n"
				"push $1f\n"
				"iret\n"
				"1:"	
				::
				"m"(new_esp)
				);
}

bool
in_kernel(void)
{
	uint16_t ss;	
	asm volatile ("mov %%ss, %0\n" : "=r"(ss));
	if((ss & 3) == 0)
		return true;

	return false;
}


thread_t * 
thread_create(uint32_t eip, uint32_t esp)
{
	thread_t *new, *cur;
//	interrupt_disable();
	registers_t *reg_frame;
	uint8_t *kernel_stack, *user_stack;
	uintptr_t new_sp;
	
	kernel_stack = pallocn(STACK_PAGES);
	user_stack = palloc();

	cur = thread_current();
	
	new = (thread_t *)kernel_stack;	
	new_sp = (uintptr_t)kernel_stack + 4096;;

	kmemsetl((uint32_t*)user_stack, 0, 1024);
	kmemsetl((uint32_t*)kernel_stack,0, 1024);	

	new->pd = pagedir_new();
	kmemcpy(user_stack, (void *)(PHYS_BASE - 0x1000), 0x1000);	
	pagedir_insert_page(new->pd, (uintptr_t)user_stack, 
		(uintptr_t)PHYS_BASE - 0x1000, 0x7);
		
	new->parent = cur->pid;
	new->cur_dir = cur->cur_dir;	
	new->magic = 0xcafedeed;
	new->pid = pid_allocate();
			
	reg_frame = (void *)(kernel_stack + 4096);
	reg_frame--;

	new->regs = (struct registers *)reg_frame;
	reg_frame->eip = eip;
	
//	uint32_t ebp;			
//	asm volatile ("mov %%ebp, %0\n" :"=m"(ebp));
//	reg_frame->ebp = ebp;	

	reg_frame->cs = 0x1b;
	reg_frame->ds = reg_frame->es = reg_frame->fs = 
		reg_frame->gs = reg_frame->ss = 0x23;
	reg_frame->eax = 0;	
	reg_frame->eflags = 0x200;

	reg_frame->useresp = esp;
	reg_frame->esp = new_sp - 14*4;	
		
	new->sp = (uint8_t *)(new_sp - (sizeof(registers_t) + 4));
		
	list_add_tail(&new->all_list,&all_list);
	list_add_tail(&new->list,&cur->list);
	
//	interrupt_set(old_level);	
	return new;//new->pid;	

}

pid_t sys_getpid()
{
	thread_t *cur = thread_current();
	pid_t pid = cur->pid;
	printf("cur %X pid = %i\n",cur,pid);
	return pid;
}

pid_t pid_allocate()
{
	static pid_t pid_count = 0;

	pid_count++;

	return pid_count;
}
pid_t sys_fork(registers_t *regs)
{

	thread_t *new;
	uintptr_t new_useresp = (PHYS_BASE - 4096) + (regs->useresp & 0xfff);
	new = thread_create(regs->eip, new_useresp);
	
/*	thread_t *new, *cur;
//	interrupt_disable();
	registers_t *reg_frame;
	uint8_t *kernel_stack, *user_stack;
	uintptr_t new_sp;
	
	kernel_stack = pallocn(STACK_PAGES);
	user_stack = palloc();

	cur = thread_current();
	
	new = (thread_t *)kernel_stack;	
	new_sp = (uintptr_t)kernel_stack + 4096;;

	kmemsetl((uint32_t*)user_stack, 0, 1024);
	kmemsetl((uint32_t*)kernel_stack,0, 1024);	

	new->pd = pagedir_new();
	kmemcpy(user_stack, (void *)(PHYS_BASE - 0x1000), 0x1000);	
	pagedir_insert_page(new->pd, (uintptr_t)user_stack, 
		(uintptr_t)PHYS_BASE - 0x1000, 0x7);
		
	new->parent = cur->pid;
	new->cur_dir = cur->cur_dir;	
	new->magic = 0xcafedeed;
	new->pid = pid_allocate();
			
	reg_frame = (void *)new_sp - sizeof(*reg_frame);//(kernel_stack + 4096);
//	reg_frame--;

	new->regs = (struct registers *)reg_frame;
	reg_frame->eip = regs->eip;
	
//	uint32_t ebp;			
//	asm volatile ("mov %%ebp, %0\n" :"=m"(ebp));
//	reg_frame->ebp = ebp;	

	reg_frame->cs = 0x1b;
	reg_frame->ds = reg_frame->es = reg_frame->fs = 
		reg_frame->gs = reg_frame->ss = 0x23;
	reg_frame->eax = 0;	
	reg_frame->eflags = 0x200;
	reg_frame->useresp = PHYS_BASE - 4096 + (regs->useresp & 0xfff);
	printf("useresp %x newesp %x\n", regs->useresp, reg_frame->useresp);
	reg_frame->esp = new_sp - 14*4;	
		
	new->sp = (uint8_t *)(new_sp - (sizeof(registers_t) + 4));
		
	list_add_tail(&new->all_list,&all_list);
	list_add_tail(&new->list,&cur->list);
*/	
	return new->pid;	
}

