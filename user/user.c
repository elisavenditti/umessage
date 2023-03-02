#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "../user_hdr.h"


#define INVALIDATE 134 //this depends on what the kernel tells you when mounting the printk-example module
#define PUT 	   156
#define GET	   	   174


char* get_input(int size, char* str){
	
	int i;
	char c;

	// get up to size-1 char 
	for(i = 0; i < size; i++) {
		
		(void) fread(&c, sizeof(char), 1, stdin);
		if(c == '\n') {
			str[i] = '\0';
			break;
		} else
			str[i] = c;
	}
	
	// assure the string terminator is included
	str[size-1] = '\0';

	// Flush the exceeding content
	if(strlen(str) >= size) {
		do {
			c = getchar();
		} while (c != '\n');
	}
	return str;

}



void get_data(void) {

	size_t size;
	int offset, ret;
	char* destination;
	char input[NUM_DIGITS(NBLOCKS)+1];
	char input2[NUM_DIGITS(DEFAULT_BLOCK_SIZE)+1];

	// first parameter
	printf("[GET DATA PARAMETERS]");
	printf("\n*Block number: ");
	get_input(NUM_DIGITS(NBLOCKS)+1, input);
	offset = strtol(input, NULL, 10);

	
	// second parameter
	printf("*Size to read: ");
	get_input( NUM_DIGITS(DEFAULT_BLOCK_SIZE)+1, input2);
	size = strtol(input2, NULL, 10);
	
	// invoke syscall
	destination = (char*) malloc(size);
	ret = syscall(GET, offset, destination, size);

	if(ret >= 0) 						printf("%d byte read from block %ld: '%s'\n", ret, offset, destination);
	else if(ret<0 && errno == EINVAL)	printf("error: invalid parameters\n");
	else if(ret<0 && errno == ENODATA)	printf("error: block requested is not valid\n");
	else if(ret<0 && errno == ENODEV)	printf("error: no filesystem mounted\n");
	else 								printf("generic error\n");
	

	free(destination);
	printf("\n\nPress enter to exit: ");
	while (getchar() != '\n');
	return;
			
}



void put_data(void) {

	int ret;
	char input[DATA_SIZE];
	
	printf("[PUT DATA PARAMETERS]");
	printf("\n*Message (up to %d bytes): ", DATA_SIZE);
	get_input(DATA_SIZE, input);

	ret = syscall(PUT, input, strlen(input));
	if(ret >= 0) 						printf("message succesfully inserted in block %d\n", ret);
	else if(ret<0 && errno == ENOMEM)	printf("error: the device is full of messages or there are problems in memory allocation\n");
	else if(ret<0 && errno == ENODEV)	printf("error: no filesystem mounted\n");
	else 								printf("generic error\n");

	printf("\n\nPress enter to exit: ");
	while (getchar() != '\n');
	return;

			
}


void invalidate_data(void) {
	
	int ret, offset;
	char input[NUM_DIGITS(NBLOCKS)];

	printf("[INVALIDATE DATA PARAMETERS]");
	printf("\n*Block number: ");
	get_input(NUM_DIGITS(NBLOCKS)+1, input);
	offset = strtol(input, NULL, 10);

	ret = syscall(INVALIDATE, offset);
	
	if(ret >= 0) 						printf("block %d succesfully invalidated\n", offset);
	else if(ret<0 && errno == EINVAL)	printf("error: invalid parameters\n");
	else if(ret<0 && errno == ENODATA) 	printf("error: block requested is already invalid, nothing to do\n");
	else if(ret<0 && errno == ENODEV)	printf("error: no filesystem mounted\n");
	else 								printf("generic error\n");
	

	printf("\n\nPress enter to exit: ");
	while (getchar() != '\n');
	return;

			
}



int main(int argc, char** argv){

 	int op1;
    long int arg;	
	char operation;
	int ret, ret_sys;
	size_t size = 10;
	char* lettura = malloc(size);
	char mount_command[1024];
	char input[2];


	while (1) {
		
		printf("%s\nSelect an option: ", homepage);
		get_input(2, input);
		op1 = strtol(input, NULL, 10);

		if (op1 > 6){
			printf("\nerror - incorrect input\n");
			return -1;
		}
		
		if (op1 == 1) put_data();
		else if (op1 == 2) get_data();
		else if (op1 == 3) invalidate_data();
		else if (op1 == 4) {
			
			//mount -o loop -t singlefilefs image ./mount/
			sprintf(mount_command,"mount -o loop -t singlefilefs %s ./", PATH_TO_IMAGE);
			ret_sys = system(mount_command);
			if (ret_sys > 0) printf("mount completed successfully.\n");
			else printf("error: %s\n", strerror(errno));
			
			printf("\n\nPress enter to exit: ");
			while (getchar() != '\n');
		
		} else if (op1 == 5) {
			
			sprintf(mount_command,"umount ./");
			ret_sys = system(mount_command);
			if (ret_sys > 0) printf("umount completed successfully.\n");
			else printf("error: %s\n", strerror(errno));
			
			printf("\n\nPress enter to exit: ");
			while (getchar() != '\n');
		}
		else if (op1 == 6) {
			return;
		}
		else {
			return;
		}
	}
}
