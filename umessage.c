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
#include <linux/delay.h>
#include "lib/include/scth.h"
#include "umessage_header.h"
#include "umessage_device_driver.c"
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



// KERNEL THREAD: reset the epoch counter
int house_keeper(void* data){

	unsigned long last_epoch;
	unsigned long updated_epoch;
	unsigned long grace_period_threads;
	int index;

redo:
        // "period" seconds sleep
	msleep(PERIOD*1000); 

	mutex_lock(&(rcu.lock));

        updated_epoch = (rcu.next_epoch_index) ? MASK : 0;
        rcu.next_epoch_index += 1;
	rcu.next_epoch_index %= 2;	

	last_epoch = __atomic_exchange_n (&(rcu.epoch), updated_epoch, __ATOMIC_SEQ_CST);
	index = (last_epoch & MASK) ? 1 : 0; 
	grace_period_threads = last_epoch & (~MASK); 

	AUDIT printk(KERN_INFO "%s: HOUSE KEEPING (waiting %lu readers on index = %d)\n", MODNAME, grace_period_threads, index);
	
        wait_event_interruptible(wqueue, rcu.pending[index] >= grace_period_threads);
        rcu.pending[index] = 0;
	mutex_unlock(&(rcu.lock));

	goto redo;
	return 0;
}


// STARTUP AND SHUTDOWN FUNCTIONS

int init_module(void) {

        int i;
        int k;
        int ret;
        int ret2;
        struct block_node *head;
	struct task_struct *the_daemon;
        char name[128] = "the_reset_daemon";
        
	
	printk("%s: received sys_call_table address %px\n", MODNAME, (void*)the_syscall_table);
     	printk("%s: initializing - hacked entries %d\n", MODNAME, HACKED_ENTRIES);
	


        // system call initialization

	new_sys_call_array[0] = (unsigned long)sys_invalidate_data;
	new_sys_call_array[1] = (unsigned long)sys_put_data;
	new_sys_call_array[2] = (unsigned long)sys_get_data;

        ret = get_entries(restore, HACKED_ENTRIES, (unsigned long*)the_syscall_table, &the_ni_syscall);
        if (ret != HACKED_ENTRIES){
                printk("%s: could not hack %d entries (just %d)\n", MODNAME, HACKED_ENTRIES, ret); 
                return -1;      
        }

	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
        }
	protect_memory();

        printk("%s: all new system-calls correctly installed on sys-call table\n", MODNAME);



        // device driver initialization

        Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	if (Major < 0) {
	        printk("%s: registering device failed\n",MODNAME);
	        return Major;
	}

	printk("%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

        
        // filesystem registration

        ret2 = register_filesystem(&onefilefs_type);
        if (likely(ret2 == 0))
                printk("%s: sucessfully registered umessagefs\n",MODNAME);
        else
                printk("%s: failed to register umessagefs - error %d", MODNAME,ret2);


        
        // metadata array initialization

        for(k=0; k<MAXBLOCKS; k++){
                block_metadata[k].val_next = NULL;              // null is invalid (leftmost bit set to 0)
                block_metadata[k].num = k;
        }

        // counter init

        rcu.epoch = 0x0;
	rcu.pending[0] = 0x0;
        rcu.pending[1] = 0x0;
	rcu.next_epoch_index = 0x1;
        mutex_init(&rcu.lock);


        // creation of a permanent head to hook elements

        head = (struct block_node *) kmalloc(sizeof(struct block_node), GFP_KERNEL);
        if(head == NULL){
                printk("%s: kmalloc error, can't allocate memory needed to manage permanent head\n",MODNAME);
                return -1;
        }

        head->val_next = change_validity(NULL);
        head->num = -1;
        valid_messages = head;
        printk("%s: metadata initialized\n",MODNAME);


        // create kernel thread to reset epoch counter and avoid overflow

	the_daemon = kthread_create(house_keeper, NULL, name);
	if(the_daemon) {
		wake_up_process(the_daemon);
                printk("%s: kernel daemon initialized\n",MODNAME);
	}
        else{
                printk("%s: failed initializing kernel daemon\n",MODNAME);
                return -1;
        }
	
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
        printk("%s: sys-call table restored to its original content\n",MODNAME);

        
        // device driver deletion
        
        unregister_chrdev(Major, DEVICE_NAME);
        
        printk("%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);
        
        

        // filesystem deletion
        ret = unregister_filesystem(&onefilefs_type);

        if (likely(ret == 0))
                printk("%s: sucessfully unregistered file system driver\n",MODNAME);
        else
                printk("%s: failed to unregister umessagefs driver - error %d", MODNAME, ret);

        
        // deallocation of the permanent head
        kfree(valid_messages);
        return;
        
}