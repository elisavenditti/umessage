obj-m += umessage_module.o
umessage_module-objs += umessage.o lib/scth.o umessage_filesystem/file.o umessage_filesystem/dir.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)
NBLOCKS := 2
all:
	gcc umessage_filesystem/makefs_umessagefs.c -o umessage_filesystem/makefs_umessagefs
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
	gcc user/user.c -o user/user.o

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm ./image
	rm ./umessage_filesystem/makefs_umessagefs
	rmdir ./mount

mount-mod:
	insmod umessage_module.ko the_syscall_table=$(A)

umount-mod:
	rmmod umessage_module
	
create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=image
	./umessage_filesystem/makefs_umessagefs image $(NBLOCKS)
	mkdir mount
	
mount-fs:
	mount -o loop -t singlefilefs image ./mount/

umount-fs:
	umount ./mount/	

