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
    
	int ret;
	if (offset>NBLOCKS || offset<0) return -EINVAL;
	
	AUDIT {
		printk("%s: invocation of sys_invalidate_data\n",MODNAME);
		printk("block to invalidate is: %d\n", offset);
	}


	// call the specific function in device driver
	do{
		ret = dev_invalidate_data(offset);
	} while (ret == -EAGAIN);
	
	return ret;
}




#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char*, source, size_t, size){
#else
asmlinkage int sys_put_data(char* source, size_t size){
#endif

	int ret;
	int len;
	char* message;

    if(size > DATA_SIZE) return -EINVAL;
	
    // dynamic allocation of area to contain the message
	message = kzalloc(DATA_SIZE, GFP_KERNEL);
    if (!message){
    	printk("%s: kmalloc error, unable to allocate memory for receiving buffer in ioctl\n\n",MODNAME);
        return -ENOMEM;
    }

	// user is allowed to write only size bytes
    ret = copy_from_user(message, source, size);
	len = strlen(message);
	if(len<size) size = len;
    message[size] = '\0';

    printk("message: %s, len: %lu\n", message, size+1);


	// call the specific function in device driver
	do {
		ret = dev_put_data(message, size + 1);				// the insertion must include '/0'	
	} while (ret == -EAGAIN);
	
	return ret;
}





#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _get_data, int, offset, char*, destination, size_t, size){
#else
asmlinkage int sys_get_data(int offset, char* destination, size_t size){
#endif

	int ret;
	if(size > DATA_SIZE) size = DATA_SIZE;
	else if(size < 0 || offset>NBLOCKS || offset<0) return -EINVAL;

	AUDIT{
		printk("%s: invocation of sys_get_data\n",MODNAME);
		printk("destination: %px, len: %lu, block: %d\n", destination, size, offset);
	}
	
	// call the specific function in device driver
	ret = dev_get_data(offset, destination, size);
	return ret;
}





// dopo le macro vengono create due funzioni con nomi differenti da sys_...

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;       
long sys_put_data = (unsigned long) __x64_sys_put_data;
long sys_get_data = (unsigned long) __x64_sys_get_data;
#else
#endif