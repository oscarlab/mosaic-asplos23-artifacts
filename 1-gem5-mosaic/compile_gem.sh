#!/bin/bash
set -x

cur_dir=$(pwd)
gem5_dir=$GEM5DIR
linux_kernel_dir=$KERN_SRC

sudo apt-get install libpython-dev libpython-all-dev python-all-dev
sudo apt-get install qemu-system
sudo apt-get install scons

#compile gem5
cd $gem5_dir
export NUM_CPU=$CORES
/usr/bin/env python2 $(which scons)  -j $CORES --clean
/usr/bin/env python2 $(which scons)  -j $CORES build/X86/gem5.opt


cd $gem5_dir/util/term
make
gcc  -o m5term term.c
sudo make install
sudo install -o root -m 555 m5term /usr/local/bin
#To connect to terminal
#m5term localhost 3456
