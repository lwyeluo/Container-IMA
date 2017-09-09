# Trusted-Container for kernel 3.13.0

## Quick Start

### install docker (omit...)

### install our docker-run

```
# export your GOPATH, for example
root@moby:/usr/src/Trusted-Container/runc# cat /etc/profile | grep GOPATH
export GOPATH=/go/

# our code for runc
mkdir -p /go/src/github.com/opencontainers
cd /go/src/github.com/opencontainers

cp -rf /usr/src/Trusted-Container/runc .

# add dependencies
apt-get install libseccomp-dev 
apt-get install libapparmor-dev

# make runc
cd runc/
make 
make install

cd /usr/bin
cp docker-runc docker-runc.bak

rm docker-runc
ln -s /usr/local/sbin/runc  docker-runc

# refer to our docker-runc
root@moby:/usr/bin# ls -l | grep docker-runc
lrwxrwxrwx 1 root root          20 9月   8 19:48 docker-runc -> /usr/local/sbin/runc
-rwxr-xr-x 1 root root     8727368 9月   8 19:47 docker-runc.bak
```

### disable docker daemon

```
systemctl disable docker.service

root@moby:~# systemctl list-unit-files -t service | grep docker
docker.service                             disabled
root@moby:~#
```

### compile and login our kernel (omit)

### test

- show ima

```
root@moby:/sys/kernel/security/ima# ls
ascii_runtime_measurements  binary_runtime_measurements  cpcr  kernel_module_measurements  runtime_measurements_count  violations
root@moby:/sys/kernel/security/ima# head -n 5 *
==> ascii_runtime_measurements <==
10 1d8d532d463c9f8c205d0df7787669a85f93e260 ima-ng sha1:0000000000000000000000000000000000000000 boot_aggregate
10 708c904047d025dfad5cf994a79972b6b8a9933b ima-ng sha1:ea635ede655b8364e93478ed2c998584b4e84ce7 4026531840:/sbin/init
10 ee82a099d86d5f6a06867c4685c200a37be89828 ima-ng sha1:bc659d9e2cb30539f49d1a008b685971b4f1e1a3 4026531840:/lib/x86_64-linux-gnu/ld-2.23.so
10 ebc240f17b95dbd69ec6d4b19601227a0b66a784 ima-ng sha1:2204529e85806a38b9b0077fdf18a002b5ad2069 4026531840:/lib/x86_64-linux-gnu/libselinux.so.1
10 673230165c43b5dc1ab9f62f752b00217adde06b ima-ng sha1:e8e0d27fcea338497a86023d4a2d0905ffc2b96c 4026531840:/lib/x86_64-linux-gnu/libcap.so.2.24

==> binary_runtime_measurements <==

??¡§_?ima-ng1sha1:boot_aggregate
p@G¦´?\?????;ima-ng8sha1:ò¨?[dôé???4026531840:/sbin/init
??_j|F?{Þ°ima-ngNsha1:?e,?9??hYq???4026531840:/lib/x86_64-linux-gnu/ld-2.23.so
????¨³¡À"z
        f¡ìima-ngSsha1:"Rj8????? i14026531840:/lib/x86_64-linux-gnu/libselinux.so.1

==> cpcr <==
head: error reading 'cpcr': Too many levels of symbolic links

==> kernel_module_measurements <==
11 bbee1ac32ee8fae3b717c058f55f1845ba6b99db ima-ng sha1:deda712cad827ee929d307d907a40253f012230e 4026531840:/lib/modules/3.13.11-ckt39/kernel/fs/autofs4/autofs4.ko
11 fd3048e35014b00de71bb79b6179f93147e668e1 ima-ng sha1:b647b3e6f64c294673d21d2131bbbafec25aa117 4026531840:/lib/modules/3.13.11-ckt39/kernel/drivers/parport/parport.ko
11 477303ff7de4bca54eb0a67f71ff603a612d2346 ima-ng sha1:69c3773f62f621217063daf98f0df244cb8b3d38 4026531840:/lib/modules/3.13.11-ckt39/kernel/drivers/char/lp.ko
11 6abed2753dfa4031f9122a13a457c62c8efa816f ima-ng sha1:4c82753983a3366732e9af22ddfc42fec7619413 4026531840:/lib/modules/3.13.11-ckt39/kernel/drivers/char/ppdev.ko
11 b3f494e378d1f7cb28befd5440a723673c05b5f7 ima-ng sha1:bc2691857c711780166b8ce70bb20d8f439a8e19 4026531840:/lib/modules/3.13.11-ckt39/kernel/drivers/parport/parport_pc.ko

==> runtime_measurements_count <==
794

==> violations <==
0
root@moby:/sys/kernel/security/ima#
```

- run docker daemon

```
unshare -m dockerd
```

- all information of docker daemon has been recorded

```
root@moby:/sys/kernel/security/ima# ls
4026532469  ascii_runtime_measurements  binary_runtime_measurements  cpcr  kernel_module_measurements  runtime_measurements_count  violations
root@moby:/sys/kernel/security/ima# cat 4026532469
4026532469 10a7e817ab08caedc66638e27d81006eb2fa2e1e ima-ng sha1:8bd883ec6e04713d4ff74c6180459c84402b8172 4026532469:/usr/bin/unshare
4026532469 34286d7a18bbab66c9d6950f388fee2ea7f39a2a ima-ng sha1:b3003acb2054f9d1c0845eb2022d96f16c12f34b 4026532469:/usr/bin/dockerd
4026532469 e6921b66bb0c97cf8e5f9ec5647b9611a07b12b8 ima-ng sha1:76e5b111d23adea7f6f221aaebfe83f58333d5b5 4026532469:/usr/bin/docker-containerd
4026532469 212e2192b218ecf229377b2751ce1277daea30f3 ima-ng sha1:7b40d27ad4b7777df2298c7f1b406a9556d4516f 4026532469:/lib/modules/3.13.11-ckt39/kernel/ubuntu/aufs/aufs.ko
4026532469 27f57fd024be758921e132b09cc5d906d36239e2 ima-ng sha1:1e84a7b6b7e72c069f94c14e010122478a02a54a 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/llc/llc.ko
4026532469 7f4b6147a2ac98321ccc0cff779c4ed47f0c209d ima-ng sha1:90337226892d174015250bcb0c7fd120e5e3bd3c 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/802/stp.ko
4026532469 af68abfee82a70e8bd0e0490ffab1bc0e5645247 ima-ng sha1:abcea61d0c39239adb476fc4414b619ac741b804 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/bridge/bridge.ko
4026532469 5e5b1c35b3a6a23d856db574eb4e8d1eb3903258 ima-ng sha1:90c882af0fef80f9bd7e0665868f913eec0e6fcb 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/netfilter/nf_conntrack.ko
4026532469 0078da79ec702f1a9b3f9a15ee1b7acd2ae1c90f ima-ng sha1:c57d7d3eaf7f4d35bac8c1e3885dda731af7a3d3 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/netfilter/nf_nat.ko
4026532469 e21783ef6f89c6b9908950b91ca4738e99383373 ima-ng sha1:e90dbe08b2080e67ea3b0fe95440b6aa7f81779a 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/netfilter/x_tables.ko
4026532469 56710141bd3d6e737b2ea268bd0ffbc4ab678522 ima-ng sha1:14d7316d627d3de0a4059830e5a7f0cd7f39bcde 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/netfilter/xt_conntrack.ko
4026532469 40fe3b62651b927569b61246dd19a8600547a34a ima-ng sha1:d8a94c405afa1eff7e171442f2db0cbc86ebb234 4026532469:/sbin/iptables
4026532469 62813400bf85212372b37b39dffab1ad3353149d ima-ng sha1:a82627630c31dd2c6a7525bcab97ab29cbe0ebe3 4026532469:/lib/x86_64-linux-gnu/libip4tc.so.0.1.0
4026532469 684d2b4522c29ce0ef4bd40074f31376346ffd5b ima-ng sha1:f559f66b59ee743a43bad9c6b5d1e2066138422f 4026532469:/lib/x86_64-linux-gnu/libip6tc.so.0.1.0
4026532469 77c4f87309dd866bf18121b400cff127e0d281ac ima-ng sha1:9dd74deddf9d0f27c4649c851ea1160d79e27509 4026532469:/lib/x86_64-linux-gnu/libxtables.so.11.0.0
4026532469 defe2bc938b18b93aa05b98a6bf5cc8637b258aa ima-ng sha1:3f668c447c0728179b423a116461db3a95d74a72 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/ipv4/netfilter/ip_tables.ko
4026532469 c21fa6422bed7a0ccafa68139aa1c9fb96c50f53 ima-ng sha1:67d2a08c0ad44b14c53538e0bff91dfdcc52be8b 4026532469:/lib/xtables/libxt_addrtype.so
4026532469 1c8a69c9fd560ffa6437c563156c8fcea7e0ad10 ima-ng sha1:4b681cc3d0577c3feac182b1e0ee507786d1eb94 4026532469:/lib/xtables/libxt_standard.so
4026532469 ae23fcc62f5aac6015eb63c158682cecea790540 ima-ng sha1:21de2ac08618738c1ac2ff76b0ebe738ac17dd09 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/xfrm/xfrm_algo.ko
4026532469 a48958410becee328ed03edf0346a59e534866a6 ima-ng sha1:4ce184053fe355a02ea4c848ecc52283205c0952 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/xfrm/xfrm_user.ko
4026532469 a6c65eaf43bf2fc64659291e3df33bda77a630ff ima-ng sha1:4d873040b6d191a58234f022fdcfa99a598dc04b 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/netfilter/nfnetlink.ko
4026532469 032479654b0b322d4b9fd5854b0f8eb39bdd2ee1 ima-ng sha1:d11621496920d3e2c59ad06455f79d8d7fabdb23 4026532469:/lib/modules/3.13.11-ckt39/kernel/net/netfilter/nf_conntrack_netlink.ko
4026532469 de17e14560bc130613e3dd23b7bae69aaf5411fd ima-ng sha1:cea10776418c6170076aa4dbdfc68dcee368656e 4026532469:/lib/xtables/libipt_MASQUERADE.so
4026532469 e13053436640b950002aa32497c67b79b1635190 ima-ng sha1:db1c209efb1738654a7f68e515536e2d5f817325 4026532469:/lib/xtables/libxt_conntrack.so
4026532469 4258b615408f3ac9cbe87b83cada823b2743805a ima-ng sha1:9d51706a3fe0d5ced57537c80da6075cab9ab2ee 4026532469:/usr/bin/docker-runc
4026532469 0e15dffeb2a12dbe64ea19b715da9de745df852d ima-ng sha1:ee0e918c05bdc39bf48ddf973fc88695df728b28 4026532469:/usr/bin/docker-init
root@moby:/sys/kernel/security/ima#
```

- run a container

```
root@moby:~# docker run -ti ubuntu:16.04
root@b00b2dcd8b74:/#
```

- information of this container has been recorded

```
root@moby:/sys/kernel/security/ima# ls
4026532469  4026532481  ascii_runtime_measurements  binary_runtime_measurements  cpcr  kernel_module_measurements  runtime_measurements_count  violations
root@moby:/sys/kernel/security/ima# cat 4026532481
4026532481 4b80269ef25931488b065b999d2462eefd2cfc40 ima-ng sha1:9d51706a3fe0d5ced57537c80da6075cab9ab2ee 4026532481:/usr/bin/docker-runc
4026532481 c889f1ef372a4d6ac7271618c52d47437b6c0533 ima-ng sha1:611a59c515074dbb376713fd19040c10a0c8e5e2 4026532481:/bin/bash
4026532481 8cea4c0e5b44e2c07f84ae6611625425ab8ed7b1 ima-ng sha1:bc659d9e2cb30539f49d1a008b685971b4f1e1a3 4026532481:/lib/x86_64-linux-gnu/ld-2.23.so
4026532481 815b835ca1fca565c8fda1229e105d015c02df8b ima-ng sha1:2bd9389f52439de7a42efcb8a3c3f8d4c881e8b2 4026532481:/lib/x86_64-linux-gnu/libtinfo.so.5.9
4026532481 7f30860f25acc1331373196de4482cc2a7d39e5b ima-ng sha1:a0a3f1428cd99e8646532ad1991f1f75631416b6 4026532481:/lib/x86_64-linux-gnu/libdl-2.23.so
4026532481 087de18ffc3d5d7d530f243801b3ec859457c56d ima-ng sha1:14c22be9aa11316f89909e4237314e009da38883 4026532481:/lib/x86_64-linux-gnu/libc-2.23.so
4026532481 927eaeb75073906ff806df200f6918c8a5567201 ima-ng sha1:747b3c4af6c2ed287d12d223fc9f1b5c3192eb34 4026532481:/lib/x86_64-linux-gnu/libnss_compat-2.23.so
4026532481 faa759df015e451163aac493dacce4d5d73759d5 ima-ng sha1:42e4fc53b2b73cc5f40c601ae3cace035c8fda9d 4026532481:/lib/x86_64-linux-gnu/libnsl-2.23.so
4026532481 056d55291c93a96a0e8eb8207a479c2480374305 ima-ng sha1:b5d30795e252df782a605ad6c2155805f0a6e94b 4026532481:/lib/x86_64-linux-gnu/libnss_nis-2.23.so
4026532481 f8f4aa07fa8cf560997e0ce6877c1d0710631825 ima-ng sha1:5cd2b270c29b368e1183a9c530c662483a49c703 4026532481:/lib/x86_64-linux-gnu/libnss_files-2.23.so
4026532481 a9bd82871ebb7c27bd013648657bba706c549d62 ima-ng sha1:00d1beed62bdd61647ee8548ad3d1f6bd2bf6621 4026532481:/usr/bin/groups
4026532481 e36d9a62fe473595f2b3661021f5adaf8b035926 ima-ng sha1:3c2f00e9ebf51520b63dda65f2c369fd4d3b90a9 4026532481:/usr/bin/dircolors
root@moby:/sys/kernel/security/ima#
```
