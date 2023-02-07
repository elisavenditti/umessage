#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"  /* Device file name in /dev/ - not mandatory  */
#define DEFAULT_BLOCK_SIZE 4096
#define NBLOCKS 10


// block content definition
struct bdev_block {
    int next;                   // index of next block
	char message[DEFAULT_BLOCK_SIZE - sizeof(int)];
};