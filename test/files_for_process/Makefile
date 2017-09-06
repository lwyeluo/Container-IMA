name = hello
modules_cur := $(name).ko
obj-m := $(name).o
KERNEL := /lib/modules/$(shell uname -r)/build/
all:
	make -C $(KERNEL) M=`pwd` $(modules_cur)
install:
	#make -C $(KERNEL) M=`pwd` modules_install
	sudo insmod $(modules_cur)
clean:
	make -C $(KERNEL) M=`pwd` clean
	sudo rmmod $(modules_cur)
