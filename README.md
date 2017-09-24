# Trusted-Container for kernel 3.13.0

## Quick Start

### install docker (omit...)

### build tpm tools

- make sure you have a tpm chip, otherwise you should run a tpm-emulator[https://github.com/lwyeluo/tpm-emulator] for test. 

```
apt-get install libtspi-dev trousers
cd tool/tpm-extend
make
make install

cd ../show-pcr
make
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

### compile and login our kernel (omit)

### test

- show ima

```
root@TContainer:~# cd /sys/kernel/security/ima/
root@TContainer:/sys/kernel/security/ima# ls
ascii_runtime_measurements   cpcr    runtime_measurements_count
binary_runtime_measurements  policy  violations
root@TContainer:/sys/kernel/security/ima# head -n 5 *
==> ascii_runtime_measurements <==
10 e024c261cccd18a9b80a42780875dc86e912a92d ima-ng sha1:3d94c4fa9ec4969d54202cfc92bfac20beacb3d7 boot_aggregate
10 e43015028ff976aed9750d1d2a265fbf364ee771 ima-ng sha1:a5e65f1ca3a779cea954a015bde7ec5daa3f7612 4026531840:/init
10 ee766354b07ac3c08bedc1a3053b9b3413726247 ima-ng sha1:dc3e621c72cde19593c42a7703e143fd3dad5320 4026531840:/bin/sh
10 53f863d4c51370caf662e21324b9702366941821 ima-ng sha1:67c253d8ea7089253719cad7f952fb4c22240f27 4026531840:/lib64/ld-linux-x86-64.so.2
10 00e6e868fb546be32b9811d785624692765b394e ima-ng sha1:fac553d7706114a2ed0ef587aaf48f58e19f381a 4026531840:/etc/ld.so.cache

==> binary_runtime_measurements <==

�$�a����
Bu܆��-ima-ng1sha1:=����Ė�T ,���� ���boot_aggregate
*&_�6N�qima-ng3sha1:��_��yΩT����]�?v4026531840:/init
�vcT�z������;�4rbGima-ng5sha1:�>br�ᕓ�*w�C�=�S 4026531840:/bin/sh

==> cpcr <==
head: error reading ‘cpcr’: Too many levels of symbolic links
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3313

==> violations <==
4
root@TContainer:/sys/kernel/security/ima#
```

- show pcr(if you have a hardware TPM)

```
root@TContainer:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 029641cb9943b654fea149a37a69c6887ed44e0f
PCR 01 7d0af2df392eccac86f87a96cfebf6295cef753c
PCR 02 21bb7fb701c2aafea94826edc08a9da680a6356b
PCR 03 b2a83b0ebf2f8374299a5b2bdfc31ea955ad7236
PCR 04 5278bd4b22544edf470f074eacb3d3603019da22
PCR 05 45a323382bd933f08e7f0e256bc8249e4095b1ec
PCR 06 5647168983c894e7bbeff1aa3dfbde848f918f06
PCR 07 b2a83b0ebf2f8374299a5b2bdfc31ea955ad7236
PCR 08 0000000000000000000000000000000000000000
PCR 09 0000000000000000000000000000000000000000
PCR 10 451d4aabd463b935d4d313eba32a18ff72a0e6cd
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

- all information of docker daemon has been recorded into file 4026532286, and cpcr and PCR12 has values

```
root@TContainer:/sys/kernel/security/ima# ls
4026532286                   cpcr                        violations
ascii_runtime_measurements   policy
binary_runtime_measurements  runtime_measurements_count
root@TContainer:/sys/kernel/security/ima# head -n 5 *
==> 4026532286 <==
4026532286 d2b82b013e136d5f05aa03f92f519546ab206782 ima-ng sha1:38919a201521117fb8bf907ab5d41bb31eb29a39 2535->2525->2150->1646->1413->1224->1->0_4026532286:/usr/bin/unshare
4026532286 b9d85d3f55bcde0907488e074aab00d39d865154 ima-ng sha1:3b16665afb276378ad2368fab581106e563b74da 4026532286:/usr/bin/dockerd
4026532286 b23dacc13743c81a236716c19156e2ed59002bf8 ima-ng sha1:80a5ea753fe06e9ecc8f5bdf857d4af9d8aef39b 4026532286:/usr/bin/docker-containerd
4026532286 ffd2cdba8ea7d7eefe410998dba11e8cea9c4367 ima-ng sha1:126ee56dd59433f8a488cf873d0fe6fea2c3f91a 4026532286:/var/lib/docker/tmp/docker-default045283033
4026532286 eb59235b278225a90b1fe9a84a5bbcc1fee7bba0 ima-ng sha1:64acb36cdf684d68843913771b89004227c3b5f3 4026532286:/lib/modules/3.13.11-ckt39/kernel/ubuntu/aufs/aufs.ko

==> ascii_runtime_measurements <==
10 e024c261cccd18a9b80a42780875dc86e912a92d ima-ng sha1:3d94c4fa9ec4969d54202cfc92bfac20beacb3d7 boot_aggregate
10 e43015028ff976aed9750d1d2a265fbf364ee771 ima-ng sha1:a5e65f1ca3a779cea954a015bde7ec5daa3f7612 4026531840:/init
10 ee766354b07ac3c08bedc1a3053b9b3413726247 ima-ng sha1:dc3e621c72cde19593c42a7703e143fd3dad5320 4026531840:/bin/sh
10 53f863d4c51370caf662e21324b9702366941821 ima-ng sha1:67c253d8ea7089253719cad7f952fb4c22240f27 4026531840:/lib64/ld-linux-x86-64.so.2
10 00e6e868fb546be32b9811d785624692765b394e ima-ng sha1:fac553d7706114a2ed0ef587aaf48f58e19f381a 4026531840:/etc/ld.so.cache

==> binary_runtime_measurements <==

�$�a����
Bu܆��-ima-ng1sha1:=����Ė�T ,���� ���boot_aggregate
*&_�6N�qima-ng3sha1:��_��yΩT����]�?v4026531840:/init
�vcT�z������;�4rbGima-ng5sha1:�>br�ᕓ�*w�C�=�S 4026531840:/bin/sh

==> cpcr <==
history 02b64cf95371330e57664dde04990bd7d58ce8b9
4026532286 5473d418073ebecdf699223ff0f8191809a44d1c
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3600

==> violations <==
5
root@TContainer:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 029641cb9943b654fea149a37a69c6887ed44e0f
PCR 01 7d0af2df392eccac86f87a96cfebf6295cef753c
PCR 02 21bb7fb701c2aafea94826edc08a9da680a6356b
PCR 03 b2a83b0ebf2f8374299a5b2bdfc31ea955ad7236
PCR 04 5278bd4b22544edf470f074eacb3d3603019da22
PCR 05 45a323382bd933f08e7f0e256bc8249e4095b1ec
PCR 06 5647168983c894e7bbeff1aa3dfbde848f918f06
PCR 07 b2a83b0ebf2f8374299a5b2bdfc31ea955ad7236
PCR 08 0000000000000000000000000000000000000000
PCR 09 0000000000000000000000000000000000000000
PCR 10 c6f6b41d6b23658dbfe02c196a7fab7d712575f6
PCR 11 0000000000000000000000000000000000000000
PCR 12 191a06919ba93075d1c4cf176e0d3ec6f6302633
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
root@TContainer:~# docker run -ti ubuntu:16.04
root@b00b2dcd8b74:/#
```

- information of this container has been recorded into 4026532295, and cpcr and PCR11 has values

```
root@TContainer:/sys/kernel/security/ima# ls
4026532286  ascii_runtime_measurements   cpcr    runtime_measurements_count
4026532295  binary_runtime_measurements  policy  violations
root@TContainer:/sys/kernel/security/ima# head -n 5 *
==> 4026532286 <==
4026532286 d2b82b013e136d5f05aa03f92f519546ab206782 ima-ng sha1:38919a201521117fb8bf907ab5d41bb31eb29a39 2535->2525->2150->1646->1413->1224->1->0_4026532286:/usr/bin/unshare
4026532286 b9d85d3f55bcde0907488e074aab00d39d865154 ima-ng sha1:3b16665afb276378ad2368fab581106e563b74da 4026532286:/usr/bin/dockerd
4026532286 b23dacc13743c81a236716c19156e2ed59002bf8 ima-ng sha1:80a5ea753fe06e9ecc8f5bdf857d4af9d8aef39b 4026532286:/usr/bin/docker-containerd
4026532286 ffd2cdba8ea7d7eefe410998dba11e8cea9c4367 ima-ng sha1:126ee56dd59433f8a488cf873d0fe6fea2c3f91a 4026532286:/var/lib/docker/tmp/docker-default045283033
4026532286 eb59235b278225a90b1fe9a84a5bbcc1fee7bba0 ima-ng sha1:64acb36cdf684d68843913771b89004227c3b5f3 4026532286:/lib/modules/3.13.11-ckt39/kernel/ubuntu/aufs/aufs.ko

==> 4026532295 <==
4026532295 d474795df7bb0635c3f755479dff6223f860d304 ima-ng sha1:8cc5736a8f575fd926f17950bb16169fa14e2ed6 2776->2773->2765->2546->2535->2525->2150->1646->1413->1224->1->0_4026532295:/usr/local/sbin/runc
4026532295 0dcda56b3285d291e0a6f8e6ddfcf4450238b748 ima-ng sha1:611a59c515074dbb376713fd19040c10a0c8e5e2 4026532295:/bin/bash
4026532295 04190a7c917965a1aeb15bf2653a643e2a24cbf8 ima-ng sha1:bc659d9e2cb30539f49d1a008b685971b4f1e1a3 4026532295:/lib/x86_64-linux-gnu/ld-2.23.so
4026532295 59bd39953815ef17a7cd6522df16b90ecdb3f206 ima-ng sha1:eaae87c3d507be4634db12ab562c9f1cb243e764 4026532295:/etc/ld.so.cache
4026532295 4afdaf7ab7754f3f75457d75ed9814c43544a452 ima-ng sha1:2bd9389f52439de7a42efcb8a3c3f8d4c881e8b2 4026532295:/lib/x86_64-linux-gnu/libtinfo.so.5.9

==> ascii_runtime_measurements <==
10 e024c261cccd18a9b80a42780875dc86e912a92d ima-ng sha1:3d94c4fa9ec4969d54202cfc92bfac20beacb3d7 boot_aggregate
10 e43015028ff976aed9750d1d2a265fbf364ee771 ima-ng sha1:a5e65f1ca3a779cea954a015bde7ec5daa3f7612 4026531840:/init
10 ee766354b07ac3c08bedc1a3053b9b3413726247 ima-ng sha1:dc3e621c72cde19593c42a7703e143fd3dad5320 4026531840:/bin/sh
10 53f863d4c51370caf662e21324b9702366941821 ima-ng sha1:67c253d8ea7089253719cad7f952fb4c22240f27 4026531840:/lib64/ld-linux-x86-64.so.2
10 00e6e868fb546be32b9811d785624692765b394e ima-ng sha1:fac553d7706114a2ed0ef587aaf48f58e19f381a 4026531840:/etc/ld.so.cache

==> binary_runtime_measurements <==

�$�a����
Bu܆��-ima-ng1sha1:=����Ė�T ,���� ���boot_aggregate
*&_�6N�qima-ng3sha1:��_��yΩT����]�?v4026531840:/init
�vcT�z������;�4rbGima-ng5sha1:�>br�ᕓ�*w�C�=�S 4026531840:/bin/sh

==> cpcr <==
history b81600abe9acfc464589c8977e592851fb21a446
4026532286 104ebc209342d84eee358ae2f0b4a80ce43442ec
4026532295 dea5b1de72766d4c2011bbc40fb2abd8e1dfdbda
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3648

==> violations <==
5
root@TContainer:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 029641cb9943b654fea149a37a69c6887ed44e0f
PCR 01 7d0af2df392eccac86f87a96cfebf6295cef753c
PCR 02 21bb7fb701c2aafea94826edc08a9da680a6356b
PCR 03 b2a83b0ebf2f8374299a5b2bdfc31ea955ad7236
PCR 04 5278bd4b22544edf470f074eacb3d3603019da22
PCR 05 45a323382bd933f08e7f0e256bc8249e4095b1ec
PCR 06 5647168983c894e7bbeff1aa3dfbde848f918f06
PCR 07 b2a83b0ebf2f8374299a5b2bdfc31ea955ad7236
PCR 08 0000000000000000000000000000000000000000
PCR 09 0000000000000000000000000000000000000000
PCR 10 ebd0bdbe9905c3d2fcbb8da1e67669ceea68bc21
PCR 11 56dfc4669193e3952f695b65da48c0cd58776fbd
PCR 12 272298409eaa09929eecf53eaf7c90ea15747eb8
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
root@TContainer:/sys/kernel/security/ima# cat /var/log/docker-boot.log 

"dea39c591fb400419a63a7e9a45aad7b3f0d5791c7ad6a7ffce1f1302d245764 [4026532295] sha256:8b72bba4485f1004e8378bc6bc42775f8d4fb851c750c6c0329d3770b3a09086 /var/lib/docker/containers/dea39c591fb400419a63a7e9a45aad7b3f0d5791c7ad6a7ffce1f1302d245764/config.v2.json" 823ced7f1ed5f7a17fb333bb47a4b4534abee954
```

- validate cpcr against PCR12

```
## ==> cpcr <==
##  history b81600abe9acfc464589c8977e592851fb21a446
##  4026532286 104ebc209342d84eee358ae2f0b4a80ce43442ec
##  4026532295 dea5b1de72766d4c2011bbc40fb2abd8e1dfdbda

##  PCR 12 272298409eaa09929eecf53eaf7c90ea15747eb8

>>> import hashlib
>>> sha1 = hashlib.sha1()
>>> sha1.update(bytes.fromhex("104ebc209342d84eee358ae2f0b4a80ce43442ec"))
>>> sha1.update(bytes.fromhex("dea5b1de72766d4c2011bbc40fb2abd8e1dfdbda"))
>>> tempDigest = sha1.hexdigest()
>>> sha1 = hashlib.sha1()
>>> sha1.update(bytes.fromhex("b81600abe9acfc464589c8977e592851fb21a446"))
>>> sha1.update(bytes.fromhex(tempDigest))
>>> print(sha1.hexdigest())
272298409eaa09929eecf53eaf7c90ea15747eb8
```

- validate boot time against PCR11

```
# entry in /var/log/docker-boot.log：
#   823ced7f1ed5f7a17fb333bb47a4b4534abee954
# pcr11：
#   PCR 11 56dfc4669193e3952f695b65da48c0cd58776fbd

>>> import hashlib
>>> sha1 = hashlib.sha1()
>>> calc_pcr = bytes.fromhex("0"*40)
>>> sha1.update(calc_pcr)
>>> sha1.update(bytes.fromhex("823ced7f1ed5f7a17fb333bb47a4b4534abee954"))
>>> print(sha1.hexdigest())
56dfc4669193e3952f695b65da48c0cd58776fbd
>>>
```