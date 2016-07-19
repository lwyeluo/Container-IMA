#!/bin/sh

code_dir="/usr/src/linux-source-3.13.0/linux-source-3.13.0"
obj_dir="/usr/src/linux-3.13-obj"

cd $code_dir
make menuconfig O=$obj_dir

#if [ $? -ne 0 ]; then
#	echo "[INOF] make menuconfig failed"
#	exit 1
#fi

make -j4 O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INFO] make menuconfig failed"
	exit 1
fi

make modules_install O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INFO] make menuconfig failed"
	exit 1
fi

make install O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INFO] make menuconfig failed"
	exit 1
fi

sed -i "/linux\t/s/$/& ima_tcb ima_template=\"ima\" ima_hash=\"sha1\"/g" /boot/grub/grub.cfg
