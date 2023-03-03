#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "../user_hdr.h"


#define INVALIDATE 134 //this depends on what the kernel tells you when mounting the printk-example module
#define PUT 	   156
#define GET	   	   174

pthread_barrier_t barrier;

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
	print_geterror(ret, offset, destination);

	free(destination);
	printf("\n\nPress enter to exit: ");
	while (getchar() != '\n');
	return;
			
}



void put_data(void) {

	int ret;
	char input[DATA_SIZE];
	
	printf("[PUT DATA PARAMETERS]");
	printf("\n*Message (up to %ld bytes): ", DATA_SIZE);
	get_input(DATA_SIZE, input);

	ret = syscall(PUT, input, strlen(input));
	print_puterror(ret);
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
	print_invalidateerror(ret, offset);

	printf("\n\nPress enter to exit: ");
	while (getchar() != '\n');
	return;

			
}

void* multiple_put(void* arg){
	
	int i, ret;
	char str[NUM_DIGITS(NREQUESTS)];

	pthread_barrier_wait(&barrier);

	for(i=0; i<NREQUESTS; i++){
		if(i==NREQUESTS/2) sleep(12);
		sprintf(str, "%d", i);
		ret = syscall(PUT, str, strlen(str));
		print_puterror(ret);
	}
	pthread_exit(NULL);
}

void* multiple_get(void* arg){
	
	int i, ret;
	char destination[124];
	
	pthread_barrier_wait(&barrier);
	
	for(i=0; i<NREQUESTS; i++){
		if(i==NREQUESTS/2) sleep(12);
		ret = syscall(GET, 0, destination, 100);
		print_geterror(ret, 100, destination);
	}

	pthread_exit(NULL);
}

void* multiple_invalidate(void* arg){
	
	int i, ret;

	pthread_barrier_wait(&barrier);
	
	for(i=0; i<NREQUESTS; i++){
		ret = syscall(INVALIDATE, i%NBLOCKS);
		print_invalidateerror(ret, i%NBLOCKS);
	}

	pthread_exit(NULL);
}


int main(int argc, char** argv){

	char input[2];
	int op1,ret_sys;
	char mount_command[1024];
	pthread_t tid1, tid2, tid3;
	

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
			printf("outcome: %s\n", strerror(errno));
			
			printf("\n\nPress enter to exit: ");
			while (getchar() != '\n');
		
		} else if (op1 == 5) {
			
			sprintf(mount_command,"umount ./");
			ret_sys = system(mount_command);
			printf("outcome: %s\n", strerror(errno));
			
			printf("\n\nPress enter to exit: ");
			while (getchar() != '\n');

		} else if(op1 == 6){

			pthread_barrier_init(&barrier, NULL, 3);
			pthread_create(&tid1,NULL,multiple_put,NULL);
			pthread_create(&tid2,NULL,multiple_get,NULL);
			pthread_create(&tid3,NULL,multiple_invalidate,NULL);
			pthread_join(tid1, NULL);
			pthread_join(tid2, NULL);
			pthread_join(tid3, NULL);
			pthread_barrier_destroy(&barrier);
			
			printf("\n\nPress enter to exit: ");
			while (getchar() != '\n');
			
		} 
		
		else return 0;
	}
}
