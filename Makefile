obj-m += the_my_syscall.o
the_my_syscall-objs += my_syscall.o lib/scth.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
	gcc user/user.c -o user/user.o

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm user/*.o

mount:
	insmod the_my_syscall.ko the_syscall_table=$(A)