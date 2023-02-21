#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>	
#include <linux/pid.h>		         /* For pid types */
#include <linux/tty.h>		         /* For the tty declarations */
#include <linux/version.h>	         /* For LINUX_VERSION_CODE */
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/ioctl.h>


// DEFINIZIONI E DICHIARAZIONI

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
// static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static ssize_t dev_read (struct file *, char *, size_t, loff_t *);
static long    dev_ioctl(struct file *, unsigned int, unsigned long);
int            dev_put_data(char *, size_t);
int            dev_invalidate_data(int);
int            dev_get_data(int, char *, size_t);

int Major;
struct block_device *bdev;
// static DEFINE_MUTEX(device_state);



// IMPLEMENTAZIONE DELLE OPERAZIONI

static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param){
   

   long ret;
   int minor = get_minor(filp);
   int major = get_major(filp);


   AUDIT{
      printk(KERN_INFO "%s: somebody called an ioctl on dev with [major,minor] number [%d,%d] and command %u \n\n",MODNAME, major, minor, command);
      printk(KERN_INFO "%s: %s is the block device name\n", MODNAME, block_device_name);
   }

   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't read from invalid block device name, your filesystem is not mounted", MODNAME);
      return -1;                          // return ENODEV ?
   }

   if(_IOC_TYPE(command) != MAGIC_UMSG) return -EINVAL;
   
   switch(command){

      case PUT_DATA:
         ret = dev_put_data(((struct put_args *) param)->source, ((struct put_args *) param)->size);
         break;
      
      
      case GET_DATA:
         ret = dev_get_data(((struct get_args*)param)->offset, ((struct get_args*)param)->destination, ((struct get_args*)param)->size);
         break;
      
      
      case INVALIDATE_DATA:
         ret = dev_invalidate_data(*((int*) param));
         break;

      
      default:
         return -EINVAL;
         
   }
   
   return ret;

}



// PUT DATA

int dev_put_data(char* source, size_t size){
   
   int i;
   int block_to_write;
   struct block_node *cas;
   struct block_node *tail;
   struct block_node *old_next;
   struct buffer_head *bh = NULL;
   struct block_node current_block;
   struct block_node* selected_block = NULL;



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
      return -1;
   }
   printk("il blocco invalido è il numero %d\n", selected_block->num);



   // get the tail of the valid blocks (ordered write)
   tail = valid_messages;
   
   while(get_pointer(tail->val_next) != change_validity(NULL)){       // the tail has next pointer NULL but VALID
      tail = get_pointer(tail->val_next);
      if(tail->val_next == NULL){
         printk("%s: not able to find the tail due to concurrency, retry\n", MODNAME);
         return -1;
      }
   }
   printk("la coda è al blocco: %d\n", tail->num);
   
   

   // write lock on the selected block
   mutex_lock(&(selected_block->lock));
   
   
   // BLOCK VALIDATION – another writer could select this block and insert it in the valid list.
   // The CAS can detect the change (current_block.val_next not found) and stop this insertion.
   old_next = __sync_val_compare_and_swap(&(selected_block->val_next), current_block.val_next, change_validity(NULL));

   if(old_next == change_validity(NULL)){
      // FAILURE - new value returned
      printk("%s: La CAS è fallita, valore trovato: %px - valore atteso: %px\n", MODNAME, change_validity(selected_block->val_next), change_validity(NULL));
      mutex_unlock(&(selected_block->lock));
      return -1;
   }
   
   // SUCCESS - old value returned
   printk("%s: La CAS ha avuto successo selected.next = %px (dovrebbe essere NULL)\n", MODNAME, selected_block->val_next);
   

   
   cas = __sync_val_compare_and_swap(&(tail->val_next), change_validity(NULL), selected_block);    // ALL OR NOTHING - scambio il campo next della coda (NULL)
                                                                                                   // con il puntatore all'elemento che sto inserendo
                                                                                                   // (ptr nel kernel ha bit a sx = 1, valido). Il bit di validità nel
                                                                                                   // puntatore garantisce il fallimento in caso di interferenze

   if(cas != change_validity(NULL)){
      // FAILURE - new value returned
      printk("%s: La CAS è fallita, valore trovato: %px - valore atteso: %px\n", MODNAME, change_validity(tail->val_next), selected_block);
      selected_block->val_next = NULL;
      mutex_unlock(&(selected_block->lock));
      return -1;
   }
   
   // SUCCESS - old value returned
   printk("%s: La CAS ha avuto successo ex_coda.next = %px, &inserted_block = %px\n", MODNAME, change_validity(tail->val_next), selected_block);
      
      
   // trovare il block device   
   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't write from invalid block device name, your filesystem is not mounted", MODNAME);
      selected_block->val_next = NULL;
      mutex_unlock(&(selected_block->lock));
      return -1;                          // return ENODEV ? settare ERRNO ?
   }

   bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);
   
   if(bdev == NULL){
      printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
      selected_block->val_next = NULL;
      mutex_unlock(&(selected_block->lock));
      return -1;                          // return ENODEV ?
   }


   // get the cached data
   
   block_to_write = offset(selected_block->num);
   bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_write);
   if(!bh){
      selected_block->val_next = NULL;
      mutex_unlock(&(selected_block->lock));
      return -EIO;
   }
   if (bh->b_data != NULL){      // e se è null che succede?
      AUDIT printk(KERN_INFO "%s: [blocco %d] valore vecchio: %s\n", MODNAME, block_to_write, bh->b_data);   
   }

   
   // the update is not atomic: the lock protect concurrent updates

   strncpy(bh->b_data, source, size);  
   brelse(bh);
   blkdev_put(bdev, FMODE_READ);
   mutex_unlock(&(selected_block->lock));  

   //STAMPA PROVA
   for(i=0; i<NBLOCKS; i++){
      printk("%d) val_next=%px", i, block_metadata[i].val_next);
   }
   //FINE STAMPA

   return DEFAULT_BLOCK_SIZE;

}





// GET DATA

int dev_get_data(int offset, char * destination, size_t size){

   int ret;
	int index;
   int return_val;
   int block_to_read;
   struct buffer_head *bh = NULL;
   struct block_node* selected_block;
   
	unsigned long my_epoch;

   // get the block device in order to reach cached data
   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't read from invalid block device name, your filesystem is not mounted", MODNAME);
      return -1;                          // return ENODEV ? settare ERRNO ?
   }

   bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);
   
   if(bdev == NULL){
      printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
      return -1;                          // return ENODEV ?
   }


   // get metadata of the block   
   printk(KERN_INFO "%s: GETDATA del blocco %d\n", MODNAME, offset);
   selected_block = &block_metadata[offset];

   if(get_validity(selected_block->val_next) == 0){
      printk(KERN_INFO "%s: the block requested is not valid", MODNAME);
      return 0;
   }

   // signal the presence of reader - avoid that a writer reuses this block while i'm reading
	my_epoch = __sync_fetch_and_add(&epoch,1);


   // get cached data
   block_to_read = offset(offset);
   bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_read);
   if(!bh){
      return_val = -EIO;
      goto ret;
   }
   if (bh->b_data != NULL)
      AUDIT printk(KERN_INFO "%s: [blocco %d] ho letto -> %s\n", MODNAME, block_to_read, bh->b_data);

   
   // ritorna il numero di byte che non ha potuto copiare
   ret = copy_to_user(destination, bh->b_data, size);
   return_val = size - ret;
   
   brelse(bh);
   blkdev_put(bdev, FMODE_READ);

   
ret:
   // the first bit in my_epoch is the index where we must release the counter
   index = (my_epoch & MASK) ? 1 : 0;           
	__sync_fetch_and_add(&pending[index],1);
   return return_val;

}





// INVALIDATE DATA

int dev_invalidate_data(int offset){
   printk("INVALIDATE DATA\n");

   int ret;
   struct block_node *cas;
   struct block_node *selected;
   struct block_node *predecessor;

	unsigned long last_epoch;
	unsigned long updated_epoch;
	unsigned long grace_period_threads;
	int index;

   ret = 0;
   selected = &block_metadata[offset];   
   predecessor = valid_messages;                                                   

   printk("sto invalidando il blocco %d\n", offset);                                                      
   // get write lock on the element to invalidate
   mutex_lock(&(selected->lock));

   // if the block is already invalid there is nothing to do   
   if(get_validity(selected->val_next) == 0){
      AUDIT printk("%s: the block %d is already invalid, nothing to do\n", MODNAME, offset);
      goto end;
   }
   
   printk("il blocco ha validità %d\n", get_validity(selected->val_next));
   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't write from invalid block device name, your filesystem is not mounted", MODNAME);
      goto error;                         // return ENODEV ? settare ERRNO ?
   }
   


   // Get the predecessor (N-1) of the block to invalidate (N) 
   
   while(get_pointer(predecessor->val_next) != &block_metadata[offset]){
      printk("sono il blocco: %d\n", predecessor->num);
      predecessor = get_pointer(predecessor->val_next);
      if(predecessor == change_validity(NULL)){
         printk("%s: element to invalidate not found, inconsistent values in metadata", MODNAME);
         goto error;
      }
   }
   printk("il predecessore è al blocco: %d\n", predecessor->num);

 

   // invalidate the block - it is valid now, we checked after the lock
   
   selected->val_next = change_validity(selected->val_next);                                               
   AUDIT printk("ora il blocco selezionato ha val_next: %px\n", selected->val_next);


   // unhook the element               DOVE                    COSA C'ERA              COSA CI METTO
   // get_pointer function validate in every case the predecessor: the CAS wants to find the predecessor in a valid state
   // in order to avoid problems in invalidations of adjacent elements
   cas = __sync_val_compare_and_swap(&(predecessor->val_next), get_pointer(predecessor->val_next), change_validity(selected->val_next));
   

   if(cas == change_validity(selected->val_next)){
      // FAILURE - new value returned
      printk("%s: La CAS è fallita, valore trovato: %px - valore atteso: %px\n", MODNAME, predecessor->val_next, change_validity(selected->val_next));
      goto error;
   }
   

   // SUCCESS - old value returned
   printk("%s: La CAS ha avuto successo prev.next = %px, invalidated.next = %px\n", MODNAME, predecessor->val_next, selected->val_next);
      
      
   // move to a new epoch
	updated_epoch = (next_epoch_index) ? MASK : 0;
   next_epoch_index += 1;
	next_epoch_index %= 2;	

	last_epoch = __atomic_exchange_n (&(epoch), updated_epoch, __ATOMIC_SEQ_CST); 
	index = (last_epoch & MASK) ? 1 : 0; 
	grace_period_threads = last_epoch & (~MASK); 

	AUDIT
	printk("%s: waiting %d readers)\n", MODNAME);
	
	
   DECLARE_WAIT_QUEUE_HEAD(wqueue);
   wait_event_interruptible(wqueue, pending[index] >= grace_period_threads);
   pending[index] = 0;
   printk("HO FINITO\n");


end: 
   mutex_unlock(&(selected->lock));  
   return ret;
error:
   selected->val_next = change_validity(selected->val_next);
   ret = -1;
   goto end;
}





// READ - FILE OPERATION

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

   // LETTURA DI TUTTI I BLOCCHI IN ORDINE

   int ret; 
   int index;
   int lenght;
   int copied;
   char temp[] = "";
   char* temp_buf;
   int block_to_read;
   struct block_node *next;
   struct block_node *curr;
   struct block_device *bdev;
   int minor = get_minor(filp);
   int major = get_major(filp);
   struct buffer_head *bh = NULL;

   unsigned long my_epoch;

   AUDIT{
      printk(KERN_INFO "%s: somebody called a read on dev with [major,minor] number [%d,%d], len = %d\n",MODNAME, major, minor, len);
      printk(KERN_INFO "%s: %s is the block device name\n", MODNAME, block_device_name);
   }

   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't read from invalid block device name, your filesystem is not mounted", MODNAME);
      return -1;                          // return ENODEV ?
   }

   if(*off != 0) return 0;
   copied = 0;

   // get the block device in order to reach cached data

   bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);

   if(bdev == NULL){
      printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
      return -1;                          // return ENODEV ?
   }

   // signal the presence of reader 
   my_epoch = __sync_fetch_and_add(&epoch,1);
 
   // check if the valid list is empty
   if(get_pointer(valid_messages->val_next) == change_validity(NULL)){
      ret = 0;
      goto ending;
   }


   // READ ACCESS to the first block
   next = get_pointer(valid_messages->val_next);

   // iterate on the blocks until the next is NULL
   
   while(next != change_validity(NULL)){      
      
      curr = next;
      block_to_read = offset(next->num);

      // get cached data
      bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_read);
      if(!bh){
         ret = -EIO;
         goto ending;
      }

      if (bh->b_data != NULL){
      
         lenght = strlen(bh->b_data);
         AUDIT printk(KERN_INFO " data: %s of len: %lu\n", bh->b_data, lenght);


         // allocate and use the temp buffer in order to modify 
         // the retrieved message with the termination character

         temp_buf = kmalloc(lenght+1, GFP_KERNEL);
         if(!temp_buf){
            printk("%s: kmalloc error, unable to allocate memory for read messages as single file\n", MODNAME);
            ret = -1;
            goto ending;
         }

         strncpy(temp_buf, bh->b_data, lenght);
         temp_buf[lenght] = '\n';
      
         ret = copy_to_user(buff + copied, temp_buf, lenght+1);
         copied = copied + lenght + 1 - ret;
      }
      
      brelse(bh);      
      next = get_pointer(curr->val_next);    // READ ACCESS to next block

   }

   temp_buf = kmalloc(1, GFP_KERNEL);
   if(!temp_buf){
      printk("%s: kmalloc error, unable to allocate memory for read messages as single file\n", MODNAME);
      ret = -1;
      goto ending;
   }

   temp[0] = '\0';
   ret = copy_to_user(buff + copied, temp, 1);
   copied =  copied + 1 - ret;
   
   index = (my_epoch & MASK) ? 1 : 0;           
	__sync_fetch_and_add(&pending[index],1);
   
   ret = len;
ending:   
   kfree(temp_buf);
   blkdev_put(bdev, FMODE_READ);
   *off = *off + copied;
   return ret;
   
}




static int dev_open(struct inode *inode, struct file *file) {

   // TODO

   // this device file is single instance
   // if (!mutex_trylock(&device_state)) {
	// 	return -EBUSY;
   // }

   printk(KERN_INFO "%s: device file successfully opened by thread %d\n",MODNAME,current->pid);
   printk("%s: TODO vedi se introdurre sincronizzazione\n",MODNAME);
   //device opened by a default nop
   return 0;
}




static int dev_release(struct inode *inode, struct file *file) {

   // TODO
   // mutex_unlock(&device_state);

   printk(KERN_INFO "%s: device file closed by thread %d\n",MODNAME,current->pid);
   //device closed by default nop
   return 0;

}






static struct file_operations fops = {
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl
};