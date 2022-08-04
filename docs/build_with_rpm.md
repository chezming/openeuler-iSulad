# Install iSulad with rpm package
You can also try to install iSulad using rpm with those provided rpm package. The rpm packages are in `docs/rpms`. 

**Attention**: Now we only provide rpm packges based on centos-8-X86-64.

## Build Steps on Centos 8

Go to the rpm packages direcotory `cd iSulad/docs/rpms`. 

### Install lxc
```shell
yum install -y yajl-2.1.0-10.el8.x86_64 rsync-3.1.3-12.el8.x86_64
rpm -Uvh lxc-libs-4.0.3-2022072501.x86_64.rpm
rpm -Uvh lxc-4.0.3-2022072501.x86_64.rpm
```

### Install lcr
```shell
yum instal -y pkgconf-pkg-config-1.4.2-1.el8.x86_64
rpm -Uvh lcr-2.1.0-2.x86_64.rpm
rpm -Uvh lcr-devel-2.1.0-2.x86_64.rpm
```

### Install protobuf
```shell
rpm -Uvh protobuf-3.14.0-4.x86_64.rpm
yum install -y emacs-26.1-7.el8.x86_64
rpm -Uvh protobuf-compiler-3.14.0-4.x86_64.rpm
```

### Install grpc
```shell
yum install -y c-ares-1.13.0-5.el8.x86_64 gperftools-libs-2.7-9.el8.x86_64
dnf --enablerepo=powertools install gflags-devel
rpm -Uvh grpc-1.31.0-1.x86_64.rpm
yum install -y openssl-devel.x86_64
rpm -Uvh grpc-devel-1.31.0-1.x86_64.rpm
```

### Install libarchive
```shell
rpm -Uvh libarchive-3.4.3-4.x86_64.rpm
rpm -Uvh libarchive-devel-3.4.3-4.x86_64.rpm
```

### Install libwebsockets
```shell
yum install -y epel-release
dnf --enablerepo=powertools install libuv-devel
dnf install libwebsockets-devel
```

### Install iSulad
```shell
dnf --enablerepo=powertools install http-parser-devel
yum install -y sqlite-devel.x86_64
rpm -Uvh iSulad-2.1.0-1.x86_64.rpm
```