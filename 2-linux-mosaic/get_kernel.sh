#!/bin/sh

# 1. Download Linux kernel 5.11.6
curl -O https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.11.6.tar.xz
tar xf linux-5.11.6.tar.xz
mv linux-5.11.6 linux
tar xf linux-5.11.6.tar.xz
rm linux-5.11.6.tar.xz

# 1. Alternatively, download from git
#git clone -b v5.11.6 https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git linux


# 2. Patch Mosaic changes
patch -p1 -d linux < mosaic.patch

# 3. Use kernel config for kvm
cp config_kvm linux/.config
cp config_kvm linux-5.11.6/.config

# 4. Build kernel
make -C linux
make -C linux-5.11.6
