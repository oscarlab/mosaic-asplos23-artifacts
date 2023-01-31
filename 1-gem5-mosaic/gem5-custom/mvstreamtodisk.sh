cd STREAM
gcc stream.c -o stream
cd ..
sudo mount -o loop qemu-image.img mountdir/
sudo scp STREAM/stream mountdir/root/
sudo umount mountdir
