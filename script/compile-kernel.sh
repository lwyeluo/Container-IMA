#!/bin/sh

code_dir="/usr/src/linux-4.4.3"
obj_dir="/usr/src/linux-4.4.3-obj"

cd $code_dir
#make menuconfig O=$obj_dir

#if [ $? -ne 0 ]; then
#	echo "[INOF] make menuconfig failed"
#	exit 1
#fi

make -j4  modules O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INOF] make menuconfig failed"
	exit 1
fi

make modules_install O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INOF] make menuconfig failed"
	exit 1
fi

make install O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INOF] make menuconfig failed"
	exit 1
fi


