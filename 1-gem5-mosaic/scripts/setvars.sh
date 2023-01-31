export BASE=$PWD
export GEM5DIR=$BASE/gem5-custom

#export NPROC=$(grep -c processor /proc/cpuinfo)

#Pass the release name
export OS_RELEASE_NAME=$1
export KERN_SRC=$BASE/linux-4.17
#CPU parallelism
export CORES="32"
export VER="4.17.0"

#QEMU
export QEMU_IMG=$BASE
export QEMU_IMG_FILE=$QEMU_IMG/iceberg.img
export MOUNT_DIR=$QEMU_IMG/mountdir
export QEMUMEM="50G"
export KERNEL=$BASE/KERNEL
export OUTPUTDIR=$BASE/OUTPUT 

#BENCHMARKS AND LIBS
export LINUX_SCALE_BENCH=$BASE/linux-scalability-benchmark
export APPBENCH=$BASE/apps

#SCRIPTS
export SCRIPTS=$BASE/scripts
export APPPREFIX="numactl --preferred=0 /usr/bin/time -v"
# ccache for Linux development
export CC="gcc"
export CXX="g++"
# Set cache size




#Commands
mkdir $OUTPUTDIR
mkdir $KERNEL
