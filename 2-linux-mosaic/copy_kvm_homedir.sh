#!/bin/bash

DISK="disk.img"
MNTPNT=`mktemp -d`

function fail {
	[ "$M1" = "1" ] && sudo umount $MNTPNT/dev
	[ "$M0" = "1" ] && sudo umount $MNTPNT
	rmdir $MNTPNT
	exit $1
}
function run {
	echo $@
	$@
	if [ $? != 0 ]; then
		echo "Failed [$*]"
		fail $?
	fi
}

run sudo mkdir -p $MNTPNT
run sudo mount -o loop $DISK $MNTPNT; M0=1
run sudo mkdir -p $MNTPNT/dev
run sudo mount --bind /dev/ $MNTPNT/dev; M1=1

echo "Disk image mounted. Type \"exit\" to unmount the image."

mkdir -p homedir
cp -r $MNTPNT/home/oscar/* homedir/

run sudo umount $MNTPNT/dev; M1=0
run sudo umount $MNTPNT; M0=0

run rmdir $MNTPNT
