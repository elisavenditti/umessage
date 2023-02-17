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
// #include <linux/uaccess.h>          // per la get_user
// #include "umessage_header.h"


// DEFINIZIONI E DICHIARAZIONI

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static ssize_t dev_read (struct file *, char *, size_t, loff_t *);
static long    dev_ioctl(struct file *, unsigned int, unsigned long);
int            dev_put_data(char *, size_t);
int            dev_invalidate_data(int);
int            dev_get_data(int, char *, size_t);

static int Major;
struct block_device *bdev;
// static DEFINE_MUTEX(device_state);



// IMPLEMENTAZIONE DELLE OPERAZIONI

static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param){
   
   // TODO
   long ret;
   void* arg;
   size_t len;
   char* message;
   // struct block_device *bdev;
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
         
         // 1. prendere parametri dall'utente
         // alloco dinamicamente l'area di memoria per ospitare la struttura put_args, poi la popolo

         arg = kmalloc(sizeof(struct put_args), GFP_KERNEL);
         if(!arg){
            printk("%s: kmalloc error, unable to allocate memory for receiving the user arguments\n",MODNAME);
            return -1;
         }

         ret = copy_from_user(arg, (void *) param, sizeof(struct put_args));
         len = ((struct put_args*)arg)->size;
         

         // alloco dinamicamente l'area di memoria per ospitare il messaggio, poi lo copio dallo spazio utente
         message = kmalloc(len+1, GFP_KERNEL);
         if (!message){
            printk("%s: kmalloc error, unable to allocate memory for receiving buffer in ioctl\n\n",MODNAME);
            return -ENOMEM;
         }

         ret = copy_from_user(message, ((struct put_args*)arg)->source, len);
         message[len+1] = '\0';
         ((struct put_args*)arg)->source = message;
         
         printk("message: %s, len: %lu\n", message, ((struct put_args*)arg)->size);

         // 2. logica di inserimento di un messaggio
         ret = dev_put_data(((struct put_args *) arg)->source, ((struct put_args *) arg)->size);
         break;
      
      
      case GET_DATA:

         // struct get_args{
         //    int offset;
         //    char * destination;
         //    size_t size;
         // };

         // 1. prendere parametri dall'utente
         // alloco dinamicamente l'area di memoria per ospitare la struttura get_args, poi la popolo

         arg = kmalloc(sizeof(struct get_args), GFP_KERNEL);
         if(!arg){
            printk("%s: kmalloc error, unable to allocate memory for receiving the user arguments\n",MODNAME);
            return -1;
         }

         ret = copy_from_user(arg, (void *) param, sizeof(struct get_args));
         
         printk("buffer to insert: %px - len to read: %d - block to read: %d \n", ((struct get_args*)arg)->destination, ((struct get_args*)arg)->size, ((struct get_args*)arg)->offset);

         // 2. logica di inserimento di un messaggio
         ret = dev_get_data(((struct get_args*)arg)->offset, ((struct get_args*)arg)->destination, ((struct get_args*)arg)->size);
         break;
      
      
      case INVALIDATE_DATA:
         // 1. prendere parametri dall'utente
         arg = kmalloc(sizeof(int), GFP_KERNEL);
         if(!arg){
            printk("%s: kmalloc error, unable to allocate memory for receiving the user arguments\n", MODNAME);
            return -1;
         }

         ret = copy_from_user(arg, (void *) param, sizeof(int));        
         printk("block to invalidate is: %d\n", *((int*)arg));

         // 2. logica di eliminazione di un messaggio
         ret = dev_invalidate_data(*((int*)arg));
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
   
   while(get_pointer(tail->val_next) != change_validity(NULL)){       // l'ultimo nella lista avrà puntatore a NULL ma valido
      tail = get_pointer(tail->val_next);
      if(tail->val_next == NULL){
         printk("%s: not able to find the tail due to concurrency, retry\n", MODNAME);
         return -1;
      }
   }
   printk("la coda è al blocco: %d\n", tail->num);
   
   

   // write lock on the selected block
   mutex_lock(&(selected_block->lock));
   
   selected_block->val_next = change_validity(NULL);                                               // Il NULL ha come primo bit 0 (invalido) e bisogna 
                                                                                                   // validarlo prima di inserirlo nei metadati.
   printk("ora il blocco selezionato ha val_next: %px\n", selected_block->val_next);
   cas = __sync_val_compare_and_swap(&(tail->val_next), change_validity(NULL), selected_block);    // ALL OR NOTHING - scambio il campo next della coda (NULL)
                                                                                                   // con il puntatore all'elemento che sto inserendo
                                                                                                   // (ptr nel kernel ha bit a sx = 1, valido). Il bit di validità nel
                                                                                                   // puntatore garantisce il fallimento in caso di interferenze

   if(cas != change_validity(NULL)){
      // FALLIMENTO
      printk("%s: La CAS è fallita, valore trovato: %px - valore atteso: %px\n", MODNAME, change_validity(tail->val_next), selected_block);
      mutex_unlock(&(selected_block->lock));
      return -1;
   }
   

   // SUCCESSO - ricevo il valore vecchio
   printk("%s: La CAS ha avuto successo ex_coda.next = %px, &inserted_block = %px\n", MODNAME, change_validity(tail->val_next), selected_block);
      
      
   // trovare il block device   
   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't write from invalid block device name, your filesystem is not mounted", MODNAME);
      mutex_unlock(&(selected_block->lock));
      return -1;                          // return ENODEV ? settare ERRNO ?
   }

   bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);
   
   if(bdev == NULL){
      printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
      mutex_unlock(&(selected_block->lock));
      return -1;                          // return ENODEV ?
   }


   // prendo i dati da aggiornare
   
   block_to_write = offset(selected_block->num);
   bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_write);
   if(!bh){
      return -EIO;
   }
   if (bh->b_data != NULL){      // e se è null che succede?
      AUDIT printk(KERN_INFO "%s: [blocco %d] valore vecchio: %s\n", MODNAME, block_to_write, bh->b_data);   
   }

   
   // aggiornamento non atomico, il lock protegge da scritture concorrenti
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
   int block_to_read;
   struct buffer_head *bh = NULL;
   struct block_node* selected_block;

   

   // get metadata of the block   
   printk(KERN_INFO "%s: GETDATA del blocco %d\n", MODNAME, offset);
   selected_block = &block_metadata[offset];

   if(get_validity(selected_block->val_next) == 0){
      printk(KERN_INFO "%s: the block requested is not valid", MODNAME);
      return 0;
   }

   
   // increase read counter of 1 unit
   printk("CTR vecchio valore: %lu\n", *(selected_block->ctr));
   __sync_fetch_and_add(selected_block->ctr,1);
   printk("CTR nuovo valore: %lu\n", *(selected_block->ctr));
   
      

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


   // get cached data
   block_to_read = offset(offset);
   bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_read);
   if(!bh){
      return -EIO;
   }
   if (bh->b_data != NULL)
      AUDIT printk(KERN_INFO "%s: [blocco %d] ho letto -> %s\n", MODNAME, block_to_read, bh->b_data);

   
   // ritorna il numero di byte che non ha potuto copiare
   ret = copy_to_user(destination, bh->b_data, size);

   __sync_fetch_and_sub(selected_block->ctr,1);
   printk("CTR reset al valore: %lu\n", *(selected_block->ctr));
   
   brelse(bh);
   blkdev_put(bdev, FMODE_READ);
   return size-ret;

}





// INVALIDATE DATA

int dev_invalidate_data(int offset){
   printk("INVALIDATE DATA\n");

   int i;
   int block_to_write;
   struct block_node *cas;
   struct block_node *next;
   struct block_node *selected;
   struct block_node *predecessor;
   
   unsigned long *cas_counter;
   unsigned long *old_counter;
   unsigned long *counter = NULL;


   selected = &block_metadata[offset];

   // if the block is already invalid there is nothing to do   
   if(get_validity(selected->val_next) == 0){
      AUDIT printk("%s: the block %d is already invalid, nothing to do\n", MODNAME, offset);
      return 0;
   }
   
   
   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't write from invalid block device name, your filesystem is not mounted", MODNAME);
      return -1;                          // return ENODEV ? settare ERRNO ?
   }
   

   predecessor = valid_messages;
   next = get_pointer(selected->val_next);

   // get the predecessor of the block to invalidate
   while(get_pointer(predecessor->val_next) != &block_metadata[offset]){
      printk("sono il blocco: %d\n", predecessor->num);
      predecessor = get_pointer(predecessor->val_next);
   }
   printk("il predecessore è al blocco: %d\n", predecessor->num);


   // need 2 write locks!
   mutex_lock(&(selected->lock));
   if(next != change_validity(NULL))
      mutex_lock(&(next->lock));
   

   // invalido il blocco
   selected->val_next = change_validity(selected->val_next);                                               
   AUDIT printk("ora il blocco selezionato ha val_next: %px\n", selected->val_next);

   // sgancio l'elemento               DOVE                 COSA C'ERA              COSA CI METTO
   // cas = __sync_val_compare_and_swap(&(selected->val_next), change_validity(NULL), selected_block);    
   cas = __sync_val_compare_and_swap(&(predecessor->val_next), predecessor->val_next, change_validity(selected->val_next));    
   

   if(cas == change_validity(selected->val_next)){
      // FALLIMENTO - ricevo il valore che volevo inserire
      printk("%s: La CAS è fallita, valore trovato: %px - valore atteso: %px\n", MODNAME, predecessor->val_next, change_validity(selected->val_next));
      if(next != change_validity(NULL)) mutex_unlock(&(next->lock));
      mutex_unlock(&(selected->lock));
      return -1;
   }
   

   // SUCCESSO - ricevo il valore vecchio
   printk("%s: La CAS ha avuto successo prev.next = %px, invalidated.next = %px\n", MODNAME, predecessor->val_next, selected->val_next);
      
      
   // allocare il nuovo contatore

   counter = (unsigned long *) kmalloc(sizeof(unsigned long), GFP_KERNEL);
   printk("kmalloc done");
   if(counter == NULL){
      printk("%s: kmalloc error, can't allocate memory needed to create new counter to redirect readers\n",MODNAME);
      return -1;
   }
   *counter = 0;
   printk("new ctr ok");


   // spostare il contatore da quello vecchio a quello nuovo
   cas_counter = __sync_val_compare_and_swap(&(selected->ctr), selected->ctr, counter);    
   printk("new cas");   

   if(cas_counter == counter){
      // FALLIMENTO - ricevo il valore che volevo inserire
      printk("%s: La CAS è fallita, valore trovato: %px - valore atteso: %px\n", MODNAME, selected->ctr, counter);
      if(next != change_validity(NULL)) mutex_unlock(&(next->lock));
      mutex_unlock(&(selected->lock));
      return -1;
   }
   

   // SUCCESSO - ricevo il valore vecchio
   printk("%s: La CAS ha avuto successo block->counter = %px, allocated_counter = %px\n", MODNAME, selected->ctr, counter);
   

   // while(*cas_counter != 0){
   //    printk("CTR (blocco %d) = %d\n", offset, *cas_counter);
   DECLARE_WAIT_QUEUE_HEAD(wqueue);
   wait_event_interruptible(wqueue, *(cas_counter) == 0);

   printk("ho finito");

   if(next != change_validity(NULL)) mutex_unlock(&(next->lock));
   mutex_unlock(&(selected->lock));  



   return 0;
}





// READ - FILE OPERATION

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

   // LETTURA DI TUTTI I BLOCCHI IN ORDINE

   int ret; 
   int lenght;
   int copied;
   char* temp_buf;
   char termination;  
   int block_to_read;
   struct block_node *next;
   struct block_node *curr;
   struct block_device *bdev;
   int minor = get_minor(filp);
   int major = get_major(filp);
   struct buffer_head *bh = NULL;


   AUDIT{
      printk(KERN_INFO "%s: somebody called a read on dev with [major,minor] number [%d,%d], len = %d\n",MODNAME, major, minor, len);
      printk(KERN_INFO "%s: %s is the block device name\n", MODNAME, block_device_name);
   }

   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't read from invalid block device name, your filesystem is not mounted", MODNAME);
      return -1;                          // return ENODEV ?
   }

   // get the block device in order to reach cached data

   bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);

   if(bdev == NULL){
      printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
      return -1;                          // return ENODEV ?
   }


   // get the first element to read: the block pointed by the HEAD

   curr = valid_messages;
   next = get_pointer(curr->val_next);
   
   if(next == change_validity(NULL)){
      // no valid elements (HEAD->next is NULL), just return
      blkdev_put(bdev, FMODE_READ);
      return 0;
   }

   // there is at least one valid element, signal the presence of reader on this element
     
   AUDIT printk("(blocco:%d) prima posizione\n CTR vecchio valore: %lu\n", next->num, *(next->ctr));
   __sync_fetch_and_add(next->ctr,1);
   AUDIT printk(" CTR nuovo valore: %lu\n", *(next->ctr));
   
   
   copied = 0;
   termination = '\n';
   block_to_read = offset(next->num);


   // iterate on the blocks until the next is NULL
   
   while(next != change_validity(NULL)){      
      
      curr = next;
      next = get_pointer(curr->val_next);
      
      // get cached data
      bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_read);

      if(!bh){
         return -EIO;
      }
      if (bh->b_data == NULL) continue;
      
      lenght = strlen(bh->b_data);
      AUDIT printk(KERN_INFO " data: %s\n", bh->b_data);


      // allocate and use the temp buffer in order to modify 
      // the retrieved message with the termination character

      temp_buf = kmalloc(lenght+1, GFP_KERNEL);
      if(!temp_buf){
         printk("%s: kmalloc error, unable to allocate memory for read messages as single file\n", MODNAME);
         return -1;
      }

      strncpy(temp_buf, bh->b_data, lenght);
      
      if(next == change_validity(NULL)) termination = '\0';
      temp_buf[lenght] = termination;
      
      ret = copy_to_user(buff + copied, temp_buf, strlen(bh->b_data)+1);
      copied = copied + lenght + 1 - ret;
      
      if(next != change_validity(NULL)){
         
         // signal the presence of reader on the NEXT element
         block_to_read = offset(next->num);
         AUDIT printk("(blocco:%d) CTR vecchio valore: %lu\n\n", next->num, *(next->ctr));
         __sync_fetch_and_add(next->ctr,1);
         AUDIT printk(" CTR nuovo valore: %lu\n", *(next->ctr));

      }

      // release the presence counter for the current block

      __sync_fetch_and_sub(curr->ctr,1);
      AUDIT printk(" CTR reset al valore: %lu\n", *(curr->ctr));   
      brelse(bh);

   }

   kfree(temp_buf);
   blkdev_put(bdev, FMODE_READ);
   return len;
   
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




// WRITE - DA TOGLIEREEEEEEEEEEEEEEEEEEEEEEE

// static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {


//    // SCRITTURA DI UN BLOCCO DEL BLOCK DEVICE - sovrascrive completamente il blocco

//    int ret;
//    char* cas;
//    char* message;
//    char* old_message;   
//    int block_to_write;
//    int minor = get_minor(filp);
//    int major = get_major(filp);
//    struct buffer_head *bh = NULL;
//    struct block_device *bdev;


//    block_to_write = minor + 2;             // the value 2 accounts for superblock and file-inode on device

//    AUDIT{
//       printk(KERN_INFO "%s: somebody called a write on dev with [major,minor] number [%d,%d] so the block to access is minor+2 = %d\n",MODNAME,major, minor, block_to_write);
//       printk(KERN_INFO "%s: %s is the block device name\n", MODNAME, block_device_name);
//    }

//    if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
//       printk("%s: can't write from invalid block device name, your filesystem is not mounted", MODNAME);
//       return -1;                          // return ENODEV ? settare ERRNO ?
//    }

//    // get the block device in order to reach cached data
//    bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);
   
//    if(bdev == NULL){
//       printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
//       return -1;                          // return ENODEV ?
//    }


//    // valore da inserire
	
//    message = (char*) kmalloc(DEFAULT_BLOCK_SIZE, GFP_KERNEL);
// 	if(message == NULL){
// 		printk("%s: kmalloc error, unable to allocate memory for receiving the user message\n",MODNAME);
// 		return -1;
// 	}

// 	ret = copy_from_user(message, buff, len);
// 	message[len]='\0';
// 	printk(KERN_INFO "%s: [blocco %d] valore da inserire: %s", MODNAME, block_to_write, message);



//    // prendo i dati da aggiornare

//    bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_write);
//    if(!bh){
//       return -EIO;
//    }
//    if (bh->b_data != NULL){      // e se è null che succede?
      
//       old_message = bh->b_data;
//       AUDIT printk(KERN_INFO "%s: [blocco %d] valore vecchio: %s\n", MODNAME, block_to_write, bh->b_data);
   
//    }

   
//    // aggiornamento atomico

//    cas = __sync_val_compare_and_swap(&(bh->b_data), old_message, message);

//    if(strncmp(cas, old_message, strlen(old_message)) == 0){
//       // ha avuto successo
//       printk("%s: La CAS ha avuto successo bh->b_data = %s\n", MODNAME, bh->b_data);
//    }
//    else {
//       // fallimento
//       printk("%s: La CAS ha fallito, valore trovato: %s - valore atteso: %s\n", MODNAME, cas, old_message);
//    } 
   
   
//    brelse(bh);
//    blkdev_put(bdev, FMODE_READ);

//    return DEFAULT_BLOCK_SIZE;
   

// }



static struct file_operations fops = {
//   .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl
};