#!/bin/sh

code_dir="/usr/src/Trusted-Container/linux-source-3.13.0"
obj_dir="/usr/src/linux-3.13-obj"

mkdir $code_dir || true

cd $code_dir
make menuconfig O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INOF] make menuconfig failed"
	exit 1
fi

make -j4 O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INFO] make menuconfig failed"
	exit 1
fi

cd $obj_dir
cp arch/x86/boot/bzImage /opt/kernel-debug

#make modules_install O=$obj_dir
#
#if [ $? -ne 0 ]; then
#	echo "[INFO] make menuconfig failed"
#	exit 1
#fi
#
#make install O=$obj_dir
#
#if [ $? -ne 0 ]; then
#	echo "[INFO] make menuconfig failed"
#	exit 1
#fi

#sed -i "/linux\t/s/$/& ima_tcb ima_template=\"ima\" ima_hash=\"sha1\"/g" /boot/grub/grub.cfg

