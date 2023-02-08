#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"  /* Device file name in /dev/ - not mandatory  */
#define DEFAULT_BLOCK_SIZE 4096
#define NBLOCKS 10


// #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
// #define get_major(session)	MAJOR(session->f_inode->i_rdev)
// #define get_minor(session)	MINOR(session->f_inode->i_rdev)
// #else
// #define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
// #define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
// #endif


// block content definition
struct bdev_block {
    int next;                   // index of next block
	char message[DEFAULT_BLOCK_SIZE - sizeof(int)];
};
