#!/bin/sh
base_dir=$(cd $(dirname .) && pwd)
code_dir=${base_dir}/linux-3.13.11
obj_dir="/usr/src/linux-3.13-TContainer-obj"

mkdir $obj_dir || true

apt-get install m4 libncurses-dev -y

cd $code_dir
make menuconfig O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INOF] make menuconfig failed"
	exit 1
fi

make -j8 O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INFO] make menuconfig failed"
	exit 1
fi

make -j8 modules_install O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INFO] make menuconfig failed"
	exit 1
fi

make install O=$obj_dir

if [ $? -ne 0 ]; then
	echo "[INFO] make menuconfig failed"
	exit 1
fi

#sed -i "/linux\t/s/$/& ima_tcb ima_template=\"ima\" ima_hash=\"sha1\"/g" /boot/grub/grub.cfg

