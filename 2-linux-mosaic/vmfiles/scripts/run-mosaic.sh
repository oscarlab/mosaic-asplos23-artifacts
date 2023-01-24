#!/bin/bash

#./run.sh

for i in {0..9}
do
	./run.sh bt ice $i 0
	cat /proc/iutil > bt_${i}_ice_util.txt
	sleep 10
	./run.sh xb ice $i 0
	cat /proc/iutil > xb_${i}_ice_util.txt
	sleep 10
	./run.sh g5 ice $i 0
	cat /proc/iutil > g5_${i}_ice_util.txt
	sleep 10
done
