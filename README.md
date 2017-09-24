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
3309

==> violations <==
5
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
PCR 00 029641cb9943b654fea149a37a69c6887ed44e
PCR 01 7d0af2df392eccac86f87a96cfebf6295cef75
PCR 02 21bb7fb701c2aafea94826edc08a9da680a635
PCR 03 b2a83b0ebf2f8374299a5b2bdfc31ea955ad72
PCR 04 5278bd4b22544edf470f074eacb3d3603019da
PCR 05 45a323382bd933f08e7f0e256bc8249e4095b1
PCR 06 5647168983c894e7bbeff1aa3dfbde848f918f
PCR 07 b2a83b0ebf2f8374299a5b2bdfc31ea955ad72
PCR 08 00000000000000000000000000000000000000
PCR 09 00000000000000000000000000000000000000
PCR 10 e03c839e0d192a170dea16b3acb71d9dc5e598
PCR 11 00000000000000000000000000000000000000
PCR 12 00000000000000000000000000000000000000
PCR 13 00000000000000000000000000000000000000
PCR 14 00000000000000000000000000000000000000
PCR 15 00000000000000000000000000000000000000
PCR 16 00000000000000000000000000000000000000
PCR 17 ffffffffffffffffffffffffffffffffffffff
PCR 18 ffffffffffffffffffffffffffffffffffffff
PCR 19 ffffffffffffffffffffffffffffffffffffff
PCR 20 ffffffffffffffffffffffffffffffffffffff
PCR 21 ffffffffffffffffffffffffffffffffffffff
PCR 22 ffffffffffffffffffffffffffffffffffffff
PCR 23 00000000000000000000000000000000000000
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
4026532286 1f03d59cbee6529aed5c563fc1a14b0050b37554 ima-ng sha1:38919a201521117fb8bf907ab5d41bb31eb29a39 2529->2518->2115->1639->1422->1177->1->0_4026532286:/usr/bin/unshare
4026532286 b9d85d3f55bcde0907488e074aab00d39d865154 ima-ng sha1:3b16665afb276378ad2368fab581106e563b74da 4026532286:/usr/bin/dockerd
4026532286 b23dacc13743c81a236716c19156e2ed59002bf8 ima-ng sha1:80a5ea753fe06e9ecc8f5bdf857d4af9d8aef39b 4026532286:/usr/bin/docker-containerd
4026532286 100d8fba5b238c01f60db3ff078880e2e16a7dba ima-ng sha1:126ee56dd59433f8a488cf873d0fe6fea2c3f91a 4026532286:/var/lib/docker/tmp/docker-default111928052
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
history 3748c62f0ce61151ce10a5926142c6e6e8d54938
4026532286 700409a7af8b3093d6c3cbcd01d171774d32896b
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3522

==> violations <==
6
root@TContainer:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 029641cb9943b654fea149a37a69c6887ed44e
PCR 01 7d0af2df392eccac86f87a96cfebf6295cef75
PCR 02 21bb7fb701c2aafea94826edc08a9da680a635
PCR 03 b2a83b0ebf2f8374299a5b2bdfc31ea955ad72
PCR 04 5278bd4b22544edf470f074eacb3d3603019da
PCR 05 45a323382bd933f08e7f0e256bc8249e4095b1
PCR 06 5647168983c894e7bbeff1aa3dfbde848f918f
PCR 07 b2a83b0ebf2f8374299a5b2bdfc31ea955ad72
PCR 08 00000000000000000000000000000000000000
PCR 09 00000000000000000000000000000000000000
PCR 10 8157d83dc87c094e610e121507cc2e5a90688a
PCR 11 00000000000000000000000000000000000000
PCR 12 dd811745250acef5e38667743367897c8f5ce7
PCR 13 00000000000000000000000000000000000000
PCR 14 00000000000000000000000000000000000000
PCR 15 00000000000000000000000000000000000000
PCR 16 00000000000000000000000000000000000000
PCR 17 ffffffffffffffffffffffffffffffffffffff
PCR 18 ffffffffffffffffffffffffffffffffffffff
PCR 19 ffffffffffffffffffffffffffffffffffffff
PCR 20 ffffffffffffffffffffffffffffffffffffff
PCR 21 ffffffffffffffffffffffffffffffffffffff
PCR 22 ffffffffffffffffffffffffffffffffffffff
PCR 23 00000000000000000000000000000000000000
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
4026532286 1f03d59cbee6529aed5c563fc1a14b0050b37554 ima-ng sha1:38919a201521117fb8bf907ab5d41bb31eb29a39 2529->2518->2115->1639->1422->1177->1->0_4026532286:/usr/bin/unshare
4026532286 b9d85d3f55bcde0907488e074aab00d39d865154 ima-ng sha1:3b16665afb276378ad2368fab581106e563b74da 4026532286:/usr/bin/dockerd
4026532286 b23dacc13743c81a236716c19156e2ed59002bf8 ima-ng sha1:80a5ea753fe06e9ecc8f5bdf857d4af9d8aef39b 4026532286:/usr/bin/docker-containerd
4026532286 100d8fba5b238c01f60db3ff078880e2e16a7dba ima-ng sha1:126ee56dd59433f8a488cf873d0fe6fea2c3f91a 4026532286:/var/lib/docker/tmp/docker-default111928052
4026532286 eb59235b278225a90b1fe9a84a5bbcc1fee7bba0 ima-ng sha1:64acb36cdf684d68843913771b89004227c3b5f3 4026532286:/lib/modules/3.13.11-ckt39/kernel/ubuntu/aufs/aufs.ko

==> 4026532295 <==
4026532295 b071932c6febc9250a39702b037b373e5ea3b391 ima-ng sha1:4f77d7f50160b4153ec183bd1fe8d9a1736386eb 2761->2756->2750->2538->2529->2518->2115->1639->1422->1177->1->0_4026532295:/usr/local/sbin/runc
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
history 14e02a2c3157b082a8316719ac904413735fd22c
4026532286 c40325915ee0230d1e35da8b047d516b14b5136e
4026532295 c0db868b8e6c66ed1c4966149cff4a1813c59183
head: cannot open ‘policy’ for reading: Permission denied

==> runtime_measurements_count <==
3570

==> violations <==
6
root@TContainer:/sys/kernel/security/ima# show-pcr
Create Context : Success
Context Connect : Success
Get TPM Handle : Success
Get the SRK handle : Key not found in persistent storage
Get the SRK policy : Invalid handle
PCR 00 029641cb9943b654fea149a37a69c6887ed44e
PCR 01 7d0af2df392eccac86f87a96cfebf6295cef75
PCR 02 21bb7fb701c2aafea94826edc08a9da680a635
PCR 03 b2a83b0ebf2f8374299a5b2bdfc31ea955ad72
PCR 04 5278bd4b22544edf470f074eacb3d3603019da
PCR 05 45a323382bd933f08e7f0e256bc8249e4095b1
PCR 06 5647168983c894e7bbeff1aa3dfbde848f918f
PCR 07 b2a83b0ebf2f8374299a5b2bdfc31ea955ad72
PCR 08 00000000000000000000000000000000000000
PCR 09 00000000000000000000000000000000000000
PCR 10 17d92b15575afb73c7d6f35bdc6bc09dcf57de
PCR 11 69af9eff5afdf33db002457ffdba8fe23b58d4
PCR 12 8cb42e6861c4d180a326ab5862ac2bf2b08b62
PCR 13 00000000000000000000000000000000000000
PCR 14 00000000000000000000000000000000000000
PCR 15 00000000000000000000000000000000000000
PCR 16 00000000000000000000000000000000000000
PCR 17 ffffffffffffffffffffffffffffffffffffff
PCR 18 ffffffffffffffffffffffffffffffffffffff
PCR 19 ffffffffffffffffffffffffffffffffffffff
PCR 20 ffffffffffffffffffffffffffffffffffffff
PCR 21 ffffffffffffffffffffffffffffffffffffff
PCR 22 ffffffffffffffffffffffffffffffffffffff
PCR 23 00000000000000000000000000000000000000
```

- the boot time of container is recorded in `/var/log/docker-boot.log`

```
root@TContainer:/sys/kernel/security/ima# cat /var/log/docker-boot.log 

"867fa470d6594c3295f2ea5d24744c6dcb5d37b50dfde9034f19ecd63c3c8b2e [4026532295] sha256:8b72bba4485f1004e8378bc6bc42775f8d4fb851c750c6c0329d3770b3a09086 /var/lib/docker/containers/867fa470d6594c3295f2ea5d24744c6dcb5d37b50dfde9034f19ecd63c3c8b2e/config.v2.json" 2cbdc1963fff33bbb74f554681d563fa7063468e
```

- validate cpcr against PCR12

```
## ==> cpcr <==
##  history 14e02a2c3157b082a8316719ac904413735fd22c
##  4026532286 c40325915ee0230d1e35da8b047d516b14b5136e
##  4026532295 c0db868b8e6c66ed1c4966149cff4a1813c59183

##  PCR 12 8cb42e6861c4d180a326ab5862ac2bf2b08b62

>>> import hashlib
>>> sha1 = hashlib.sha1()
>>> sha1.update(bytes.fromhex("c40325915ee0230d1e35da8b047d516b14b5136e"))
>>> sha1.update(bytes.fromhex("c0db868b8e6c66ed1c4966149cff4a1813c59183"))
>>> tempDigest = sha1.hexdigest()
>>> sha1 = hashlib.sha1()
>>> sha1.update(bytes.fromhex("14e02a2c3157b082a8316719ac904413735fd22c"))
>>> sha1.update(bytes.fromhex(tempDigest))
>>> print(sha1.hexdigest())
8cb42e6861c4d180a326ab5862ac2bf2b08b623a
```

- validate boot time against PCR11

```
# entry in /var/log/docker-boot.log：
#   2cbdc1963fff33bbb74f554681d563fa7063468e
# pcr11：
#   PCR 11 69af9eff5afdf33db002457ffdba8fe23b58d4

>>> import hashlib
>>> sha1 = hashlib.sha1()
>>> calc_pcr = bytes.fromhex("0"*40)
>>> sha1.update(calc_pcr)
>>> sha1.update(bytes.fromhex("2cbdc1963fff33bbb74f554681d563fa7063468e"))
>>> print(sha1.hexdigest())
69af9eff5afdf33db002457ffdba8fe23b58d4b5
>>>
```