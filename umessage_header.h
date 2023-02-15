#ifndef _UMESSAGE_H
#define _UMESSAGE_H

#include <linux/version.h>
#include <linux/ioctl.h>

#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"  /* Device file name in /dev/ - not mandatory  */
#define DEFAULT_BLOCK_SIZE 4096
#define NBLOCKS 10


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif


// casi della ioctl
#define MAGIC_UMSG      (0xEF)
#define PUT_DATA        _IOW(MAGIC_UMSG, 1, struct put_args)    // forse il num di sequenza deve partire da 0
#define GET_DATA        _IOR(MAGIC_UMSG, 2, struct get_args)
#define INVALIDATE_DATA _IOW(MAGIC_UMSG, 3, int)

struct put_args{
    char* source;
    size_t size;
};

struct get_args{
    int offset;
    char * destination;
    size_t size;
};



// block content definition
// struct bdev_block {
//     int next;                   // index of next block
// 	char message[DEFAULT_BLOCK_SIZE - sizeof(int)];
// };

struct block_node {
    struct block_node *val_next;        // il primo bit si riferisce alla validità del blocco corrente
                                        // prima di modificare questo valore va impostata la validità corretta
    int num;
    //struct mutex lock;
    unsigned long *ctr;
};

extern struct block_node block_metadata[NBLOCKS];
extern struct block_node* valid_messages;


//block device to contact in order to get data - it is initialized when the FS is mounted
extern char block_device_name[20];



#define change_validity(ptr)    (struct block_node *)((unsigned long) ptr^(0x8000000000000000))
#define get_pointer(ptr)        (struct block_node *)((unsigned long) ptr|(0x8000000000000000))
#define get_validity(ptr)       ((unsigned long) ptr >> (sizeof(unsigned long) * 8 - 1))


#endif