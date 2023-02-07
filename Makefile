obj-m += umessage_module.o
umessage_module-objs += umessage.o lib/scth.o umessage_filesystem/file.o umessage_filesystem/dir.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	gcc umessage_filesystem/makefs_umessagefs.c -o umessage_filesystem/makefs_umessagefs
	gcc user/user.c -o user/user.o
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rmdir mount

create-fs:
	dd bs=4096 count=100 if=/dev/zero of=image
	./umessage_filesystem/makefs_umessagefs image
	mkdir mount
	
mount-fs:
	mount -o loop -t singlefilefs image ./mount/
	
mount:
	insmod umessage_module.ko the_syscall_table=$(A)

umount:
	rmmod umessage_module