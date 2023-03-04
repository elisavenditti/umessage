#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
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

#include <linux/pid.h>
#include <linux/tty.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/delay.h>            // inclusion for testing





static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read (struct file *, char *, size_t, loff_t *);



// DEFINIZIONI UTILI

unsigned long the_ni_syscall;
unsigned long new_sys_call_array[] = {0x0,0x0,0x0}; //to initialize at startup

#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};

DECLARE_WAIT_QUEUE_HEAD(wqueue);





// PUT_DATA SYSCALL - insert size byte of the source in a free block

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char*, source, size_t, size){
#else
asmlinkage int sys_put_data(char* source, size_t size){
#endif

	
   char* message;
   int i, ret, len;
   int block_to_write;
   struct block_node *cas;
   struct block_node *tail;
   struct block_node *old_next;
   struct buffer_head *bh = NULL;
   struct block_node current_block;
   struct block_node* selected_block = NULL;
   struct block_device *temp;
   
   printk("%s: DEVICE DRIVER - PUT DATA\n", MODNAME);
   if(size >= DATA_SIZE) return -EINVAL;
	
   
   // dynamic allocation of area to contain the message to write
	message = kzalloc(DATA_SIZE, GFP_KERNEL);
   if (!message){
    	printk("%s: kmalloc error, unable to allocate memory for receiving buffer in ioctl\n\n",MODNAME);
      return -ENOMEM;
   }


	// copy size bytes from user to kernel buffer
   ret = copy_from_user(message, source, size);
	len = strlen(message);
	if(len<size) size = len;
   message[size] = '\0';
   AUDIT	printk(KERN_INFO "%s: message to put is %s (len - %lu)\n", MODNAME, message, size+1);
	


   // signal the presence of reader on bdev variable
   __sync_fetch_and_add(&(bdev_md.bdev_usage),1);            
   temp = bdev_md.bdev;
   if(temp == NULL){
      printk("%s: NO DEVICE MOUNTED", MODNAME);
      ret = -ENODEV;
      goto put_exit;
   }


   // get an invalid block to overwrite
   for(i=0; i<NBLOCKS; i++){
      
      current_block = block_metadata[i];
      
      if(get_validity(current_block.val_next) == 0){
         selected_block = &block_metadata[i];
         break;
      }
   }
   
   if (selected_block == NULL){
      printk("%s: no space available to insert a message\n", MODNAME);
      ret = -ENOMEM;
      goto put_exit;
   }
   AUDIT printk(KERN_INFO "%s: the invalid block is the number %d\n", MODNAME, selected_block->num);




   // BLOCK VALIDATION – another writer could select this block and attempt to validate it.
   // The CAS can detect the change (current_block.val_next not found) and stop this insertion.
   old_next = __sync_val_compare_and_swap(&(selected_block->val_next), current_block.val_next, change_validity(NULL));

   if(old_next == change_validity(NULL)){
      // FAILURE - new value returned
      printk("%s: CAS failed - found value: %px - expected value to find: %px\n", MODNAME, selected_block->val_next, current_block.val_next);
      ret = -EAGAIN;
      goto put_exit;
   }
   
   // SUCCESS - old value returned
   AUDIT printk(KERN_INFO "%s: CAS succeded - selected.next = %px (should be valid NULL)\n", MODNAME, selected_block->val_next);


   // get the cached data
   block_to_write = offset(selected_block->num);
   bh = (struct buffer_head *)sb_bread(temp->bd_super, block_to_write);
   if(!bh){
      selected_block->val_next = NULL;
      ret = -EIO;
      goto put_exit;
   }
   

   // UPDATE DATA: the concurrent insertions on this block stopped on previous CAS so this is the only writer on bh->b_data
   if (bh->b_data != NULL){ 
      strncpy(bh->b_data, message, DATA_SIZE);  
      mark_buffer_dirty(bh);
   }
   
   

   // get the tail of the valid blocks
   tail = valid_messages;
   while(get_pointer(tail->val_next) != change_validity(NULL)){       // the tail has next pointer NULL but VALID
      
      tail = get_pointer(tail->val_next);
      if(tail->val_next == NULL){
         printk("%s: not able to find the tail due to concurrency, retry\n", MODNAME);
         selected_block->val_next = NULL;
         ret = -EAGAIN;
         goto put_exit;
      }
   }

   AUDIT printk(KERN_INFO "%s: the tail is at block %d\n", MODNAME, tail->num);




   // INSERT THE NODE in the valid list. Exchange the tail's next field (valid NULL) with the pointer
   // to the new element. Interferences are detected: the tail's next field has the validity bit included
   cas = __sync_val_compare_and_swap(&(tail->val_next), change_validity(NULL), selected_block);
   
   if(cas != change_validity(NULL)){
      // FAILURE - new value returned
      printk("%s: CAS failed - found value: %px - expected value: %px\n", MODNAME, tail->val_next, selected_block);
      selected_block->val_next = NULL;
      ret = -EAGAIN;
      goto put_exit;
      return -1;
   }
   
   // SUCCESS - old value returned
   AUDIT printk(KERN_INFO "%s: CAS succeded - tail.next = %px, &inserted_block = %px\n", MODNAME, tail->val_next, selected_block);
   
   
#ifdef FORCE_SYNC 
      // force the synchronous write on the device
      if(sync_dirty_buffer(bh) == 0){
         AUDIT printk(KERN_INFO "%s: SUCCESS IN SYNCHRONOUS WRITE", MODNAME);
      } else
         printk("%s: FAILURE IN SYNCHRONOUS WRITE", MODNAME);
#endif
   
   

   brelse(bh);

   // STAMPA PROVA
   for(i=0; i<NBLOCKS; i++){
      printk("%d) val_next=%px", i, block_metadata[i].val_next);
   }
   // FINE STAMPA

   ret = selected_block->num;

put_exit:   
   __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);
   wake_up_interruptible(&umount_queue);
   kfree(message);
   return ret;

}






// GET_DATA SYSCALL - get size bytes from the block at the specified offset

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _get_data, int, offset, char*, destination, size_t, size){
#else
asmlinkage int sys_get_data(int offset, char* destination, size_t size){
#endif

   int ret;
	int index;
   int return_val;
   int block_to_read;
   char end = '\0';
   unsigned long my_epoch;
   struct buffer_head *bh = NULL;
   struct block_node* selected_block;  
   struct block_device *temp; 
	

   printk("%s: DEVICE DRIVER - GET DATA\n", MODNAME);

   if(size > DATA_SIZE) size = DATA_SIZE;
	else if(size < 0 || offset>NBLOCKS || offset<0) return -EINVAL;
   AUDIT printk(KERN_INFO "%s: the destination buffer is at %px; getting %lu bytes of block %d\n", MODNAME, destination, size, offset);


   // signal the presence of reader on bdev variable
   __sync_fetch_and_add(&(bdev_md.bdev_usage),1);            
   temp = bdev_md.bdev;
   if(temp == NULL){
      printk("%s: NO DEVICE MOUNTED", MODNAME);
      __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);    
      wake_up_interruptible(&umount_queue);
      return -ENODEV;
   }

   // get metadata of the block   
   selected_block = &block_metadata[offset];

   if(get_validity(selected_block->val_next) == 0){
      printk("%s: the block requested is not valid", MODNAME);
      __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);
      wake_up_interruptible(&umount_queue);
      return -ENODATA;
   }

   // signal the presence of reader - avoid that a writer reuses this block while i'm reading
	my_epoch = __sync_fetch_and_add(&(rcu.epoch),1);


   // get cached data
   block_to_read = offset(offset);
   bh = (struct buffer_head *)sb_bread(temp->bd_super, block_to_read);
   if(!bh){
      return_val = -EIO;
      goto get_exit;
   }
   
   
   if (bh->b_data != NULL){
      AUDIT printk(KERN_INFO "%s: [block %d] - %s\n", MODNAME, block_to_read, bh->b_data);  
      ret = copy_to_user(destination, bh->b_data, size);
      return_val = size - ret;
      ret = copy_to_user(destination+return_val, &end, 1);
      if(strlen(bh->b_data)<size) return_val = strlen(bh->b_data);
   }

   else return_val = 0;
   brelse(bh);

   
get_exit:

   // the first bit in my_epoch is the index where we must release the counter
   index = (my_epoch & MASK) ? 1 : 0;           
	__sync_fetch_and_add(&(rcu.pending[index]),1);
   __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);
   wake_up_interruptible(&wqueue);
   wake_up_interruptible(&umount_queue);
   return return_val;

}






// INVALIDATE_DATA SYSCALL - invalidate the block at specified offset

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, int, offset){
#else
asmlinkage int sys_invalidate_data(int offset){
#endif

   int ret, index;
   struct block_node *cas;
   struct block_device *temp;
   struct block_node *selected;
   struct block_node *predecessor;
   
	unsigned long last_epoch;
	unsigned long updated_epoch;
	unsigned long grace_period_threads;
    


   printk("%s: DEVICE DRIVER - INVALIDATE DATA\n", MODNAME);
   if (offset>NBLOCKS || offset<0) return -EINVAL;
   AUDIT printk(KERN_INFO "%s: block to invalidate is: %d\n", MODNAME, offset);

   
   // signal the presence of reader on bdev variable
   __sync_fetch_and_add(&(bdev_md.bdev_usage),1);
   temp = bdev_md.bdev;
   if(temp == NULL){
      printk("%s: NO DEVICE MOUNTED", MODNAME);
      __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);    
      wake_up_interruptible(&umount_queue);
      return -ENODEV;
   }

   // CONCURRENCY TEST
   // printk("10 seconds sleep\n");
   // msleep(10000);
   // printk("i'm awake\n");
   

   // get write lock 
   mutex_lock(&(rcu.lock));

   ret = 0;
   selected = valid_messages;
   predecessor = valid_messages;

   
   // get the block to invalidate
   while(get_pointer(selected->val_next) != change_validity(NULL)){
      
      selected = get_pointer(selected->val_next);
      if(selected->num == offset){
         printk("%s: found block to invalidate\n", MODNAME);
         break;   
      }
   }


   // if the block is not present in the list, it is already invalid and there is nothing to do 
   if (selected->num != offset){
      printk("%s: the block %d is already invalid, nothing to do\n", MODNAME, offset);
      ret = -ENODATA;
      goto inv_end;    
   }



   // get the predecessor (N-1) of the block to invalidate (N) - no concurrency here
   while(get_pointer(predecessor->val_next) != &block_metadata[offset]){
   
      predecessor = get_pointer(predecessor->val_next);
      if(predecessor == change_validity(NULL)){
         printk("%s: element to invalidate not found, inconsistent values in metadata", MODNAME);
         ret = -1;
         goto inv_end;
      }
   }
   AUDIT printk(KERN_INFO "%s: predecessor is the block %d\n", MODNAME, predecessor->num);
 



   // DELETE THE ELEMENT from valid list
   // get_pointer function validate in every case the predecessor: the CAS wants to find the predecessor in a valid state
   cas = __sync_val_compare_and_swap(&(predecessor->val_next), get_pointer(predecessor->val_next), get_pointer(selected->val_next));
   
   if(cas == change_validity(selected->val_next)){
      // FAILURE - new value returned
      printk("%s: CAS failed - found value: %px - expected value: %px\n", MODNAME, predecessor->val_next, get_pointer(selected->val_next));
      ret = -EAGAIN;
      goto inv_end;
   }

   // SUCCESS - old value returned
   AUDIT printk(KERN_INFO "%s: CAS succeded - prev.next = %px - invalidated.next = %px\n", MODNAME, predecessor->val_next, selected->val_next);
      

      
   // MOVE to a new epoch
   updated_epoch = (rcu.next_epoch_index) ? MASK : 0;
   rcu.next_epoch_index += 1;
	rcu.next_epoch_index %= 2;	

	last_epoch = __atomic_exchange_n (&(rcu.epoch), updated_epoch, __ATOMIC_SEQ_CST);
	index = (last_epoch & MASK) ? 1 : 0; 
	grace_period_threads = last_epoch & (~MASK); 

	AUDIT printk(KERN_INFO "%s: INVALIDATE (waiting %lu readers on index = %d)\n", MODNAME, grace_period_threads, index);
	
	
   // WAIT the pending readers on the valid list
   wait_event_interruptible(wqueue, rcu.pending[index] >= grace_period_threads);
   rcu.pending[index] = 0;

   // invalidate the block - it should be valid, redundant check
   if(get_validity(selected->val_next)) selected->val_next = change_validity(selected->val_next);
   AUDIT printk(KERN_INFO "%s: invalidation completed! Now selected block has val_next %px\n", MODNAME, selected->val_next);



inv_end: 
   mutex_unlock(&(rcu.lock)); 
   __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);
   wake_up_interruptible(&umount_queue); 
   return ret;
}





// READ - FILE OPERATION

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

   int i;
   char* data;
   char* temp_buf;
   char temp[] = "";
   int block_to_read;
   unsigned long my_epoch;
   struct block_node *next;
   struct block_node *curr;
   int minor = get_minor(filp);
   int major = get_major(filp);
   
   struct buffer_head *bh = NULL;
   struct block_device *temp_bdev;
   int ret, index, lenght, copied;
   
   

   if(*off != 0) return 0;
   copied = 0;
   i = 0;


   
   printk(KERN_INFO "%s: READ FILE (maj,min) = (%d,%d)\n", MODNAME, major, minor);
   
   // signal the presence of reader on bdev variable
   __sync_fetch_and_add(&(bdev_md.bdev_usage),1);
   temp_bdev = bdev_md.bdev;
   if(temp_bdev == NULL){
      printk("%s: NO DEVICE MOUNTED", MODNAME);
      __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);
      wake_up_interruptible(&umount_queue);
      return -ENODEV;
   }
   

   // signal the presence of reader 
   AUDIT printk(KERN_INFO "%s: old ctr: %ld\n", MODNAME, (rcu.epoch) & (~MASK));
   my_epoch = __sync_fetch_and_add(&(rcu.epoch),1);
   AUDIT printk(KERN_INFO "%s: new ctr: %ld\n", MODNAME, (rcu.epoch) & (~MASK));


   // check if the valid list is empty
   if(get_pointer(valid_messages->val_next) == change_validity(NULL)){
      ret = 0;
      goto read_end;
   }

   // READ ACCESS to the first block
   next = get_pointer(valid_messages->val_next);




   // ITERATE ON THE BLOCKS UNTIL THE NEXT IS NULL
   while(next != change_validity(NULL)){      
      
      curr = next;
      block_to_read = offset(next->num);

      // get cached data
      bh = (struct buffer_head *)sb_bread(temp_bdev->bd_super, block_to_read);
      if(!bh){
         ret = -EIO;
         goto read_end;
      }

      if (bh->b_data != NULL){
         
         data = ((struct bdev_node*) (bh->b_data))->data;
         lenght = strlen(bh->b_data);
         AUDIT printk(KERN_INFO "%s: data: %s of len: %d\n", MODNAME, bh->b_data, lenght);


         // allocate and use the temp buffer in order to modify 
         // the retrieved message with the termination character

         temp_buf = kmalloc(lenght+1, GFP_KERNEL);
         if(!temp_buf){
            printk("%s: kmalloc error, unable to allocate memory for read messages as single file\n", MODNAME);
            ret = -1;
            goto read_end;
         }

         strncpy(temp_buf, bh->b_data, lenght);
         temp_buf[lenght] = '\n';
      
         ret = copy_to_user(buff + copied, temp_buf, lenght+1);
         copied = copied + lenght + 1 - ret;
      }
      
      
      brelse(bh);
      next = get_pointer(curr->val_next);    // READ ACCESS to next block      
      
      i++;

#ifdef TEST
      if(i==2){
         printk("10 seconds sleep\n");
         msleep(10000);
         printk("i'm awake\n");
      }
#endif

   }

   temp[0] = '\0';
   ret = copy_to_user(buff + copied, temp, 1);
   copied =  copied + 1 - ret;
   ret = len;
   kfree(temp_buf);
   


read_end:  

   index = (my_epoch & MASK) ? 1 : 0;           
	
   AUDIT printk(KERN_INFO "%s: reset ctr index %d (before): %ld\n", MODNAME, index, rcu.pending[index]);
   __sync_fetch_and_add(&(rcu.pending[index]),1);
   AUDIT printk(KERN_INFO "%s: reset ctr index %d (after): %ld\n", MODNAME, index, rcu.pending[index]);
   wake_up_interruptible(&wqueue);
   
   __sync_fetch_and_sub(&(bdev_md.bdev_usage),1);
   wake_up_interruptible(&umount_queue);

   *off = *off + copied;
   return ret;
   
}




static int dev_open(struct inode *inode, struct file *file) {

   printk(KERN_INFO "%s: thread %d trying to open file\n",MODNAME,current->pid);
   
   // not reserving the bdev_usage counter 
   if(bdev_md.bdev == NULL){
      printk("%s: NO DEVICE MOUNTED", MODNAME);
      return -ENODEV;
   }   

   // block every access in write mode to the file - read only FS
   if(file->f_mode & FMODE_WRITE){
      printk("%s: FORBIDDEN WRITE", MODNAME);
      return -EROFS;
   }

   return 0;
}




static int dev_release(struct inode *inode, struct file *file) {

   printk(KERN_INFO "%s: thread %d trying to close device file\n",MODNAME,current->pid);
   
   // not reserving the bdev_usage counter
   if(bdev_md.bdev == NULL){
      printk("%s: NO DEVICE MOUNTED", MODNAME);
      return -ENODEV;
   }

   return 0;

}




// dopo le macro vengono create due funzioni con nomi differenti da sys_...

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;       
long sys_put_data = (unsigned long) __x64_sys_put_data;
long sys_get_data = (unsigned long) __x64_sys_get_data;
#else
#endif




const struct file_operations fops = {
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release,
};