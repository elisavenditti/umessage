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



// DEFINIZIONE DEL PARAMETRO DEL MODULO: l'indirizzo della system call table

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);



// STRUTTURA DATI MANTENENTE I METADATI DEI BLOCCHI

struct block_node block_metadata[NBLOCKS];
struct block_node* valid_messages;


// FUNZIONI DI STARTUP E SHUTDOWN DEL MODULO

int init_module(void) {

        int i;
        int k;
        int ret;
        int ret2;
        struct block_node *head;
        unsigned long *counter = NULL;
        
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


        
        // inizializzazione dell'array
        
        // block_metadata = (struct block_node*) kmalloc(NBLOCKS*sizeof(struct block_node), GFP_KERNEL);
        // if(block_metadata == NULL){
        //         printk("%s: kmalloc error, can't allocate memory needed to manage user messages (array)\n",MODNAME);
        //         return -1;
        // }

        for(k=0; k<NBLOCKS; k++){
                              
                counter = (unsigned long *) kmalloc(sizeof(unsigned long), GFP_KERNEL);
                if(counter == NULL){
                        printk("%s: kmalloc error, can't allocate memory needed to manage user messages (counter)\n",MODNAME);
                        return -1;
                }
                *counter = 0;
                block_metadata[k].val_next = NULL;                  // il null è invalido (ha come bit più a sx uno 0)
                block_metadata[k].num = k;
                block_metadata[k].ctr = counter;
                mutex_init(&block_metadata[k].lock);
                
                // if(k==0){
                //         printk("puntatore è:                     %px\n", &block_metadata[k]);
                //         printk("puntatore con bit invalidato è:  %px\n", change_validity(&block_metadata[k]));
                //         printk("puntatore con bit val dopo inv:  %px\n", change_validity(change_validity(&block_metadata[k])));
                //         printk("puntatore ricavato da ptr puro:  %px\n", get_pointer(&block_metadata[k]));
                //         printk("puntatore ricavato da ptr val:   %px\n", get_pointer(change_validity(change_validity(&block_metadata[k]))));
                //         printk("puntatore ricavato da ptr inval: %px\n", get_pointer(change_validity(&block_metadata[k])));
                //         printk("validità ptr:                    %lu\n", get_validity(&block_metadata[k]));
                //         printk("validità ptr invalido:           %lu\n", get_validity(change_validity(&block_metadata[k])));
                //         printk("validità ptr valido:             %lu\n", get_validity(change_validity(change_validity(&block_metadata[k]))));
                //         printk("validità ptr null:               %lu\n", get_validity(NULL));
                        
                // }
                printk("ctr blocco %d inizializzato a %lu\n", k, *(block_metadata[k].ctr));
        }


        // creo una head permanente a cui agganciare elementi
        head = (struct block_node *) kmalloc(sizeof(struct block_node), GFP_KERNEL);
        if(head == NULL){
                printk("%s: kmalloc error, can't allocate memory needed to manage permanent head\n",MODNAME);
                return -1;
        }

        head->val_next = change_validity(NULL);
        head->num = -1;
        head->ctr = NULL;
        mutex_init(&head->lock);
        printk("chg validity NULL               = %px\n", change_validity(NULL));
        printk("get_pointer(NULL)               = %px\n", get_pointer(NULL));
        
        valid_messages = head;
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

        
        
        // TODO DEALLOCARE MEMORIA PER LA LISTA ... E PER L'ARRAY ...

	return;
        
}