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
   // int found;
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
   }
   printk("la coda è al blocco: %d\n", tail->num);
   
   

   // write lock on the selected block
   mutex_lock(&(selected_block->lock));
   
   selected_block->val_next = change_validity(NULL);                                                // Il NULL ha come primo bit 0 (invalido) e bisogna 
                                                                                                   // validarlo prima di inserirlo nei metadati.
   printk("ora il blocco selezionato ha val_next: %px\n", selected_block->val_next);
   cas = __sync_val_compare_and_swap(&(tail->val_next), change_validity(NULL), selected_block);    // ALL OR NOTHING - scambio il campo next della coda (NULL)
                                                                                                   // con il puntatore all'elemento che sto inserendo
                                                                                                   // (ptr nel kernel ha bit a sx = 1, valido). Il bit di validità nel
                                                                                                   // puntatore garantisce il fallimento in caso di interferenze

   if(cas != change_validity(NULL)){
      // FALLIMENTO
      printk("%s: La CAS ha fallito, valore trovato: %px - valore atteso: %px\n", MODNAME, change_validity(tail->val_next), selected_block);
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
   printk("ho ricevuto %d\n", offset);
   return 0;
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




// READ - FILE OPERATION

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

   // LETTURA DI TUTTI I BLOCCHI IN ORDINE

   int ret; 
   int lenght;  
   int block_to_read;
   int minor = get_minor(filp);
   int major = get_major(filp);
   struct buffer_head *bh = NULL;
   struct block_device *bdev;

   struct block_node *next;
   struct block_node *curr;
   struct block_node *selected_block = NULL;


   // block_to_read = minor + 2;             // the value 2 accounts for superblock and file-inode on device

   AUDIT{
      printk(KERN_INFO "%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME, major, minor);
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
   printk("curr->next=%px\n", curr->val_next);
   if(get_pointer(curr->val_next) != change_validity(NULL)){
      
      curr = get_pointer(curr->val_next);
      printk("(blocco:%d) prima posizione\n", curr->num);
      printk(" CTR vecchio valore: %lu\n", *(curr->ctr));   
      __sync_fetch_and_add(curr->ctr,1);
      printk(" CTR nuovo valore: %lu\n", *(curr->ctr));
      block_to_read = offset(curr->num);

   }
   len = 0;
   // iterate for each valid element until the last one with NULL (but valid) pointer to next
   // while(get_pointer(curr->val_next) != change_validity(NULL)){       
   
   while(curr != change_validity(NULL)){      
      if(curr == NULL) printk("ERA NULL\n");
      if(curr == change_validity(NULL)) printk("ERA CHANGE_VALIDITY(NULL)\n");      
      // get cached data
      bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_read);
      if(!bh){
         return -EIO;
      }
      if (bh->b_data != NULL)
         AUDIT printk(KERN_INFO " data: %s\n", bh->b_data);

      
      
      // ritorna il numero di byte che non ha potuto copiare
      ret = copy_to_user(buff+lenght, bh->b_data, strlen(bh->b_data));
      lenght = lenght + strlen(bh->b_data);
      
      next = get_pointer(curr->val_next);
      if(next != change_validity(NULL)){
         block_to_read = offset(next->num);
         printk(" next block:%d\n", block_to_read);
         printk("(blocco:%d)\n", next->num);
      
         // riservo contatore per il prossimo blocco
         printk(" CTR vecchio valore: %lu\n", *(next->ctr));   
         __sync_fetch_and_add(next->ctr,1);
         printk(" CTR nuovo valore: %lu\n", *(next->ctr));

         // rilascio contatore blocco corrente
         __sync_fetch_and_sub(curr->ctr,1);
         printk(" CTR reset al valore: %lu\n", *(curr->ctr));
      }
      
      brelse(bh);
      
      // go to next block
      curr = next;
   }

   
   // rilascio contatore ultimo blocco, se la lista non era vuota
   // if(get_pointer(curr != change_validity(NULL)){
   //    __sync_fetch_and_sub(curr->ctr,1);
   //    printk(" CTR reset al valore: %lu\n", *(curr->ctr));
   //    printk("l'ultimo elemento valido è il blocco: %d\n", curr->num);
   // }
   blkdev_put(bdev, FMODE_READ);
   return 0;
   
}



// WRITE - DA TOGLIEREEEEEEEEEEEEEEEEEEEEEEE

static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {


   // SCRITTURA DI UN BLOCCO DEL BLOCK DEVICE - sovrascrive completamente il blocco

   int ret;
   char* cas;
   char* message;
   char* old_message;   
   int block_to_write;
   int minor = get_minor(filp);
   int major = get_major(filp);
   struct buffer_head *bh = NULL;
   struct block_device *bdev;


   block_to_write = minor + 2;             // the value 2 accounts for superblock and file-inode on device

   AUDIT{
      printk(KERN_INFO "%s: somebody called a write on dev with [major,minor] number [%d,%d] so the block to access is minor+2 = %d\n",MODNAME,major, minor, block_to_write);
      printk(KERN_INFO "%s: %s is the block device name\n", MODNAME, block_device_name);
   }

   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't write from invalid block device name, your filesystem is not mounted", MODNAME);
      return -1;                          // return ENODEV ? settare ERRNO ?
   }

   // get the block device in order to reach cached data
   bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);
   
   if(bdev == NULL){
      printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
      return -1;                          // return ENODEV ?
   }


   // valore da inserire
	
   message = (char*) kmalloc(DEFAULT_BLOCK_SIZE, GFP_KERNEL);
	if(message == NULL){
		printk("%s: kmalloc error, unable to allocate memory for receiving the user message\n",MODNAME);
		return -1;
	}

	ret = copy_from_user(message, buff, len);
	message[len]='\0';
	printk(KERN_INFO "%s: [blocco %d] valore da inserire: %s", MODNAME, block_to_write, message);



   // prendo i dati da aggiornare

   bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_write);
   if(!bh){
      return -EIO;
   }
   if (bh->b_data != NULL){      // e se è null che succede?
      
      old_message = bh->b_data;
      AUDIT printk(KERN_INFO "%s: [blocco %d] valore vecchio: %s\n", MODNAME, block_to_write, bh->b_data);
   
   }

   
   // aggiornamento atomico

   cas = __sync_val_compare_and_swap(&(bh->b_data), old_message, message);

   if(strncmp(cas, old_message, strlen(old_message)) == 0){
      // ha avuto successo
      printk("%s: La CAS ha avuto successo bh->b_data = %s\n", MODNAME, bh->b_data);
   }
   else {
      // fallimento
      printk("%s: La CAS ha fallito, valore trovato: %s - valore atteso: %s\n", MODNAME, cas, old_message);
   } 
   
   
   brelse(bh);
   blkdev_put(bdev, FMODE_READ);

   return DEFAULT_BLOCK_SIZE;
   

}



static struct file_operations fops = {
  .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl
};