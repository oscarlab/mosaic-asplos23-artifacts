#!/bin/bash
set -x

cur_dir=$(pwd)
gem5_dir=$GEM5DIR
linux_kernel_dir=$KERN_SRC

sudo apt-get update
sudo apt-get install libpython-dev libpython-all-dev python-all-dev
sudo apt-get install qemu-system
sudo apt-get install scons

./compile.sh
qemu-img create $QEMU_IMG_FILE 10g
./create_qemu_img.sh
$BASE/test-scripts/copyapps_qemu.sh
