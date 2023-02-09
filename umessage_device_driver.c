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
// #include "umessage_header.h"



// DEFINIZIONI

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static ssize_t dev_read (struct file *, char *, size_t, loff_t *);

static int Major;            /* Major number assigned to broadcast device driver */
// static DEFINE_MUTEX(device_state);



// IMPLEMENTAZIONE DELLE OPERAZIONI

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


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_inode->i_rdev),MINOR(filp->f_inode->i_rdev));
#else
   printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_dentry->d_inode->i_rdev),MINOR(filp->f_dentry->d_inode->i_rdev));
#endif
//  return len;
  return 1;

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
      printk(KERN_INFO "%s is the block device name\n", block_device_name);
   }

   if(block_device_name == NULL || strcmp(block_device_name, "") == 0){
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
   if (bh->b_data != NULL){
      AUDIT printk("[blocco %d] ho letto -> %s\n", 2, bh->b_data);
   
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
  .release = dev_release
};