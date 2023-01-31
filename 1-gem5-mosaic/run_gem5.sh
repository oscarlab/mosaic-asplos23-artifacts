#!/bin/bash
set -x

cur_dir=$(pwd)
gem5_dir=$GEM5DIR
gem5_config_dir=$gem5_dir/configs
linux_dir=$KERN_SRC

$gem5_dir/build/X86/gem5.opt $gem5_config_dir/example/fs.py --kernel $linux_dir/vmlinux --disk-image $QEMU_IMG_FILE --cpu-type X86KvmCPU --command-line "earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda" --caches --l2cache --mem-size 32768MB --sys-clock 4GHz --l1d_size 32kB --l1i_size 32kB --l2_size 256kB --l3_size 8192kB
