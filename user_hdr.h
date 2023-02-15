#ifndef _UMESSAGE_H
#define _UMESSAGE_H

#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"  /* Device file name in /dev/ - not mandatory  */
#define DEFAULT_BLOCK_SIZE 4096
#define NBLOCKS 10

#include <linux/ioctl.h>
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

#endif