#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "../umessage_header.h"
#include "umessagefs.h"



struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {

    struct onefilefs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    printk("%s: running the lookup inode-function for name %s",MODNAME,child_dentry->d_name.name);

    if(!strcmp(child_dentry->d_name.name, UNIQUE_FILE_NAME)){

	
		// get a locked inode from the cache 
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
       		 return ERR_PTR(-ENOMEM);

		// already cached inode - simply return successfully
		if(!(the_inode->i_state & I_NEW)){
			return child_dentry;
		}


		// this work is done if the inode was not already cached
		inode_init_owner(&init_user_ns, the_inode, NULL, S_IFREG );
		the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
		
		// the file operations are defined in the device driver !!
		the_inode->i_fop = &fops;
		the_inode->i_op = &onefilefs_inode_ops;

		//just one link for this file
		set_nlink(the_inode,1);

		//now we retrieve the file size via the FS specific inode, putting it into the generic inode
		bh = (struct buffer_head *)sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER );
		if(!bh){
			iput(the_inode);
			return ERR_PTR(-EIO);
		}

		FS_specific_inode = (struct onefilefs_inode*)bh->b_data;
		the_inode->i_size = FS_specific_inode->file_size;
		brelse(bh);

		d_add(child_dentry, the_inode);
		dget(child_dentry);

		// unlock the inode to make it usable 
		unlock_new_inode(the_inode);

		return child_dentry;
    }

    return NULL;

}



//look up goes in the inode operations
const struct inode_operations onefilefs_inode_ops = {
    .lookup = onefilefs_lookup,
};