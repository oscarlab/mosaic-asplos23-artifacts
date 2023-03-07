# Mosaic Gem5 Full System Simulation Instructions


## System Requirements for Gem5 full system simulation
----------------------------------------------------
- A Linux system (tested with Debian distribution)
- 4 or more cores
- At least 20GB of free RAM
- 20-30GB disk for sequential runs and 100GB for parallel runs


## (1) Setting  Gem5 Mosaic on CloudLab (Skip to 2 if not using CloudLab)
-----------------------------------------------------------------------
You have the option to run Gem5 Mosaic on CloudLab (which we used for
development and experiments). If you do not want to use CloudLab, skip to 2.

## 1.1 Instantiating CloudLab nodes before cloning the repo 
We run the gem5 simulation in CloubLab and use its fast NVMe storage to speed up full system simulation. We recommend users use the following profile to instantiate two CloudLab nodes.

**CloudLab profile:** You could use any CloudLab machine with Intel-based CPUs running Debian kernel version 18.04.
**Recommended Instance Types:** We recommend using m510 nodes in UTAH datacenter that have fast NVMe storage and are generally available.
You could also use a pre-created profile, "2-NVMe-Nodes," which will launch two NVMe-based nodes to run several parallel instances.

## 1.2 Creating an SSD partition and cloning the code on SSD.

Before you clone the repo, if you are using CloudLab, the root partitions only has 16GB (for example: m510 instances). 
First, set up the CloudLab node with SSD and download the code on the SSD folder.

If you are using the m510 nodes (or 2-NVMe-Nodes profile), the NVMe SSD with 256GB is in
"/dev/nvme0n1p4"

### 1.3 To partition an ext4 file system, use the following commands
```
sudo mkfs.ext4 /dev/nvme0n1p4 
mkdir ~/ssd
sudo mount /dev/nvme0n1p4  ~/ssd
sudo chown -R $USER ~/ssd
```

### 1.4 Now clone the repo and "cd" into 1-gem5-mosaic
```
cd ~/ssd
git clone https://github.com/oscarlab/mosaic-asplos23-artifacts
cd mosaic-asplos23-artifacts/1-gem5-mosaic
```

## (2) Compilation
------------------------

All the package installations before compilation use debian distribution and "apt-get"  

### 2.1 Setting the environmental variables
MAKE sure to set the correct OS codename with scripts/setvars.sh. 

For example, 
```
export OS_RELEASE_NAME=`lsb_release -a | grep "Codename:" | awk '{print $2}'`
source scripts/setvars.sh $OS_RELEASE_NAME
```
**Note: Anytime you logout or change sessions before or at the time of compilation or when running experiments, you 
must set the environmental variables again** 



### 2.2 Compile gem5 and linux 4.17 kernel. 

Feel free to use other kernel versions if required.

```
./compile.sh
```
## 2.2 QEMU image setup for gem5 full system simulation
Create a 10GB QEMU image. 
```
qemu-img create $QEMU_IMG_FILE 10g
```
```
./create_qemu_img.sh
```

### 2.3 Mount the QEMU image  
```
test-scripts/mount_qemu.sh
sudo apt-get update
sudo apt-get install build-essential g++
exit 
```
### 2.4 Copying applications and gem5 scripts to VM
To copy all apps to the root folder inside the QEMU image, use the following
command. In addition to the application, we also copy the m5 application
required for letting the gem5 host for starting and stoping the simulation.
```
$BASE/test-scripts/copyapps_qemu.sh
```

### 2.5 Setting the password for QEMU VM
- Set the VM image password to the letter "s". This is just a QEMU gem5 VM and will not cause any 
issues. *We will make them configurable soon to avoid using a specific password.*

```
test-scripts/mount_qemu.sh
passwd
```
Once the password is set, exit the VM and umount the image

```
exit
cd $BASE
test-scripts/umount_qemu.sh
```
### 2.6 Disabling paranoid mode
```
echo "-1" | sudo tee /proc/sys/kernel/perf_event_paranoid
```

### 2.7 Optional: Using MOSH to avoid SSH session termination
Because some simulations run for days, one could use mosh-based SSH or screen to avoid termination of SSH session. 
More details here: 

To install and use mosh 
```
sudo apt-get install mosh
mosh USER@host
```


## (3.) Running Simulations
----------------------
We use "test-scripts/prun.sh," a reasonably automated script to run the
simulations for different applications.

In the AE, as evaluated in the paper, we have included the sample applications: 
(1) hello, (2) graph500, (3) xsbench, (4) btree, or (5) gups

We support two workloads: 
(1) "tiny" input, with the smallest possible input to quickly check if everything works 
( < 2 minutes for each application except gups and btree which would run for a longer time)
(2) "large" input as used in the paper (could take several hours to days)


### 3.1 Setting the environmental variables
```
export OS_RELEASE_NAME=`lsb_release -a | grep "Codename:" | awk '{print $2}'`
source scripts/setvars.sh $OS_RELEASE_NAME
```

### 3.2 Running the gem5 simulator with Mosaic 
```
test-scripts/prun.sh $APPNAME $ASSOCIATIVITY $TOCSIZE $TELNET_PORT $USE_LARGE_INPUT
```
**NOTE: For each WAYS and TOCSIZE configuration, a separate copy of the repo is generated along with the VM image, and the simulation is run from the copied folder. Please be mindful of the available disk and memory size.**

#### Examples:
To run graph500, xsbench, btree, gups sequentially (one after the other) with  2-way associativity with TOC size of 4 and TELNET port at 3160, with **tiny input**

```
export OS_RELEASE_NAME=`lsb_release -a | grep "Codename:" | awk '{print $2}'`
source scripts/setvars.sh $OS_RELEASE_NAME
```

```
test-scripts/prun.sh graph500 2 4 10160 0 
sleep 20
test-scripts/prun.sh xsbench 2 4 10161 0
sleep 20
test-scripts/prun.sh btree 2 4 10162 0
sleep 20
test-scripts/prun.sh gups 2 4 10163 0
sleep 20
```

To sequentially run all applications with large inputs
```
test-scripts/prun.sh graph500 2 4 10160 1
sleep 60
test-scripts/prun.sh xsbench 2 4 10161 1
sleep 60
test-scripts/prun.sh btree 2 4 10162 1
sleep 60
test-scripts/prun.sh gups 2 4 10163 1
```

For direct-mapped TLBs (associativity=1) with TOC size of 4 and TELNET port at 3161, 
```
test-scripts/prun.sh btree 1 16 10161 1
```

For full associativity, we just specify the number of ways as TLB size. 
For example, if the TLB_SIZE=1024 (default) 
```
test-scripts/prun.sh graph500 1024 16 10162 0
```

**Some notes:**
To run an application inside a VM and begin the simulation, we need to login to
the QEMU VM, run gem5's *m5 exit* inside the VM, followed by running an
application (also inside the VM), and finally run *m5 exit* to signal the gem5
simulator in the host that an application has finished execution and it is time
to stop the simulation.

To avoid manually logging into a VM and running an application, we use telnet
and a specific PORT number.


### 3.3 Running the gem5 simulator with Vanilla
```
test-scripts/pvanilla.sh $APPNAME $ASSOCIATIVITY $TELNET_PORT $USE_LARGE_INPUT
```
The steps for running Vanilla design is same except the script name.

#### Examples:
To run graph500, xsbench, btree, gups sequentially (one after the other) with  2-way associativity with **tiny input**

```
export OS_RELEASE_NAME=`lsb_release -a | grep "Codename:" | awk '{print $2}'`
source scripts/setvars.sh $OS_RELEASE_NAME
```

To run just graph500
```
test-scripts/pvanilla.sh graph500 2  10160 0
```

The output appears in the format follows:
```
RESULTS for APP: graph500 WAYS: 2
----------------------------------------------------------
Vanilla TLB miss rate:0.7265%
----------------------------------------------------------
```

To run all workloads:

```
test-scripts/pvanilla.sh graph500 2  10160 0
sleep 20
test-scripts/pvanilla.sh xsbench 2  10161 0
sleep 20
test-scripts/pvanilla.sh btree 2 10162 0
sleep 20
test-scripts/pvanilla.sh gups 2 10163 0
sleep 20
```


### 3.4 To run multiple Mosaic instances simultaneously
To run multiple instances in parallel, the instances could be run as background (interactive) tasks.
Simply copy the block from each markdown file and paste it on the terminal for the long running jobs to begin.
The number of jobs to launch depend on the number of cores, memory size, and available disk size.

```
test-scripts/parallel-graph500.md
test-scripts/parallel-btree.md
test-scripts/parallel-xsbench.md
test-scripts/parallel-gups.md
```

You could also do it manually as shown below (for tiny input):

First, set the environmental variables.
```
export OS_RELEASE_NAME=`lsb_release -a | grep "Codename:" | awk '{print $2}'`
source scripts/setvars.sh $OS_RELEASE_NAME
```

Next,

```
exec test-scripts/prun.sh $APPNAME $ASSOCIATIVITY $TOCSIZE $TELNET_PORT $USE_LARGE_INPUT &
```
For example, the following lines run graph500 varying the associativity, keeping the TOC size constant. 
Please make sure to use different port numbers.
```
exec test-scripts/prun.sh graph500 2 4 10000 0 &
sleep 20
exec test-scripts/prun.sh graph500 4 4 10001 0 &
sleep 20
exec test-scripts/prun.sh graph500 8 4 10002 0
```

We note that gem5's TELNET can be, at times, unstable for simultaneous telnet invocation. If you see a "connection refused error" for some specific configuration, we recommend either re-running just the specific configuration or terminating all instances using step 3.6 below and increasing the sleep time between each execution. 


The above commands would generate a (sample) output in the format shown below:
```
----------------------------------------------------------
RESULTS for WAYS 2 and TOC LEN 4
----------------------------------------------------------
Mosaic TLB miss rate:0.6426%
----------------------------------------------------------
RESULTS for WAYS 4 and TOC LEN 4
----------------------------------------------------------
Mosaic TLB miss rate:0.6312%
----------------------------------------------------------
RESULTS for WAYS 8 and TOC LEN 4
----------------------------------------------------------
Mosaic TLB miss rate:0.6414%
```

For large inputs (e.g., xsbench)
```
exec test-scripts/prun.sh xsbench 2 4 13160 1 &
sleep 60
exec test-scripts/prun.sh xsbench 4 4 13161 1 &
sleep 60
exec test-scripts/prun.sh xsbench 8 4 13162 1 &
sleep 60
exec test-scripts/prun.sh xsbench 1024 4 13163 1
```

### 3.5 Setting TLB size
To change the default TLB size, in prun.sh, change the following:
```
TLB_SIZE=1024 => TLB_SIZE=1536
```


### 3.6 Changing application parameters
The inputs for tiny and large inputs for each application can be changed in the follow python scripts
```
$BASE/test-scripts/gem5_client_tiny.py
$BASE/test-scripts/gem5_client_large.py
```
For example, to change the scale and the number of edges 
of graph500 workloads, modify the following command in tiny or large python scripts
```
commands = [b"/m5 exit\r\n", b"/seq-list -s 4 -e 4\r\n", b"/m5 exit\r\n" ]
```


### 3.7 Forceful termination if required
If you would like to terminate all scripts and gem5 simulation, one could use the following commands
```
PID=`ps axf | grep Ways | grep -v grep | awk '{print $1}'`;kill -9 $PID
PID=`ps axf | grep prun.sh | grep -v grep | awk '{print $1}'`;kill -9 $PID
```

## (4.) Result Generation
-------------------------
In full system simulation, for each memory reference, we use a Vanilla TLB and
a parallel Iceberg TLB to collect the TLB miss rate for both Vanilla and
Mosaic.

After an application has finished running for a configuration, the miss rates
are printed as shown below.

```
Sample output:
Mosaic TLB miss rate:0.7307%
```
## 4.1 Result Files
The result files can be seen in the result folder inside the base gem5-linux
($BASE) folder. The result file path is of the following structure:
```
result/$APP/iceberg/$ASSOCIATIVTY/$TOCSIZE
e.g., result/graph500/iceberg/2way/toc4
```

You could also see the TLB hit and miss logs for each 10M instructions in *tlblogs* file.


