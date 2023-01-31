#!/bin/bash
set -x

sudo umount $MOUNT_DIR
#Next, mount your image to the directory
mkdir $MOUNT_DIR
sudo mount -o loop $QEMU_IMG_FILE $MOUNT_DIR
sudo cp -r $BASE/apps $MOUNT_DIR/
sudo cp -r $BASE/apps/* $MOUNT_DIR/
sudo cp -r $BASE/m5 $MOUNT_DIR/
sudo umount $MOUNT_DIR
