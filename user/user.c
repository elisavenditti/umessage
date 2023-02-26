#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "../user_hdr.h"
#define fflush(stdin) while ((getchar()) != '\n')


#define INVALIDATE 134 //this depends on what the kernel tells you when mounting the printk-example module
#define PUT 	   156
#define GET	   	   174


void get_data(void) {

	int offset, ret;
	size_t size;
	char destination[] = "";
	char* input;

	printf("******************* GET DATA PARAMETERS *******************");
	printf("\n\n\n*Block number: ");
	fgets(input, NUM_DIGITS(NBLOCKS), stdin);
	fflush(stdin);
	offset = strtol(input, NULL, 10);

	// with the format [^\n] get input until \n
	printf("\n\n\n*Size to read: ");
	fgets(input,NUM_DIGITS(DEFAULT_BLOCK_SIZE), stdin);
	fflush(stdin);
	size = strtol(input, NULL, 10);

	
	printf("voglio mettere in destination (%p) i dati letti\n", destination);
	ret = syscall(GET, offset, destination, size);
	printf("ho letto %d byte dal blocco %ld: '%s'\n", ret, offset, destination);

	printf("\n\n\nPremi invio per uscire da questa schermata: ");
	while (getchar() != '\n');
	return;
			
}

void put_data(void) {

	int ret;
	char* input;
	char buff[DEFAULT_BLOCK_SIZE];
	
	printf("******************* PUT DATA PARAMETERS *******************");
	printf("\n\n\n*Message (up to %d bytes): ", DATA_SIZE);
	fgets(input, NUM_DIGITS(DATA_SIZE), stdin);
	fflush(stdin);

	printf("voglio inserire %d bytes\n", strlen(buff));
	ret = syscall(PUT, buff, strlen(buff));
	printf("il blocco è stato inserito nel blocco %d\n", ret);

	printf("\n\n\nPremi invio per uscire da questa schermata: ");
	while (getchar() != '\n');
	return;

			
}


void invalidate_data(void) {
	
	int ret, offset;
	char* input;

	printf("******************* INV DATA PARAMETERS *******************");
	printf("\n\n\n*Block number: ");
	fgets(input, NUM_DIGITS(NBLOCKS), stdin);
	fflush(stdin);
	offset = strtol(input, NULL, 10);

	ret = syscall(INVALIDATE, offset);
	printf("INVALIDATE HA RITORNATO %d\n", ret);
	

	printf("\n\n\nPremi invio per uscire da questa schermata: ");
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
	
	// if(argc < 3){
	// 	goto error_params;
	// }

	//mount -o loop -t singlefilefs image ./mount/
	// sprintf(mount_command,"mount -o loop -t singlefilefs %s ./", PATH_TO_IMAGE);
	// ret_sys = system(mount_command);
	// printf("ho chiamato la mount e mi ha ritornato %d (%s)\n", ret_sys, strerror(errno));


	///////////////////////////////////////////////
	char* input;
	while (1) {
		
		printf("%s\nSelect an option:", homepage);
		fgets(input, 2, stdin);
		// printf("ciao");
		while ((getchar()) != '\n');
		op1 = strtol(input, NULL, 10);

		if (op1 > 6){
			printf("\nerror - incorrect input\n");
			return -1;
		}
		
		if (op1 == 1) get_data();
		else if (op1 == 2) put_data();
		else if (op1 == 3) invalidate_data();
		else if (op1 == 4) {
			//mount -o loop -t singlefilefs image ./mount/
			sprintf(mount_command,"mount -o loop -t singlefilefs %s ./", PATH_TO_IMAGE);
			ret_sys = system(mount_command);
			printf("ho chiamato la mount e mi ha ritornato %d (%s)\n", ret_sys, strerror(errno));
			printf("\n\n\nPremi invio per uscire da questa schermata: ");
			while (getchar() != '\n');
		}
		else if (op1 == 5) {
			sprintf(mount_command,"umount ./");
			ret_sys = system(mount_command);
			printf("ho chiamato la umount e mi ha ritornato %d (%s)\n", ret_sys, strerror(errno));
			printf("\n\n\nPremi invio per uscire da questa schermata: ");
			while (getchar() != '\n');
		}
		else if (op1 == 6) {
			return;
		}
		else {
			return;
		}
	}


	///////////////////////////////////////////////



	// operation = argv[1][0];

	// char *path0 = "/dev/umessage";
	// int major0 = strtol(argv[2],NULL,10);
	// char buff0[4096];
			
	// switch(operation){

	// 	case 'I':
	// 		// invalidate data - input: I #blocco
			
	// 		arg = strtol(argv[2],NULL,10);
	// 		ret = syscall(INVALIDATE, arg);
	// 		printf("INVALIDATE HA RITORNATO %d\n", ret);
	// 		break;
		
		
	// 	case 'P':
	// 		// put data	- input P nuova_stinga
			
	// 		ret = syscall(PUT, argv[2], strlen(argv[2]));
	// 		printf("il blocco è stato inserito nel blocco %d\n", ret);
	// 		break;
		
		
	// 	case 'G':
	// 		// get data - input: G #blocco  #byte

	// 		if(argc<4) goto error_params;
	// 		arg = strtol(argv[2],NULL,10);
	// 		size_t size = strtol(argv[3],NULL,10);
	// 		char destination[] = "";
	// 		printf("voglio mettere in destination (%p) i dati letti\n", destination);
	// 		ret = syscall(GET, arg, destination, size);
	// 		printf("ho letto %d byte dal blocco %ld: '%s'\n", ret, arg, destination);
	// 		break;
		
	// 	case 'O':
			
	// 		// read - input: O 235 R(W)
	// 		if(argc<4) goto error_params;
	// 		char *path = "/dev/umessage";
	// 		int major = strtol(argv[2],NULL,10);
	// 		char rw = argv[3][0];
	// 		char buff[4096];

	// 		printf("creating device %s with major %d\n", path, major);

	// 		sprintf(buff,"mknod %s c %d 0\n",path,major);
	// 		system(buff);
	// 		sprintf(buff,"%s",path);	
			
	// 		printf("opening device %s\n",buff);
	// 		int fd = open(buff,O_RDONLY);
	// 		if(fd == -1) {
	// 			printf("open error on device %s\n",buff);
	// 			printf("errno=%s\n", strerror(errno));
	// 			return -1;
	// 		}
	// 		printf("device %s successfully opened\n",buff);
	// 		if(rw == 'R'){
	// 			ssize_t read_ret = read(fd, lettura, size);
	// 			printf("La lettura mi ha restituito %ld byte: %s\n\n", read_ret, lettura);
	// 		} else {
	// 			ssize_t write_ret = write(fd,"DATA",4);
	// 			printf("La scrittura ha cambiato %ld byte\n\n", write_ret);
	// 		}
			
	// 		break;




	// 	default:
	// 		printf("error in operation code, please insert one of the following:\n [I] for invalidate_data\n [P] for put_data\n [G] for get_data\n\n");
	// 		return -1;
	
	// }
	
	
	// printf("l'ho chiamata e mi ha ritornato %d\n", ret);
	// printf("errno=%s\n", strerror(errno));
	// return 0;



    



	
error_params:
	printf("usage: prog operation(I, P, G) args ....\n");
	return EXIT_FAILURE;
}
