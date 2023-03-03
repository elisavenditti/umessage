#ifndef _UMESSAGE_H
#define _UMESSAGE_H

#include <linux/version.h>
#include <linux/ioctl.h>

// STANDARD DEFINE
#define FORCE_SYNC
#define TEST
#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"
#define DEV_NAME "./mount/the-file"
#define DEFAULT_BLOCK_SIZE 4096
#define METADATA_SIZE sizeof(unsigned int)
#define DATA_SIZE (DEFAULT_BLOCK_SIZE - METADATA_SIZE)
#define NBLOCKS 4
#define MAXBLOCKS 5
#define PERIOD 30
#define PATH_TO_IMAGE "/home/elisa/Scrivania/umessage/image"

// MAJOR&MINOR UTILS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif


// KERNEL METADATA to manage messages
struct block_node {
    struct block_node *val_next;                            // the leftmost bit indicate the validity of the current block
    int num;                                                // number of the block (ignores the super block and the inode)
};

// DEVICE'S BLOCK LAYOUT - it is modified only during the unmounting of the filesystem
struct bdev_node {
    char data[DATA_SIZE];
    unsigned int val_next;
};


// the access to these two variables is performed by the mount thread, readers and writers
// the field are always used together
struct bdev_metadata{
    unsigned long bdev_usage;                               // operations that should complete before deallocation of bdev
    struct block_device *bdev;                              // block device to get data(initialized when the FS is mounted)s
};


// the access to these two variable is performed only by the mount thread
// the field are always used together
struct mount_metadata{
    int mounted;
    char block_device_name[20];
};

// RCU UTILS - the field are used together 
struct counter{
        unsigned long pending[2];                           // reader that released the counter in the current epoch
        unsigned long epoch;                                // current epoch readers (leftmost bit indicate the current epoch)
        int next_epoch_index;                               // index to access pending[] in the next epoch
        struct mutex lock;                                  // write lock on the list
};



// KERNEL DATA STRUCTURES
extern const struct file_operations fops;                   // device driver exposed file operations
extern struct block_node* valid_messages;                   // head of the valid blocks
extern struct block_node block_metadata[MAXBLOCKS];         // all blocks of the device
extern wait_queue_head_t umount_queue;                      // wait queue to stop until the bdev can be deallocated
extern struct counter rcu;
extern struct bdev_metadata bdev_md;


// VFS NON-SUPPORTED FEATURES OF DEVICE DRIVER 
int dev_put_data(char *, size_t);
int dev_invalidate_data(int);
int dev_get_data(int, char *, size_t);


// KERNEL VAL_NEXT UTILS
#define MASK 0x8000000000000000
#define change_validity(ptr)    (struct block_node *)((unsigned long) ptr^MASK)
#define get_pointer(ptr)        (struct block_node *)((unsigned long) ptr|MASK)
#define get_validity(ptr)       ((unsigned long) ptr >> (sizeof(unsigned long) * 8 - 1))
#define offset(val)             ((val) + 2)
#define data_offset(ptr)        ((ptr) + DATA_SIZE)

// DEVICE VAL_NEXT UTILS
#define VALID_MASK 0x80000000
#define INVALID_MASK (~VALID_MASK)//0x7FFFFFFF
#define set_valid(i)            ((unsigned int) (i) | VALID_MASK)
#define set_invalid(i)          ((unsigned int) (i) & INVALID_MASK)
#define get_validity_int(i)     ((unsigned int) (i) >> (sizeof(unsigned int) * 8 - 1))

// head search
#define HEAD -1
#define INVALID -2


#endif