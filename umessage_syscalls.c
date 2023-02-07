#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/io.h>
#include <linux/syscalls.h>
// #include "umessage_header.h"


// DEFINIZIONI UTILI

unsigned long the_ni_syscall;
unsigned long new_sys_call_array[] = {0x0,0x0,0x0}; //to initialize at startup

#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};




// SYSTEM CALLS

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, int, offset){
#else
asmlinkage int sys_invalidate_data(int offset){
#endif
       
	AUDIT
	printk("%s: invocation of invalidate_data with offset=%d\n",MODNAME, offset);
	return 0;
}




#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char*, source, size_t, size){
#else
asmlinkage int sys_put_data(char* source, size_t size){
#endif
        

        int ret;
	char* user_message;		//char user_message[128];
	user_message = (char*) kmalloc(size+1, GFP_KERNEL);
	if(user_message == NULL){
		printk("%s: kmalloc error, null returned\n",MODNAME);
		return 0;
	}
	ret = copy_from_user(user_message, source, size);
	user_message[size]='\0';
	
       
	AUDIT
	printk("%s: invocation of put_data with data=%s of size=%ld\n",MODNAME, user_message, size);
	return 0;
}




#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _get_data, int, offset, char*, destination, size_t, size){
#else
asmlinkage int sys_get_data(int offset, char* destination, size_t size){
#endif


	AUDIT
	printk("%s: invocation of get_data with offset=%d, destination=%px, size=%ld\n",MODNAME, offset, destination, size);
	return 0;
}





// dopo le macro vengono create due funzioni con nomi differenti da sys_...

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;       
long sys_put_data = (unsigned long) __x64_sys_put_data;
long sys_get_data = (unsigned long) __x64_sys_get_data;
#else
#endif