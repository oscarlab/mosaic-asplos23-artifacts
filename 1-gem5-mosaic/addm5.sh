#!/bin/bash
set -x

cur_dir=$(pwd)
gem5_dir=$GEM5DIR

#cd $gem5_dir/util/m5

mkdir $MOUNT_DIR

sudo mount -o loop $QEMU_IMG_FILE $MOUNT_DIR
sudo scp m5 $MOUNT_DIR/root/

sudo umount $MOUNT_DIR
