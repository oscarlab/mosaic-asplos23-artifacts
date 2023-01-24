#!/bin/bash

DISK="disk.img"
SIZE="10G"
MNTPNT=`mktemp -d`
OS="focal"
ARCH="amd64"
KERNEL=`uname -r`
USER=oscar
PASS=oscar
TTYS=ttyS0
VMFILES="vmfiles"

#download and build BUSE
#git clone https://github.com/acozzette/BUSE.git vmfiles/BUSE
make -C vmfiles/BUSE busexmp


function fail {
	[ "$M1" = "1" ] && sudo umount $MNTPNT/dev
	[ "$M0" = "1" ] && sudo umount $MNTPNT
	[ "$D0" = "1" ] && rm $DISK
	rmdir $MNTPNT
	exit $1
}

function run {
	echo "$@"
	"$@"
	if [ $? != 0 ]; then
		echo "Failed [$*]"
		fail $?
	fi
}

run qemu-img create -f raw $DISK $SIZE; D0=1

run mkfs.ext4 -F $DISK

run sudo mkdir -p $MNTPNT
run sudo mount -o loop $DISK $MNTPNT; M0=1

run sudo debootstrap --arch $ARCH $OS $MNTPNT

function copy_files {
	for f in $*
	do
		run sudo cp -r $f $MNTPNT/${f%/*}
	done
}


run sudo mkdir -p $MNTPNT/dev
run sudo mount --bind /dev/ $MNTPNT/dev; M1=1

function chroot_run {
	run sudo chroot $MNTPNT "$@"
}

chroot_run rm -f /etc/init/tty[2345678].conf
chroot_run sed -i "s:/dev/tty\\[1-[2-8]\\]:/dev/tty1:g" /etc/default/console-setup

chroot_run adduser $USER --disabled-password --gecos ""
chroot_run bash -c "echo "$USER:$PASS" | chpasswd"

chroot_run sed -i "s/^ExecStart.*$/ExecStart=-\/sbin\/agetty --noissue --autologin oscar %I $TERM/g" /lib/systemd/system/getty@.service
chroot_run sed -i "/User privilege specification/a oscar\tALL=(ALL) NOPASSWD:ALL" /etc/sudoers


#copy files
run sudo cp $VMFILES/BUSE/busexmp $MNTPNT/usr/local/bin/
run sudo cp $VMFILES/bootscripts/*.sh $MNTPNT/etc/profile.d/
run sudo cp $VMFILES/scripts/*.sh $MNTPNT/home/$USER/
run sudo cp $VMFILES/scripts/*.py $MNTPNT/home/$USER/
run sudo cp -r $VMFILES/apps $MNTPNT/home/$USER/
chroot_run chown -R oscar:oscar /home/$USER/

chroot_run apt-get update
chroot_run apt-get install --yes sysstat psmisc
chroot_run apt-get install --yes libgomp1
chroot_run apt-get install --yes screen
#chroot_run apt-get install --yes build-essential autoconf
#chroot_run apt-get install --yes initramfs-tools
#chroot_run apt-get install --yes sysstat psmisc
#chroot_run apt-get install --yes time
#chroot_run apt-get install --yes vim




run sudo umount $MNTPNT/dev; M1=0
run sudo umount $MNTPNT; M0=0

run rmdir $MNTPNT; D0=0
