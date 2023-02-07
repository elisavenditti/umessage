#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>	
#include <linux/pid.h>		         /* For pid types */
#include <linux/tty.h>		         /* For the tty declarations */
#include <linux/version.h>	         /* For LINUX_VERSION_CODE */
#include "my_header.h"



// DEFINIZIONI

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static ssize_t dev_read (struct file *, char *, size_t, loff_t *);

static int Major;            /* Major number assigned to broadcast device driver */


// static DEFINE_MUTEX(device_state);



// IMPLEMENTAZIONE DELLE OPERAZIONI

static int dev_open(struct inode *inode, struct file *file) {

// this device file is single instance
   // if (!mutex_trylock(&device_state)) {
	// 	return -EBUSY;
   // }

   printk("%s: device file successfully opened by thread %d\n",MODNAME,current->pid);
   //device opened by a default nop
   return 0;
}




static int dev_release(struct inode *inode, struct file *file) {

   // mutex_unlock(&device_state);

   printk("%s: device file closed by thread %d\n",MODNAME,current->pid);
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_inode->i_rdev),MINOR(filp->f_inode->i_rdev));
#else
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_dentry->d_inode->i_rdev),MINOR(filp->f_dentry->d_inode->i_rdev));
#endif
   return 0;

}




static struct file_operations fops = {
  .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release
};