#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "umessagefs.h"



unsigned long bdev_usage = 0;
struct block_device *bdev = NULL;
char block_device_name[20] = " ";
DECLARE_WAIT_QUEUE_HEAD(umount_queue);


static struct super_operations singlefilefs_super_ops = {
};


static struct dentry_operations singlefilefs_dentry_ops = {
};


// utils functions
int do_init(void);
int do_persistence(struct block_device*);
int dimension_check(void);


int singlefilefs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct buffer_head *bh;
    struct onefilefs_sb_info *sb_disk;
    struct timespec64 curr_time;
    uint64_t magic;
    

    // Unique identifier of the filesystem
    sb->s_magic = MAGIC;

    bh = sb_bread(sb, SB_BLOCK_NUMBER);
    if(!sb){
	return -EIO;
    }
    sb_disk = (struct onefilefs_sb_info *)bh->b_data;
    magic = sb_disk->magic;
    brelse(bh);

    // check on the expected magic number
    if(magic != sb->s_magic){
	return -EBADF;
    }

    sb->s_fs_info = NULL;                               // FS specific data (the magic number) already reported into the generic superblock
    sb->s_op = &singlefilefs_super_ops;                 // set our own operations


    root_inode = iget_locked(sb, 0);                    // get a root inode indexed with 0 from cache
    if (!root_inode){
        return -ENOMEM;
    }

    root_inode->i_ino = SINGLEFILEFS_ROOT_INODE_NUMBER; // this is actually 10
    inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);// set the root user as owned of the FS root
    root_inode->i_sb = sb;
    root_inode->i_op = &onefilefs_inode_ops;            // set our inode operations
    root_inode->i_fop = &onefilefs_dir_operations;      // set our file operations
    
    // update access permission
    root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;

    // baseline alignment of the FS timestamp to the current time
    ktime_get_real_ts64(&curr_time);
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

    // no inode from device is needed - the root of our file system is an in memory object
    root_inode->i_private = NULL;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root)
        return -ENOMEM;

    sb->s_root->d_op = &singlefilefs_dentry_ops;        // set our dentry operations

    //unlock the inode to make it usable
    unlock_new_inode(root_inode);

    return 0;
}

static void singlefilefs_kill_superblock(struct super_block *s) {
    struct block_device *temp_bdev = bdev;
    
    strncpy(block_device_name, " ", 1);
    block_device_name[1] = '\0';
    
    bdev = NULL;
    printk("%s: waiting the pending threads (%d)...", MODNAME, bdev_usage);
    wait_event_interruptible(umount_queue, bdev_usage == 0);
    
    // update metadata in the device - no concurrency
    do_persistence(temp_bdev);
    blkdev_put(temp_bdev, FMODE_READ);
    kill_block_super(s);
    
    printk(KERN_INFO "%s: singlefilefs unmount succesful.\n",MODNAME);
    return;
}


// called on file system mounting 
struct dentry *singlefilefs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    int len;
    struct dentry *ret;

    // check if the driver can manage the messages
    if(dimension_check()){
        printk("%s: error - the device driver can support up to %d blocks\n", MODNAME, MAXBLOCKS);
        return -EINVAL;
    }

    // check if the FS is already mounted
    if(bdev != NULL){
        printk("%s: error - the device driver can support single mount at time\n", MODNAME);
        return -EBUSY;
    }
    
    ret = mount_bdev(fs_type, flags, dev_name, data, singlefilefs_fill_super);

    if (unlikely(IS_ERR(ret)))
        printk("%s: error mounting onefilefs",MODNAME);
    
    else {

        // get the name of the loop device in order to access data later on        
        len = strlen(dev_name);        
        strncpy(block_device_name, dev_name, len);
        block_device_name[len]='\0';


        // get the associated block device

        bdev = blkdev_get_by_path(block_device_name, FMODE_READ|FMODE_WRITE, NULL);

        if(bdev == NULL){
            printk("%s: can't get the struct block_device associated to %s", MODNAME, block_device_name);
            return -EINVAL;
        }

        // retrieve the state of blocks from device
        do_init();
        
        printk("%s: singlefilefs is succesfully mounted on from device %s\n", MODNAME, dev_name);

    }
    return ret;
}



// file system structure
static struct file_system_type onefilefs_type = {
	.owner = THIS_MODULE,
    .name           = "singlefilefs",
    .mount          = singlefilefs_mount,
    .kill_sb        = singlefilefs_kill_superblock,
};





int dimension_check(void){
    
    struct file *filp;
    loff_t size;
    sector_t num_blocks;

    filp = filp_open("./image", O_RDONLY, 0);
    if (IS_ERR(filp)) {
        printk("%s: Error opening image file\n", MODNAME);
        return 1;
    }

    size = vfs_llseek(filp, 0, SEEK_END);
    if (size < 0) {
        printk("%s: Error getting image file size\n", MODNAME);
        filp_close(filp, NULL);
        return 1;
    }
    num_blocks = (size / DEFAULT_BLOCK_SIZE) -2;        //exclude metadata blocks
    printk("num_blocks = %llu, supported blocks = %d\n", num_blocks, MAXBLOCKS);
    
    if(num_blocks>MAXBLOCKS) return 1;
    return 0;

}


int do_init(void){

    // initialize the kernel structure to manage blocks with data from device
    // no concurrency considered: it is supported single filesystem mount

    int i;
    int next;
    int valid;
    int block_to_read;
    struct buffer_head *bh = NULL;
    struct block_node* successor;
    struct block_node* selected_block = NULL;
    struct bdev_node* bnode = NULL;

    // previous of each block
    int prev[NBLOCKS] = {[0 ... NBLOCKS-1] = HEAD};

    for(i=0; i<NBLOCKS; i++){

        block_to_read = offset(i);
        bh = (struct buffer_head *) sb_bread(bdev->bd_super, block_to_read);
        if(!bh){
            return -EIO;
        }
        if (bh->b_data != NULL){
            AUDIT printk("%s: [blocco %d]\n", MODNAME, block_to_read);   

            bnode = (struct bdev_node*) bh->b_data;
            next = set_invalid(bnode->val_next);
            valid = get_validity_int(bnode->val_next);
            
            AUDIT{
                printk("%s:     next = %d\n", MODNAME, next);
                printk("%s:     validity = %d\n", MODNAME, valid);
                printk("%s:     message = %s\n", MODNAME, bnode->data);
            }

            // first step - get the next value (VALID)
            if(next == NULL) successor = change_validity(NULL);
            else successor = &block_metadata[next-2];

            // second step - check the validity
            if(!valid){
                prev[i] = INVALID;
                change_validity(successor);
            }
            
            // -2 because next takes into account also the superblock and the inode
            else if (next != 0) prev[next-2] = i;
            
            
            block_metadata[i].val_next = successor;

            printk("%s:     block_metadata[i].val_next = %px\n", MODNAME, block_metadata[i].val_next);
            printk("%s:     &block_metadata[next] = %px\n", MODNAME, successor);
            brelse(bh);            
        }

    
    }

    // update the pointer to the head element
    for(i=0; i<NBLOCKS; i++){
        if(prev[i] == HEAD){
            valid_messages->val_next = &block_metadata[i];
            AUDIT printk("%s: HEAD AT %d\n", MODNAME, i);
        }
    }
    return 0;
}



int do_persistence(struct block_device *temp_bdev){

    // Update device content with kernel structure's data
    // threads cannot modify values in cache because global bdev is NULL
    // temp_bdev is the last reference to the struct and this thread is the only owner

    int i;
    int next;
    int valid;
    int block_to_write;
    struct buffer_head *bh = NULL;
    struct block_node* successor;
    struct block_node* selected_block = NULL;
    struct bdev_node* bnode = NULL;

    
    printk("PERSISTENCE CALLED\n");
    for(i=0; i<NBLOCKS; i++){

        block_to_write = offset(i);
        bh = (struct buffer_head *) sb_bread(temp_bdev->bd_super, block_to_write);
        if(!bh){
            return -EIO;
        }

        if (bh->b_data != NULL){
            printk("%s:[blocco %d]\n", MODNAME, block_to_write);   

            valid = get_validity(block_metadata[i].val_next);
            successor = get_pointer(block_metadata[i].val_next);

            // if successor was NULL the get_pointer operation set the leftmost bit to 1
            if(successor == change_validity(NULL)) next = NULL;
            else next = successor->num + 2;
            printk("%s:     next = %d\n", MODNAME, next);
            printk("%s:     validity = %d\n", MODNAME, valid);

            
            if(valid)   next = set_valid(next);
            else        next = set_invalid(next);

            // write into cache and flush data to device
            memcpy(&(((struct bdev_node *)bh->b_data)->val_next), &next, sizeof(unsigned int));            
            // memcpy(bh->b_data, &next, sizeof(unsigned int));
            mark_buffer_dirty(bh);
            if(sync_dirty_buffer(bh) == 0) printk("SUCCESS IN SYNCHRONOUS WRITE");
            else printk("FAILURE IN SYNCHRONOUS WRITE");
            
            
            brelse(bh);
        }
    }

    return 0;
}



