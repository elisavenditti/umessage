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
   int major;
   int minor;
   struct buffer_head *bh = NULL;
   struct buffer_head *bh1 = NULL;
   struct buffer_head *bh2 = NULL;
   // int minor = get_minor(filp);
   // int major = get_major(filp);
   int ret;   
   int block_to_read = minor + 2;    // index of the block to be read from device, the value 2 accounts for superblock and file-inode on device


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   major = MAJOR(filp->f_inode->i_rdev);
   minor = MINOR(filp->f_inode->i_rdev);
   
#else
   major = MAJOR(filp->f_dentry->d_inode->i_rdev);
   minor = MINOR(filp->f_dentry->d_inode->i_rdev);

#endif

   printk(KERN_INFO "%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,major, minor);
   printk(KERN_INFO "%s: read operation called with len %ld - and offset %lld (not used)\n",MODNAME, len, *off);
 
   printk("%s: read operation must access block %d of the device",MODNAME, block_to_read);

   struct block_device *bdev_dev_umessage = filp->f_inode->i_sb->s_bdev;
   struct block_device *bdev_dev_loop = blkdev_get_by_path("/dev/loop15", FMODE_READ|FMODE_WRITE, filp->f_inode->i_sb);

   
   if(bdev_dev_loop==NULL)
      printk("%s: non sono riuscita a prendere bdev di /dev/loop15",MODNAME);
   if(bdev_dev_loop==NULL)
      printk("%s: non sono riuscita a prendere bdev di /dev/loop15",MODNAME);

   printk("%s: /dev/loop1 = %d - /dev/umessage0 = %d\n",MODNAME, &(bdev_dev_loop)->bd_dev, &(bdev_dev_umessage)->bd_dev);




   printk("LETTURA DA BLOCCO /dev/loop\n");
   bh2 = (struct buffer_head *)__bread(bdev_dev_loop, 3, (unsigned)512);
   if(!bh2){
	   return -EIO;
   }
   if (bh2->b_data != NULL) printk("   ho letto -> %s\n", bh2->b_data);

   printk("sono qui7");




   printk("LETTURA DA BLOCCO /dev/umessage0\n");
   bh1 = (struct buffer_head *)__bread(bdev_dev_umessage, 3, (unsigned) 10);
   if(!bh1){
	   return -EIO;
   }


   if (bh1->b_data != NULL) printk("   ho letto -> %s\n", bh1->b_data);

   
   return 0;
   bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb /*quale SB ci mettiamo insomma?*/, block_to_read);
   if(!bh){
	   return -EIO;
   } 
   ret = copy_to_user(buff,bh->b_data, DEFAULT_BLOCK_SIZE);
   brelse(bh);

   return DEFAULT_BLOCK_SIZE - ret;
   

}




static struct file_operations fops = {
  .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release
};