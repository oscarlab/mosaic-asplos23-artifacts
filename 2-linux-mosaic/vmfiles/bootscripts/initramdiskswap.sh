#!/bin/sh

echo "Creating a 4GB BUSE ramdisk for swap"
sudo systemd-run busexmp 4096M /dev/nbd0
sleep 3
sudo mkswap /dev/nbd0
