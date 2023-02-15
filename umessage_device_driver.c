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
// int            dev_put_data(struct put_arg *);
int            dev_put_data(char *, size_t);


static int Major;
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
         // ret = ...
         // (struct get_arg*) arg
         break;
      case INVALIDATE_DATA:
         // ret = ...
         // (int *) arg
         break;
      default:
         return -EINVAL;
         
   }
   
   return ret;

}



// PUT DATA

int dev_put_data(char* source, size_t size){
   
   int i;
   struct block_node *tail;
   struct block_node current_block;
   struct block_node selected_block;

   // get an invalid block to overwrite
   for(i=0; i<NBLOCKS; i++){
      
      current_block = block_metadata[i];
      if(get_validity(current_block.val_next) == 0){
         
         selected_block = current_block;
      }
   }

   printk("il blocco invalido è il numero %d\n", selected_block.num);

   // get the tail of the valid blocks (ordered write)
   tail = valid_messages;
   while(get_pointer(tail->val_next) != NULL){
      tail = get_pointer(tail->val_next);
   }
   if(tail == NULL)  printk("la lista è vuota, coda=testa=NULL\n");
   else              printk("la coda è al blocco: %d\n", tail->num);



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

   // the block device is essential to reach the actual data to read

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




static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

   // LETTURA DI UN BLOCCO DEL BLOCK DEVICE - ritorna tutti i dati associati ad un blocco

   int ret;   
   int block_to_read;
   int minor = get_minor(filp);
   int major = get_major(filp);
   struct buffer_head *bh = NULL;
   struct block_device *bdev;


   block_to_read = minor + 2;             // the value 2 accounts for superblock and file-inode on device

   AUDIT{
      printk(KERN_INFO "%s: somebody called a read on dev with [major,minor] number [%d,%d] so the block to access is minor+2 = %d\n",MODNAME,major, minor, block_to_read);
      printk(KERN_INFO "%s: %s is the block device name\n", MODNAME, block_device_name);
   }

   if(block_device_name == NULL || strcmp(block_device_name, " ") == 0){
      printk("%s: can't read from invalid block device name, your filesystem is not mounted", MODNAME);
      return -1;                          // return ENODEV ?
   }

   // the block device is essential to reach the actual data to read

   bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);
   
   if(bdev == NULL){
      printk("%s: can't get the struct block_device associated to %s",MODNAME, block_device_name);
      return -1;                          // return ENODEV ?
   }


   bh = (struct buffer_head *)sb_bread(bdev->bd_super, block_to_read);
   if(!bh){
      return -EIO;
   }
   if (bh->b_data != NULL)
      AUDIT printk(KERN_INFO "[blocco %d] ho letto -> %s\n", block_to_read, bh->b_data);
   
   // ritorna il numero di byte che non ha potuto copiare
   ret = copy_to_user(buff,bh->b_data, DEFAULT_BLOCK_SIZE);
   brelse(bh);
   blkdev_put(bdev, FMODE_READ);


   return DEFAULT_BLOCK_SIZE - ret;
   
}






static struct file_operations fops = {
  .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl
};