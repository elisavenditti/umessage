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
#include "umessage_header.h"
#include "umessage_device_driver.c"
#include "umessage_syscalls.c"
#include "umessage_filesystem/umessagefs_src.c"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elisa Venditti <elisavenditti99@gmail.com>");
MODULE_DESCRIPTION("block level message mantainance");



// DEFINIZIONE DEL PARAMETRO DEL MODULO: l'indirizzo della system call table

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);




// FUNZIONI DI STARTUP E SHUTDOWN DEL MODULO

int init_module(void) {

        int i;
        int ret;
        int ret2;

	AUDIT{
	   printk(KERN_INFO "%s: received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
     	   printk(KERN_INFO "%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
	}


        // inizializzazione delle system call nella tabella

	new_sys_call_array[0] = (unsigned long)sys_invalidate_data;
	new_sys_call_array[1] = (unsigned long)sys_put_data;
	new_sys_call_array[2] = (unsigned long)sys_get_data;

        ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);
        if (ret != HACKED_ENTRIES){
                printk(KERN_INFO "%s: could not hack %d entries (just %d)\n",MODNAME,HACKED_ENTRIES,ret); 
                return -1;      
        }

	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
        }
	protect_memory();

        printk(KERN_INFO "%s: all new system-calls correctly installed on sys-call table\n",MODNAME);



        // inizializzazione del device driver

        Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	if (Major < 0) {
	        printk("%s: registering device failed\n",MODNAME);
	        return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);



        // registrazione del filesystem

        ret2 = register_filesystem(&onefilefs_type);
        if (likely(ret2 == 0))
                printk(KERN_INFO "%s: sucessfully registered umessagefs\n",MODNAME);
        else
                printk(KERN_INFO "%s: failed to register umessagefs - error %d", MODNAME,ret2);

        return 0;

}

void cleanup_module(void) {

        int i;
        int ret;
                
        printk("%s: shutting down\n",MODNAME);

        // restore della system call table

	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
        }
	protect_memory();
        printk(KERN_INFO "%s: sys-call table restored to its original content\n",MODNAME);

        // eliminazione del device driver
        
        unregister_chrdev(Major, DEVICE_NAME);
	printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);


        // eliminazione del filesystem
        ret = unregister_filesystem(&onefilefs_type);

        if (likely(ret == 0))
                printk(KERN_INFO "%s: sucessfully unregistered file system driver\n",MODNAME);
        else
                printk(KERN_INFO "%s: failed to unregister umessagefs driver - error %d", MODNAME, ret);


	return;
        
}