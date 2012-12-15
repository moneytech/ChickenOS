#include <common.h>
#include <kernel/hw.h>
#include <kernel/interrupt.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <device/pci.h>
#include <device/usb.h>
#include <device/video.h>
#include <device/audio.h>
#include <device/serial.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/initrd.h>
#include <net/net_core.h>
#include <stdio.h>
#include <string.h>
#include <thread/syscall.h>

char *BOOT_MSG = "ChickenOS v0.02 booting\n";

extern void print_mb(unsigned long addr, unsigned long magic);


void modules_init(struct multiboot_info *mb)
{
	extern uint32_t *background_image;
	
	if(mb->mods_count > 0 )
	{
		background_image  = (uint32_t *)P2V(*((void**)P2V(mb->mods_addr)));
	//	uintptr_t end  = (uintptr_t)*((void **)P2V(mb->mods_addr + 4));
	//	initrd_init(P2V(start),P2V(end));
	}
}

void kmain(uint32_t mbd, uint32_t magic)
{
	struct multiboot_info *mb = 0;
   	if ( magic != 0x2BADB002 )
   	{
	//	console_puts("Bad magic number, halting\r");
		return;
   	}
 	
	mb = (struct multiboot_info *)P2V(mbd);
	
	/* begin initializations */
	modules_init(mb);
	vm_init(mb);
	thread_init();
	interrupt_init();
//	console_init();
	pci_init();
	video_init();
	//we start out with one color scheme
	//but this will be changed if i ever get a framebuffer
	//console working
//	console_set_color(BLUE,WHITE);
	console_puts(BOOT_MSG);
	audio_init();
	extern void pci_list();
	pci_list();	

	kbd_init();	
	time_init();
	syscall_init();
	
	interrupt_enable();
	
	usb_init();
	serial_init();

	if(1)
	{
		modules_init(mb);	
		vfs_init();
		//need to move this back to console.c	
		console_fs_init();
		extern void ata_init();
		ata_init();	
		vfs_mount_root(ATA0_0_DEV, "ext2");

	}
	
//	network_init();
	
	thread_usermode();
	
	//we have this special sycall at the moment
	//to setup networking, later will be able to handle
	//it in user space	
//	network_setup();


	char *argv[] = {"dash", NULL};
	if(!fork() )
	{
		execv("/init", argv);
		PANIC("execv(init) failed!");
	}
	//only works because initial threads name is "main"
	strcpy(thread_current()->name, "idle");
	while(1);

	//should never return, unless things get really fucked
	PANIC("kmain returned");
}

