#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../user_hdr.h"
#include "umessagefs.h"

/*
	This makefs will write the following information onto the disk
	- BLOCK 0, superblock;
	- BLOCK 1, inode of the unique file (the inode for root is volatile);
	- BLOCK 2, metadata + data
	- ...
	- BLOCK N, metadata + data
*/


int main(int argc, char *argv[]){

	ssize_t ret;
	int i, fd, nbytes, nblocks;
	unsigned int metadata, next;
	struct onefilefs_sb_info sb;
	struct onefilefs_inode root_inode;
	struct onefilefs_inode file_inode;
	struct onefilefs_dir_record record;
	char *block_padding;


	if (argc != 3) {
		printf("Usage: mkfs-singlefilefs <device>\n");
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("Error opening the device");
		return -1;
	}

	nblocks = strtol(argv[2], NULL, 10);

	// pack the superblock

	sb.version = 1;								// file system version
	sb.magic = MAGIC;
	sb.block_size = DEFAULT_BLOCK_SIZE;

	ret = write(fd, (char *)&sb, sizeof(sb));

	if (ret != DEFAULT_BLOCK_SIZE) {
		printf("Bytes written [%d] are not equal to the default block size.\n", (int)ret);
		close(fd);
		return ret;
	}

	printf("Super block written succesfully\n");



	// write file inode

	file_inode.mode = S_IFREG;
	file_inode.inode_no = SINGLEFILEFS_FILE_INODE_NUMBER;
	file_inode.file_size = nblocks*DEFAULT_BLOCK_SIZE;
	printf("File size is %ld\n",file_inode.file_size);
	fflush(stdout);
	ret = write(fd, (char *)&file_inode, sizeof(file_inode));

	if (ret != sizeof(root_inode)) {
		printf("The file inode was not written properly.\n");
		close(fd);
		return -1;
	}

	printf("File inode written succesfully.\n");


		
	//padding for block 1
	
	nbytes = DEFAULT_BLOCK_SIZE - sizeof(file_inode);
	block_padding = malloc(nbytes);

	ret = write(fd, block_padding, nbytes);

	if (ret != nbytes) {
		printf("The padding bytes are not written properly. Retry your mkfs\n");
		close(fd);
		return -1;
	}
	printf("Padding in the inode block written sucessfully.\n");



	// write file datablock
	
	for(i=0; i<nblocks; i++){

		// data
		nbytes = strlen(testo[i]);
		if(nbytes > DATA_SIZE){
			printf("Data dimension not enough to contain text.\n");
			return -1;
		}
		ret = write(fd, testo[i], nbytes);
		if (ret != nbytes) {
			printf("Writing file datablock has failed.\n");
			close(fd);
			return -1;
		}
		
		// padding
		nbytes = DEFAULT_BLOCK_SIZE - strlen(testo[i]) - METADATA_SIZE;
		block_padding = malloc(nbytes);
		ret = write(fd, block_padding, nbytes);
		if (ret != nbytes) {
			printf("The padding bytes are not written properly. Retry your mkfs\n");
			close(fd);
			return -1;
		}

		// metadata	- all blocks are valid and point to the next one,
		// exept for the last one which has next pointer equal to NULL.
		// next = i+3 because it considers also the superblock and the 
		// inode in order to avoid confilcts in 0.

		next = (i+1 != nblocks) ? i+3 : 0;						

		metadata = set_valid(next);
    	ret = write(fd, &metadata, METADATA_SIZE);
		if (ret != METADATA_SIZE) {
			printf("Writing file metadata has failed.\n");
			close(fd);
			return -1;
		}


		printf("(%d/%d) File datablock has been written succesfully.\n", i+1, nblocks);

	}

	close(fd);

	return 0;
}