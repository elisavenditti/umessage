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
#include "lib/include/scth.h"
#include "my_header.h"
#include "device_driver.c"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elisa Venditti <elisavenditti99@gmail.com>");
MODULE_DESCRIPTION("block level message mantainance");



// DEFINIZIONE DEL PARAMETRO DEL MODULO: l'indirizzo della system call table

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


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





// FUNZIONI DI STARTUP E SHUTDOWN DEL MODULO

int init_module(void) {

        int i;
        int ret;

	AUDIT{
	   printk("%s: received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
     	   printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
	}


        // inizializzazione delle system call nella tabella

	new_sys_call_array[0] = (unsigned long)sys_invalidate_data;
	new_sys_call_array[1] = (unsigned long)sys_put_data;
	new_sys_call_array[2] = (unsigned long)sys_get_data;

        ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);
        if (ret != HACKED_ENTRIES){
                printk("%s: could not hack %d entries (just %d)\n",MODNAME,HACKED_ENTRIES,ret); 
                return -1;      
        }

	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
        }
	protect_memory();

        printk("%s: all new system-calls correctly installed on sys-call table\n",MODNAME);



        // inizializzazione del device driver

        Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	if (Major < 0) {
	        printk("%s: registering device failed\n",MODNAME);
	        return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

        return 0;

}

void cleanup_module(void) {

        int i;
                
        printk("%s: shutting down\n",MODNAME);

        // restore della system call table
	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
        }
	protect_memory();
        printk("%s: sys-call table restored to its original content\n",MODNAME);

        // eliminazione del device driver
        unregister_chrdev(Major, DEVICE_NAME);
	printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);

	return;
        
}