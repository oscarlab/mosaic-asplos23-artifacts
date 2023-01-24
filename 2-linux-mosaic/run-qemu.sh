#!/bin/bash

while getopts wv opt; do
    case $opt in
        w) opt_rw="SET"
        ;;
        v) opt_vanilla="SET"
        ;;
    esac
done

if [[ "$opt_rw" = "SET" ]]; then
    SNAPSHOT=""
else
    SNAPSHOT="-snapshot"
fi

CONSOLE="console=tty1 highres=off $SERIAL_APPEND"
ROOT="root=/dev/hda rw --no-log"
NCPUS=2

if [[ "$opt_vanilla" = "SET" ]]; then
    KERNEL=linux-5.11.6/arch/x86_64/boot/bzImage
else
    KERNEL=linux/arch/x86_64/boot/bzImage
fi

set -x

qemu-system-x86_64 -enable-kvm -cpu host -m 12G -smp $NCPUS -hda disk.img -kernel $KERNEL \
	-append "nokaslr $CONSOLE $ROOT" \
	-curses $SNAPSHOT $SERIAL
