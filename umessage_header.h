#ifndef _UMESSAGE_H
#define _UMESSAGE_H

#include <linux/version.h>
#include <linux/ioctl.h>



#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"
#define DEV_NAME "./mount/the-file"
#define DEFAULT_BLOCK_SIZE 4096
#define METADATA_SIZE sizeof(void*)
#define DATA_SIZE DEFAULT_BLOCK_SIZE - METADATA_SIZE
#define NBLOCKS 5


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif



struct block_node {
    struct block_node *val_next;        // il primo bit si riferisce alla validità del blocco corrente
                                        // prima di modificare questo valore va impostata la validità corretta
    int num;
    struct mutex lock;                  // mutex_(un)lock(&queue_lock); 
};

//valuta se toglierlo
struct bdev_node {
    unsigned long val_next;
    char data[];
};


extern const struct file_operations fops;
extern struct block_node block_metadata[NBLOCKS];
extern struct block_node* valid_messages;
extern int Major;
extern unsigned long pending[2];
extern unsigned long epoch;
extern int next_epoch_index;
extern unsigned long bdev_usage;
extern wait_queue_head_t umount_queue;

//block device to contact in order to get data - it is initialized when the FS is mounted
extern struct block_device *bdev;


int dev_put_data(char *, size_t);
int dev_invalidate_data(int);
int dev_get_data(int, char *, size_t);



#define MASK 0x8000000000000000
#define change_validity(ptr)    (struct block_node *)((unsigned long) ptr^MASK)
#define get_pointer(ptr)        (struct block_node *)((unsigned long) ptr|MASK)
#define get_validity(ptr)       ((unsigned long) ptr >> (sizeof(unsigned long) * 8 - 1))
#define offset(val)             val + 2
#define data_offset(ptr)        ptr + DATA_SIZE


#endif