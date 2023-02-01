#!/bin/bash
#set -x

#variables
BASE=$PWD
GEM5_ORIGINAL_CLONE=$BASE
#qemu disk location
#do it manually
QEMU_DISK=$BASE/iceberg.img

cd ../
COPYDIR=$PWD
cd $BASE

if [ "$#" -ne 5 ]; then
    echo "Illegal number of parameters. Check README"
    echo "Format: test-scripts/prun.sh "appname" "associativity ways" "toc size" "tcp port" "large input?""
    echo "example: test-scripts/prun.sh graph500 2 4 3160 0"
    exit
fi


#gem5 parallel running directories
GEM5_DIRECT=$COPYDIR/direct
GEM5_2WAY=$COPYDIR/2way
GEM5_4WAY=$COPYDIR/4way
GEM5_8WAY=$COPYDIR/8way
GEM5_FULLY=$COPYDIR/fully
GEM5_VANILLA=$COPYDIR/vanilla
#dirs=("direct" "2way" "4way" "8way" "fully")

#gem5 results directories
#APPNAME=hello
#APPNAME=gups
#APPNAME=graph500
#APPNAME=xsbench
#APPNAME=btree
#Intializing TCP_PORT 
#TCP_PORT=3460

#Iceberg TOC list
APPNAME=$1
WAYS=$2
TOC=$3
TCP_PORT=$4
USE_LARGE_WORKLOAD=$5


GEM5_RESULTS_DIR=result
#gem5 cmd line args
GEM5_MEM_SIZE=16384MB
#TLB entry rows
TLB_SIZE=1024
#CPUS reserved for running QEMU VM
NUM_CPUS=1

#Some basic steps
sudo chown $USER /dev/kvm

GEM5_OPT_NEW_NAME_ORIG="gem5.opt"
GEM5_OPT_NEW_NAME="gem5.opt"

#compile linux and gem5
COMPILE() {
    source $GEM5_ORIGINAL_CLONE/scripts/setvars.sh
    $GEM5_ORIGINAL_CLONE/compile.sh
    sudo chown -R $USER $GEM5_ORIGINAL_CLONE/gem5-custom
    cp $GEM5_ORIGINAL_CLONE/gem5-custom/build/X86/gem5.opt $GEM5_ORIGINAL_CLONE/gem5-custom/build/X86/$GEM5_OPT_NEW_NAME
}

#creating a clone of the gem5 for a specific way
create_target_gem5_dirs() {

	TARGET_DIR=$1
	ASSOC_CONFIG=$2

	if [ ! -d $TARGET_DIR ]
	then
	    #echo "creating $ASSOC_CONFIG way dir..."
	    mkdir -p $TARGET_DIR
	    #echo "copying to $TARGET_DIR..."
	    cp -a $GEM5_ORIGINAL_CLONE/. $TARGET_DIR
	fi
}


#functions to run single gem5, wait for completion, kill process and collect result
#function to collect results
move_stats_to_results_dir () {
    STAT_FILE=$1
    TLB_SET_ASSOC=$2
    ICEBERG_ENABLED=$3
    TOC_LEN=$4
    RESULT_SUBDIR=vanilla
    if [ $ICEBERG_ENABLED -eq 1 ]
    then
        RESULT_SUBDIR=iceberg
    fi
    RESULT_SUBDIR="${RESULT_SUBDIR}/${TLB_SET_ASSOC}way"
    if [ $ICEBERG_ENABLED -eq 1 ]
    then
        RESULT_SUBDIR="${RESULT_SUBDIR}/toc${TOC_LEN}"
    fi
    RESULT_LOCATION=$BASE/$GEM5_RESULTS_DIR/$APPNAME/$RESULT_SUBDIR
    #echo $RESULT_LOCATION
    if [ ! -d $RESULT_LOCATION ]
    then
        mkdir -p $RESULT_LOCATION
    fi

    #echo "Writing and moving  output $STAT_FILE to $RESULT_LOCATION"
    mv $STAT_FILE $RESULT_LOCATION
}

#function to check if output file exists and if so, terminate gem5
check_tlblogs_exist () {

    FILE=$1/m5out/stats.txt	
    TLBLOGS=$1/tlblogs
    TLBRESULT=$1/tlboutput.txt
    OUTPUTLOG=$1/output.log

    WAYS_VAL=$2
    TOC_LEN=$4

    if [ -f "$TLBRESULT" ]; then
	    rm -rf $TLBRESULT
    fi

    while :
    do
	if [ -f "$FILE" ]; then    
		var=`cat $FILE | grep "sim_ticks" | awk '{print $2}'`;
	fi

	if [ ! -z "$var" ]
	then
		let val=$var
		#if [ -f $1 ]
		if [ "$val" -gt "10000" ]
		#then echo "reasonable output"; fi    
	        then
		    #echo "reasonable output $GEM5_OPT_NEW_NAME"	&> $OUTPUTLOG
		    PID=`pidof $GEM5_OPT_NEW_NAME`; kill -9 $PID &>> $OUTPUTLOG
		    sleep 2
		    PID=`pidof $GEM5_OPT_NEW_NAME`; kill -9 $PID &>> $OUTPUTLOG

		    cat $TLBLOGS | grep "Vanilla TLB miss rate" | tail -1 &> $TLBRESULT
		    cat $TLBLOGS | grep "Mosaic TLB miss rate" | tail -1 &>> $TLBRESULT

		    echo "----------------------------------------------------------"
		    echo "RESULTS for APP: $APPNAME WAYS: $WAYS_VAL and TOCSIZE: $TOC_LEN"
		    echo "----------------------------------------------------------"
		    cat $TLBRESULT 	
		    echo "----------------------------------------------------------"

        	    #move tlblogs to result directory
            	    move_stats_to_results_dir $FILE $2 $3 $4
		    move_stats_to_results_dir $TLBLOGS $2 $3 $4
		    move_stats_to_results_dir $TLBRESULT $2 $3 $4
		   return		
		   #PID=`ps axf | grep run.sh | grep -v grep | awk '{print $1}'`;kill -9 $PID
        	fi
	fi		
    done
}

#iceberg function
run_single_iceberg_config() {

    TLB_SET_ASSOC=$1
    TOC_LEN=$2
    GEM5_OPT=$3/gem5-custom/build/X86/$GEM5_OPT_NEW_NAME
    FS_CONFIG_SCRIPT=$3/gem5-custom/configs/example/fs.py
    LINUX_BINARY_LOCATION=$3/linux-4.17/vmlinux
    PORT=$4
    TLBLOGS_LOCATION=$3
    STATSFILE=$3/m5out/stats.txt

    #clear the existing output
    if [ -f "$STATSFILE" ]; then
            rm -rf $STATSFILE
    fi

    #build gem5 command line
    GEM5_CMDLINE="$GEM5_OPT $FS_CONFIG_SCRIPT --kernel $LINUX_BINARY_LOCATION --disk-image $QEMU_DISK --cpu-type X86KvmCPU --command-line \"earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda -device e1000,netdev=net0 -netdev user,id=net0,hostfwd=tcp::5555-:22\" --caches --l2cache --mem-size $GEM5_MEM_SIZE --TCP_listening_port $4 --num-cpus $NUM_CPUS --tlb_num_entries $TLB_SIZE --tlb_set_associativity_L1 $TLB_SET_ASSOC --simulateIcebergTLB --toc_size $TOC_LEN"

    if [ "$USE_LARGE_WORKLOAD" -eq "1" ]; then
   	INPUTSCRIPT=$BASE/test-scripts/gem5_client_large.py
	echo "USING LARGE INPUT" > out.txt
    else
	INPUTSCRIPT=$BASE/test-scripts/gem5_client_tiny.py
	echo "USING TINY INPUT" > out.txt
    fi

    #echo "check_tlblogs_exist $TLBLOGS_LOCATION $1 1 $2 & ( sleep 5 && python3 $INPUTSCRIPT $PORT $APPNAME) & sh -c "$GEM5_CMDLINE""
    #run gem5 cmd
    check_tlblogs_exist $TLBLOGS_LOCATION $1 1 $2 & ( sleep 5 && python3 $INPUTSCRIPT $PORT $APPNAME) & sh -c "$GEM5_CMDLINE" &>> out.txt
}

create_exe_with_new_name() {

	NAME=$GEM5_OPT_NEW_NAME_ORIG
	ADD="Ways-$1-TOC-$2"
	DIR=$3
	APP=$4
	EXENAME=$APP"-"$ADD"-"$NAME
	
	#echo "cp $GEM5_ORIGINAL_CLONE/gem5-custom/build/X86/gem5.opt $DIR/gem5-custom/build/X86/$EXENAME"
	cp $GEM5_ORIGINAL_CLONE/gem5-custom/build/X86/gem5.opt $DIR/gem5-custom/build/X86/$EXENAME

	GEM5_OPT_NEW_NAME=$EXENAME
}

#function to run gem5 in single TLB configuration
run_TLB_config() {

    TLB_CONFIG=$1
    #run iceberg
    GEM5_DIRECTORY=$GEM5_DIRECT

    if [ $TLB_CONFIG -eq  2 ]
    then
        GEM5_DIRECTORY=$GEM5_2WAY
    elif [ $TLB_CONFIG -eq  4 ]
    then
        GEM5_DIRECTORY=$GEM5_4WAY
    elif [ $TLB_CONFIG -eq  8 ]
    then
        GEM5_DIRECTORY=$GEM5_8WAY
    elif [ $TLB_CONFIG -eq $TLB_SIZE ]
    then
        GEM5_DIRECTORY=$GEM5_FULLY
    fi

    #for TOC in ${toclist[@]}; do
	GEM5_TOC_DIRECTORY=$GEM5_DIRECTORY"/TOC_$TOC/$APPNAME"
	#echo $GEM5_TOC_DIRECTORY
	#Create a separate directory	
    	create_target_gem5_dirs $GEM5_TOC_DIRECTORY $TLB_CONFIG $TOC
	cd $GEM5_TOC_DIRECTORY
    
   	create_exe_with_new_name $1 $TOC $GEM5_TOC_DIRECTORY $APPNAME
        run_single_iceberg_config $1 $TOC $GEM5_TOC_DIRECTORY $2
    #done
}

NEW_PORT=$TCP_PORT
run_TLB_config $WAYS $NEW_PORT



################### UNUSED FUNCTIONS############################################
#vanilla function
run_single_vanilla_config() {
    TLB_SET_ASSOC=$1
    GEM5_OPT=$2/gem5-custom/build/X86/gem5.opt
    FS_CONFIG_SCRIPT=$2/gem5-custom/configs/example/fs.py
    LINUX_BINARY_LOCATION=$2/linux-4.17/vmlinux
    #build gem5 command line
    GEM5_CMDLINE="$GEM5_OPT $FS_CONFIG_SCRIPT --kernel $LINUX_BINARY_LOCATION --disk-image $QEMU_DISK --cpu-type X86KvmCPU --command-line \"earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda\" --caches --l2cache --mem-size $GEM5_MEM_SIZE --tlb_num_entries $TLB_SIZE --tlb_set_associativity_L1 $TLB_SET_ASSOC"
    #run gem5 cmd
    sh -c "$GEM5_CMDLINE"
}
























################### UNUSED FUNCTIONS############################################
#vanilla function
run_single_vanilla_config() {
    TLB_SET_ASSOC=$1
    GEM5_OPT=$2/gem5-custom/build/X86/gem5.opt
    FS_CONFIG_SCRIPT=$2/gem5-custom/configs/example/fs.py
    LINUX_BINARY_LOCATION=$2/linux-4.17/vmlinux
    #build gem5 command line
    GEM5_CMDLINE="$GEM5_OPT $FS_CONFIG_SCRIPT --kernel $LINUX_BINARY_LOCATION --disk-image $QEMU_DISK --cpu-type X86KvmCPU --command-line \"earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda\" --caches --l2cache --mem-size $GEM5_MEM_SIZE --tlb_num_entries $TLB_SIZE --tlb_set_associativity_L1 $TLB_SET_ASSOC"
    #run gem5 cmd
    sh -c "$GEM5_CMDLINE"
}




#COMPILE
#run_TLB_config $TLB_SIZE $TCP_PORT 
#sleep 5
for WAYS in ${waylist[@]}; do

	NEW_PORT=$TCP_PORT
	#echo "run_TLB_config $WAYS $NEW_PORT"
	run_TLB_config $WAYS $NEW_PORT 
	#sleep 30
        #python3 $BASE/test-scripts/gem5_client_$APPNAME.py $NEW_PORT &
	#let TCP_PORT=$NEW_PORT+1
	#sleep 20
done
