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
	int* arg;
	struct file *filp;
	
	AUDIT printk("%s: invocation of sys_invalidate_data\n",MODNAME);

	arg = kmalloc(sizeof(int), GFP_KERNEL);
    if(!arg){
        printk("%s: kmalloc error, unable to allocate memory for receiving the user arguments\n", MODNAME);
        return -1;
    }

    // ret = copy_from_user(arg, &offset, sizeof(int));
	*arg = offset;        
    printk("block to invalidate is: %d\n", *arg);
    

	// open the device file

	printk("opening device %s\n", DEV_NAME);
	
	filp = filp_open(DEV_NAME, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk("error in filp open");
		return -1;
	}
	printk("device file opened");

	
	vfs_ioctl(filp, INVALIDATE_DATA, (unsigned long) arg);	
	
	filp_close(filp, NULL);		
	return 0;
}




#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char*, source, size_t, size){
#else
asmlinkage int sys_put_data(char* source, size_t size){
#endif

	int ret;
	char* message;
	struct file *filp;
	struct put_args *args;

	// TODO controlla se è montato il file-system !

	printk("%s: invocation of sys_put_data\n",MODNAME);
	args = kmalloc(sizeof(struct put_args), GFP_KERNEL);
    if(!args){
    	printk("%s: kmalloc error, unable to allocate memory for receiving the user arguments\n",MODNAME);
        return -1;
    }

    
    // alloco dinamicamente l'area di memoria per ospitare il messaggio, poi lo copio dallo spazio utente
    message = kmalloc(size+1, GFP_KERNEL);
    if (!message){
    	printk("%s: kmalloc error, unable to allocate memory for receiving buffer in ioctl\n\n",MODNAME);
        return -ENOMEM;
    }

    ret = copy_from_user(message, source, size);
    message[size] = '\0';


    args->source = message;
	args->size = size + 1;		// the insertion must include '/0'
	     
    printk("message: %s, len: %lu\n", message, ((struct put_args*)args)->size);


	// open the device file

	printk("opening device %s\n", DEV_NAME);
	
	filp = filp_open(DEV_NAME, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk("error in filp open");
		return -1;
	}
	printk("device file opened");


	vfs_ioctl(filp, PUT_DATA, (unsigned long) args);	
	
	filp_close(filp, NULL);		
	return 0;
}




#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _get_data, int, offset, char*, destination, size_t, size){
#else
asmlinkage int sys_get_data(int offset, char* destination, size_t size){
#endif


	struct file *filp;
	struct get_args *args;

	AUDIT printk("%s: invocation of sys_get_data\n",MODNAME);


	// TODO controlla se è montato il file-system !

	args = kmalloc(sizeof(struct get_args), GFP_KERNEL);
    if(!args){
    	printk("%s: kmalloc error, unable to allocate memory for receiving the user arguments\n",MODNAME);
        return -1;
    } 
    
	
	args->offset = offset;
	args->destination = destination;
	args->size = size;
    printk("destination: %px, len: %lu, block: %d\n", ((struct get_args*)args)->destination, ((struct get_args*)args)->size, ((struct get_args*)args)->offset);


	// open the device file

	printk("opening device %s\n", DEV_NAME);
	
	filp = filp_open(DEV_NAME, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk("error in filp open");
		return -1;
	}
	printk("device file opened");

	

	vfs_ioctl(filp, GET_DATA, (unsigned long) args);	
	
	filp_close(filp, NULL);		
	return 0;
}





// dopo le macro vengono create due funzioni con nomi differenti da sys_...

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;       
long sys_put_data = (unsigned long) __x64_sys_put_data;
long sys_get_data = (unsigned long) __x64_sys_get_data;
#else
#endif