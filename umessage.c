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
MODULE_DESCRIPTION("user message block level mantainance");



// MODULE PARAMETER: system call table's address

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);



// DATA STRUCTURE TO KEEP BLOCKS METADATA

struct block_node block_metadata[MAXBLOCKS];
struct block_node* valid_messages;
struct counter rcu __attribute__((aligned(64)));

int Major;


// STARTUP AND SHUTDOWN FUNCTIONS

int init_module(void) {

        int i;
        int k;
        int ret;
        int ret2;
        struct block_node *head;
        
	AUDIT{
	   printk(KERN_INFO "%s: received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
     	   printk(KERN_INFO "%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
	}


        // system call initialization

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



        // idevice driver initialization

        Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	if (Major < 0) {
	        printk("%s: registering device failed\n",MODNAME);
	        return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

        
        // filesystem registration

        ret2 = register_filesystem(&onefilefs_type);
        if (likely(ret2 == 0))
                printk(KERN_INFO "%s: sucessfully registered umessagefs\n",MODNAME);
        else
                printk(KERN_INFO "%s: failed to register umessagefs - error %d", MODNAME,ret2);


        
        // metadata array initialization

        for(k=0; k<MAXBLOCKS; k++){
                              
                
                block_metadata[k].val_next = NULL;                  // null is invalid (leftmost bit set to 0)
                block_metadata[k].num = k;
                mutex_init(&block_metadata[k].lock);
                
        }

        // counter init
        rcu.epoch = 0x0;
	rcu.pending[0] = 0x0;
        rcu.pending[1] = 0x0;
	rcu.next_epoch_index = 0x1;	


        // creation of a permanent head to hook elements
        head = (struct block_node *) kmalloc(sizeof(struct block_node), GFP_KERNEL);
        if(head == NULL){
                printk("%s: kmalloc error, can't allocate memory needed to manage permanent head\n",MODNAME);
                return -1;
        }

        head->val_next = change_validity(NULL);
        head->num = -1;
        mutex_init(&head->lock);
        valid_messages = head;
        return 0;

}

void cleanup_module(void) {

        int i;
        int ret;
                
        printk("%s: shutting down\n",MODNAME);

        // system call table restore

	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
        }
	protect_memory();
        printk(KERN_INFO "%s: sys-call table restored to its original content\n",MODNAME);

        
        // device driver deletion
        
        unregister_chrdev(Major, DEVICE_NAME);
        
        printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);
        
        

        // filesystem deletion
        ret = unregister_filesystem(&onefilefs_type);

        if (likely(ret == 0))
                printk(KERN_INFO "%s: sucessfully unregistered file system driver\n",MODNAME);
        else
                printk(KERN_INFO "%s: failed to unregister umessagefs driver - error %d", MODNAME, ret);

        
        
        // TODO DEALLOCARE MEMORIA PER LA LISTA ... E PER L'ARRAY ...

	return;
        
}