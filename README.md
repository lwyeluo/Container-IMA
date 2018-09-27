# Trusted-Container for kernel 3.13.0

## Quick Start

Currently, we only support the Ubuntu 14.04 64bit.

```
root@wluo:~# cat /etc/lsb-release 
DISTRIB_ID=Ubuntu
DISTRIB_RELEASE=14.04
DISTRIB_CODENAME=trusty
DISTRIB_DESCRIPTION="Ubuntu 14.04 LTS"
```

The docker version is:

```
root@wluo:~# docker version
Client:
 Version:      17.06.1-ce
 API version:  1.30
 Go version:   go1.8.3
 Git commit:   874a737
 Built:        Thu Aug 17 22:53:09 2017
 OS/Arch:      linux/amd64

Server:
 Version:      17.06.1-ce
 API version:  1.30 (minimum version 1.12)
 Go version:   go1.8.3
 Git commit:   874a737
 Built:        Thu Aug 17 22:51:03 2017
 OS/Arch:      linux/amd64
 Experimental: false
root@wluo:~# 
```

### compile and login our kernel (omit)

The way to enable IMA is to modify `/etc/default/grub`:

```
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash ima_tcb"
```

and run `update-grub2`.

### install docker (omit...)

### build tpm tools

- make sure you have a tpm chip, otherwise you should run a tpm-emulator[https://github.com/lwyeluo/tpm-emulator] just for test. 

```
apt-get install libtspi-dev trousers
cd tool/tpm-extend
# make
make install

cd ../show-pcr
# make
make install
```

### install our docker-run

```
# export your GOPATH, for example
root@TContainer:/usr/src/TContainer/runc# cat /etc/profile | grep GOPATH
export GOPATH=/go/

# our code for runc
mkdir -p /go/src/github.com/opencontainers
cd /go/src/github.com/opencontainers

cp -rf /usr/src/TContainer/runc .

# add dependencies
apt-get install libseccomp-dev 
apt-get install libapparmor-dev
apt-get install golang-go

# If the go version is too old, you can
#   # mkdir /usr/local/go1.6
#   #  wget https://storage.googleapis.com/golang/go1.6.linux-amd64.tar.gz
#   # tar -xzvf go1.6.linux-amd64.tar.gz -C /usr/local/go1.6
#   # vim /etc/profile
#   #   export GOROOT=/usr/local/go1.6/go
#   #   export GOBIN=$GOROOT/bin
#   #   export GOPATH=/go/
#   #   export PATH=$PATH:$GOBIN

# go get github.com/coreos/go-tspi

# make runc
cd runc/
make 
make install

cd /usr/bin
cp docker-runc docker-runc.bak

rm docker-runc
ln -s /usr/local/sbin/runc  docker-runc

# refer to our docker-runc
root@TContainer:/usr/bin# ls -l | grep docker-runc
lrwxrwxrwx 1 root root          20 9月   8 19:48 docker-runc -> /usr/local/sbin/runc
-rwxr-xr-x 1 root root     8727368 9月   8 19:47 docker-runc.bak
```

### disable docker daemon

```
# For ubuntu16.04
systemctl disable docker.service

root@TContainer:~# systemctl list-unit-files -t service | grep docker
docker.service                             disabled
root@TContainer:~#

# For ubuntu14.04
mv /etc/init/docker.conf ~
```

### reboot


### test

- show ima

```
root@wluo:/sys/kernel/security/ima# head -n 5 *
==> ascii_runtime_measurements <==
10 9dd918600e529a076cb976584675ca7a418ae5da ima-ng sha1:27d6a1e1108a90c19003d919f325c7bd0f4acb10 boot_aggregate
10 e43015028ff976aed9750d1d2a265fbf364ee771 ima-ng sha1:a5e65f1ca3a779cea954a015bde7ec5daa3f7612 4026531840:/init
10 ee766354b07ac3c08bedc1a3053b9b3413726247 ima-ng sha1:dc3e621c72cde19593c42a7703e143fd3dad5320 4026531840:/bin/sh
10 53f863d4c51370caf662e21324b9702366941821 ima-ng sha1:67c253d8ea7089253719cad7f952fb4c22240f27 4026531840:/lib64/ld-linux-x86-64.so.2
10 00e6e868fb546be32b9811d785624692765b394e ima-ng sha1:fac553d7706114a2ed0ef587aaf48f58e19f381a 4026531840:/etc/ld.so.cache

==> binary_runtime_measurements <==

��`R�l�vXFu�zA���ima-ng1sha1:'֡�������%ǽJ�boot_aggregate
*&_�6N�qima-ng3sha1:��_��yΩT����]�?v4026531840:/init
�vcT�z������;�4rbGima-ng5sha1:�>br�ᕓ�*w�C�=�S 4026531840:/bin/sh
S�c��p��b�$�p#f�!ima-ngIsha1:g�S��p�%7���R�L"$''4026531840:/lib64/ld-linux-x86-64.so.2

==> cpcr <==
head: error reading ‘cpcr’: Too many levels of symbolic links
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3168

==> violations <==
30
```

- show pcr(if you have a hardware TPM)

```
root@wluo:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 5e7c75ba0db3b3269a582dceddf32369ab62f782
PCR 01 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 02 630963df4644b5433d952ba30c37d979ad3248cd
PCR 03 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 04 6a6b07120fe2c46e2c03b98a1ccd5429f48d2a2c
PCR 05 c45cbb60ebb801fae50c09d279d2bac8e9e4cade
PCR 06 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 07 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 08 0000000000000000000000000000000000000000
PCR 09 0000000000000000000000000000000000000000
PCR 10 6cf94887788eb0192d447121d77ca46723f92188
PCR 11 0000000000000000000000000000000000000000
PCR 12 0000000000000000000000000000000000000000
PCR 13 0000000000000000000000000000000000000000
PCR 14 0000000000000000000000000000000000000000
PCR 15 0000000000000000000000000000000000000000
PCR 16 0000000000000000000000000000000000000000
PCR 17 ffffffffffffffffffffffffffffffffffffffff
PCR 18 ffffffffffffffffffffffffffffffffffffffff
PCR 19 ffffffffffffffffffffffffffffffffffffffff
PCR 20 ffffffffffffffffffffffffffffffffffffffff
PCR 21 ffffffffffffffffffffffffffffffffffffffff
PCR 22 ffffffffffffffffffffffffffffffffffffffff
PCR 23 0000000000000000000000000000000000000000
```

- run docker daemon

```
unshare -m dockerd
```

- all information of docker daemon has been recorded into file 4026532222, and cpcr and PCR12 has values

```
root@wluo:/sys/kernel/security/ima# head -n 5 *
==> 4026532222 <==
4026532222 13ba13f3ec111d86646664a6ca5d14c03e03f011 ima-ng sha1:38919a201521117fb8bf907ab5d41bb31eb29a39 1990->1980->1907->1447->1249->1071->1->0_4026532222:/usr/bin/unshare
4026532222 8cc30685b2905ad07d3e4a2f2dd8615f128e300c ima-ng sha1:a348d30d0774ed4d84945da0f10d6319da4cd9ac 4026532222:/usr/bin/dockerd
4026532222 d05b2bd62e71e3152b48c4ef6e596240a920c397 ima-ng sha1:80a5ea753fe06e9ecc8f5bdf857d4af9d8aef39b 4026532222:/usr/bin/docker-containerd
4026532222 5feebb2da6d179de8dfb40cbcbb89852d8d3026a ima-ng sha1:126ee56dd59433f8a488cf873d0fe6fea2c3f91a 4026532222:/var/lib/docker/tmp/docker-default618280113
4026532222 4de7379d947eb91bbddfbdecf346669643d40f85 ima-ng sha1:a00d3061edf0ff271e7e7395d2d9676736f95477 4026532222:/lib/modules/3.13.11-ckt39/kernel/ubuntu/aufs/aufs.ko

==> ascii_runtime_measurements <==
10 9dd918600e529a076cb976584675ca7a418ae5da ima-ng sha1:27d6a1e1108a90c19003d919f325c7bd0f4acb10 boot_aggregate
10 e43015028ff976aed9750d1d2a265fbf364ee771 ima-ng sha1:a5e65f1ca3a779cea954a015bde7ec5daa3f7612 4026531840:/init
10 ee766354b07ac3c08bedc1a3053b9b3413726247 ima-ng sha1:dc3e621c72cde19593c42a7703e143fd3dad5320 4026531840:/bin/sh
10 53f863d4c51370caf662e21324b9702366941821 ima-ng sha1:67c253d8ea7089253719cad7f952fb4c22240f27 4026531840:/lib64/ld-linux-x86-64.so.2
10 00e6e868fb546be32b9811d785624692765b394e ima-ng sha1:fac553d7706114a2ed0ef587aaf48f58e19f381a 4026531840:/etc/ld.so.cache

==> binary_runtime_measurements <==

��`R�l�vXFu�zA���ima-ng1sha1:'֡�������%ǽJ�boot_aggregate
*&_�6N�qima-ng3sha1:��_��yΩT����]�?v4026531840:/init
�vcT�z������;�4rbGima-ng5sha1:�>br�ᕓ�*w�C�=�S 4026531840:/bin/sh
S�c��p��b�$�p#f�!ima-ngIsha1:g�S��p�%7���R�L"$''4026531840:/lib64/ld-linux-x86-64.so.2

==> cpcr <==
history bb454f047b2196b15a15f002a02ca2b6abe9beb4
4026532222 e1dcb5b283a5105b85fa0ee13a17ab8ec0f21c6c d8e8ea4c33f4476778aef29f2b101916c54ed494
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3294

==> violations <==
31
root@wluo:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 5e7c75ba0db3b3269a582dceddf32369ab62f782
PCR 01 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 02 630963df4644b5433d952ba30c37d979ad3248cd
PCR 03 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 04 6a6b07120fe2c46e2c03b98a1ccd5429f48d2a2c
PCR 05 c45cbb60ebb801fae50c09d279d2bac8e9e4cade
PCR 06 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 07 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 08 0000000000000000000000000000000000000000
PCR 09 0000000000000000000000000000000000000000
PCR 10 0d22971e1acc35071b00dce6001ac3438bb3a624
PCR 11 0000000000000000000000000000000000000000
PCR 12 e1dff04e525ad42279a0fe978f3f33274916c2c2
PCR 13 0000000000000000000000000000000000000000
PCR 14 0000000000000000000000000000000000000000
PCR 15 0000000000000000000000000000000000000000
PCR 16 0000000000000000000000000000000000000000
PCR 17 ffffffffffffffffffffffffffffffffffffffff
PCR 18 ffffffffffffffffffffffffffffffffffffffff
PCR 19 ffffffffffffffffffffffffffffffffffffffff
PCR 20 ffffffffffffffffffffffffffffffffffffffff
PCR 21 ffffffffffffffffffffffffffffffffffffffff
PCR 22 ffffffffffffffffffffffffffffffffffffffff
PCR 23 0000000000000000000000000000000000000000
```

- run a container

```
root@wluo:~# docker run -ti ubuntu:16.04 /bin/bash
root@5efdec5d8777:/# 
```

- information of this container has been recorded into 4026532238, and cpcr and PCR11 has values

```
root@wluo:/sys/kernel/security/ima# head -n 5 *
==> 4026532222 <==
4026532222 13ba13f3ec111d86646664a6ca5d14c03e03f011 ima-ng sha1:38919a201521117fb8bf907ab5d41bb31eb29a39 1990->1980->1907->1447->1249->1071->1->0_4026532222:/usr/bin/unshare
4026532222 8cc30685b2905ad07d3e4a2f2dd8615f128e300c ima-ng sha1:a348d30d0774ed4d84945da0f10d6319da4cd9ac 4026532222:/usr/bin/dockerd
4026532222 d05b2bd62e71e3152b48c4ef6e596240a920c397 ima-ng sha1:80a5ea753fe06e9ecc8f5bdf857d4af9d8aef39b 4026532222:/usr/bin/docker-containerd
4026532222 5feebb2da6d179de8dfb40cbcbb89852d8d3026a ima-ng sha1:126ee56dd59433f8a488cf873d0fe6fea2c3f91a 4026532222:/var/lib/docker/tmp/docker-default618280113
4026532222 4de7379d947eb91bbddfbdecf346669643d40f85 ima-ng sha1:a00d3061edf0ff271e7e7395d2d9676736f95477 4026532222:/lib/modules/3.13.11-ckt39/kernel/ubuntu/aufs/aufs.ko

==> 4026532238 <==
4026532238 33b5330a1874d200ee7447bb7ff9c9ed0e4f7838 ima-ng sha1:d5442f82ce6ee98f17b4a21e49fdd326e4d1c6ae 2358->2354->2346->2001->1990->1980->1907->1447->1249->1071->1->0_4026532238:/usr/local/sbin/runc
4026532238 d1d56106cdb64703146dc665dc78ebbb7d1e4a53 ima-ng sha1:611a59c515074dbb376713fd19040c10a0c8e5e2 4026532238:/bin/bash
4026532238 4bf3d21c1e4cc3d675a591456bf626e23566ae07 ima-ng sha1:b43aec6bd95cab18f2bcedf1dfcf01462f7e088d 4026532238:/lib/x86_64-linux-gnu/ld-2.23.so
4026532238 e7a5aff51a1f924eb055549198f2bde4fe8cde0a ima-ng sha1:eaae87c3d507be4634db12ab562c9f1cb243e764 4026532238:/etc/ld.so.cache
4026532238 ade57014d08244a407c4d43cb4b642cbd81e816e ima-ng sha1:2bd9389f52439de7a42efcb8a3c3f8d4c881e8b2 4026532238:/lib/x86_64-linux-gnu/libtinfo.so.5.9

==> ascii_runtime_measurements <==
10 9dd918600e529a076cb976584675ca7a418ae5da ima-ng sha1:27d6a1e1108a90c19003d919f325c7bd0f4acb10 boot_aggregate
10 e43015028ff976aed9750d1d2a265fbf364ee771 ima-ng sha1:a5e65f1ca3a779cea954a015bde7ec5daa3f7612 4026531840:/init
10 ee766354b07ac3c08bedc1a3053b9b3413726247 ima-ng sha1:dc3e621c72cde19593c42a7703e143fd3dad5320 4026531840:/bin/sh
10 53f863d4c51370caf662e21324b9702366941821 ima-ng sha1:67c253d8ea7089253719cad7f952fb4c22240f27 4026531840:/lib64/ld-linux-x86-64.so.2
10 00e6e868fb546be32b9811d785624692765b394e ima-ng sha1:fac553d7706114a2ed0ef587aaf48f58e19f381a 4026531840:/etc/ld.so.cache

==> binary_runtime_measurements <==

��`R�l�vXFu�zA���ima-ng1sha1:'֡�������%ǽJ�boot_aggregate
*&_�6N�qima-ng3sha1:��_��yΩT����]�?v4026531840:/init
�vcT�z������;�4rbGima-ng5sha1:�>br�ᕓ�*w�C�=�S 4026531840:/bin/sh
S�c��p��b�$�p#f�!ima-ngIsha1:g�S��p�%7���R�L"$''4026531840:/lib64/ld-linux-x86-64.so.2

==> cpcr <==
history 8b87f480578e432f0a11f8783b59dbfa593fd8cb
4026532222 6878a26f8b3cf924244012afab916e2db6a2826f d8e8ea4c33f4476778aef29f2b101916c54ed494
4026532238 5c66faea22f4b35d244d666a4b9b62c53228d6bd 37a7f90fdcb6c610d4b19ac579688404b2ffb4ba
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3361

==> violations <==
32
root@wluo:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 5e7c75ba0db3b3269a582dceddf32369ab62f782
PCR 01 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 02 630963df4644b5433d952ba30c37d979ad3248cd
PCR 03 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 04 6a6b07120fe2c46e2c03b98a1ccd5429f48d2a2c
PCR 05 c45cbb60ebb801fae50c09d279d2bac8e9e4cade
PCR 06 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 07 3a3f780f11a4b49969fcaa80cd6e3957c33b2275
PCR 08 0000000000000000000000000000000000000000
PCR 09 0000000000000000000000000000000000000000
PCR 10 c29084057d361ceaae2082e9fe85897905eb4c9f
PCR 11 e0d817dc981893a8f9d7751354490a689743d6d3
PCR 12 7733993702ce721df0d27aad2fa8d5c1d013ebd4
PCR 13 0000000000000000000000000000000000000000
PCR 14 0000000000000000000000000000000000000000
PCR 15 0000000000000000000000000000000000000000
PCR 16 0000000000000000000000000000000000000000
PCR 17 ffffffffffffffffffffffffffffffffffffffff
PCR 18 ffffffffffffffffffffffffffffffffffffffff
PCR 19 ffffffffffffffffffffffffffffffffffffffff
PCR 20 ffffffffffffffffffffffffffffffffffffffff
PCR 21 ffffffffffffffffffffffffffffffffffffffff
PCR 22 ffffffffffffffffffffffffffffffffffffffff
PCR 23 0000000000000000000000000000000000000000
```

- the boot time of container is recorded in `/var/log/docker-boot.log`

```
root@wluo:~# cat /var/log/docker-boot.log 
"5efdec5d87775c649eb21c2efc4e8965255bccd293fd686bc0ea4dc4c992284e [4026532238] sha256:7aa3602ab41ea3384904197455e66f6435cb0261bd62a06db1d8e76cb8960c42 /var/lib/docker/containers/5efdec5d87775c649eb21c2efc4e8965255bccd293fd686bc0ea4dc4c992284e/config.v2.json" c952b062e8be3cbc407242cb2ebcb27c8111b489
```

- validate cpcr against PCR12

```
## ==> cpcr <==
## history 8b87f480578e432f0a11f8783b59dbfa593fd8cb
## 4026532222 6878a26f8b3cf924244012afab916e2db6a2826f d8e8ea4c33f4476778aef29f2b101916c54ed494
## 4026532238 5c66faea22f4b35d244d666a4b9b62c53228d6bd 37a7f90fdcb6c610d4b19ac579688404b2ffb4ba

##  PCR 12 7733993702ce721df0d27aad2fa8d5c1d013ebd4

root@wluo:/usr/src/TContainer/tool# python3 test_pcr.py 
history: 8b87f480578e432f0a11f8783b59dbfa593fd8cb
cpcr: value=6878a26f8b3cf924244012afab916e2db6a2826f secret=d8e8ea4c33f4476778aef29f2b101916c54ed494
cpcr: value=5c66faea22f4b35d244d666a4b9b62c53228d6bd secret=37a7f90fdcb6c610d4b19ac579688404b2ffb4ba
>>> check success
[s-pcr, pcr12] = 7733993702ce721df0d27aad2fa8d5c1d013ebd4 7733993702ce721df0d27aad2fa8d5c1d013ebd4
root@wluo:/usr/src/TContainer/tool# 
```

- validate boot time against PCR11

```
# entry in /var/log/docker-boot.log：
#   c952b062e8be3cbc407242cb2ebcb27c8111b489
# pcr11：
#   PCR 11 e0d817dc981893a8f9d7751354490a689743d6d3

root@wluo:/usr/src/TContainer/tool# python3
Python 3.4.3 (default, Oct 14 2015, 20:28:29) 
[GCC 4.8.4] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> import hashlib
>>> sha1 = hashlib.sha1()
>>> calc_pcr = bytes.fromhex("0"*40)
>>> sha1.update(calc_pcr)
>>> sha1.update(bytes.fromhex("c952b062e8be3cbc407242cb2ebcb27c8111b489"))
>>> print(sha1.hexdigest())
e0d817dc981893a8f9d7751354490a689743d6d3
>>> 
```
