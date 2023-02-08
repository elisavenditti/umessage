#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>	// per O_RDWR
#include <errno.h>

#define INVALIDATE 134 //this depends on what the kernel tells you when mounting the printk-example module
#define PUT 	   156
#define GET	   	   174

int main(int argc, char** argv){

 	
    long int arg;	
	char operation;
	int ret;
	
	if(argc < 3){
		goto error_params;
	}
	
	
	operation = argv[1][0];
	
	
	printf("sto per chiamare questa intelligentissima syscall\n");
	
	switch(operation){
		case 'I':
			arg = strtol(argv[2],NULL,10);
			ret = syscall(INVALIDATE, arg);
			break;
		case 'P':
			//strlen non conta il terminatore di stringa
			ret = syscall(PUT, argv[2], strlen(argv[2]));
			break;
		case 'G':
		// 	int sys_get_data(int offset, char* destination, size_t size){
			if(argc<4) goto error_params;
			arg = strtol(argv[2],NULL,10);
			size_t size = strtol(argv[3],NULL,10);
			char* destination = "";
			printf("voglio mettere in destination (%p) i dati letti\n", destination);
			ret = syscall(GET, arg, destination, size);
			break;
		
		case 'O':
			char *path = "/dev/umessage";
			int major = strtol(argv[2],NULL,10);
			int minors = strtol(argv[3],NULL,10);
			char buff[4096];
			printf("creating %d minors for device %s with major %d\n",minors,path,major);

			for(int i=0;i<minors;i++){
				sprintf(buff,"mknod %s%d c %d %i\n",path,i,major,i);
				system(buff);
				sprintf(buff,"%s%d",path,i);	
				
				printf("opening device %s\n",buff);
				int fd = open(buff,O_RDWR);
				if(fd == -1) {
					printf("open error on device %s\n",buff);
					return NULL;
				}
				printf("device %s successfully opened\n",buff);
				if(1){
					char* lettura = "";
					size_t size = 10;
					ssize_t read_ret = read(fd, (void *)lettura, size);
					printf("La lettura mi ha restituito: %s\n", lettura, size);
				} else
					write(fd,"DATA",4);
				return;
			}
			break;
		default:
			printf("error in operation code, please insert one of the following:\n [I] for invalidate_data\n [P] for put_data\n [G] for get_data\n\n");
			return -1;
	
	}
	
	
	printf("l'ho chiamata e mi ha ritornato %d\n", ret);
	printf("errno=%s\n", strerror(errno));
	return 0;



    


















	
error_params:
	printf("usage: prog operation(I, P, G) args ....\n");
	return EXIT_FAILURE;
}
