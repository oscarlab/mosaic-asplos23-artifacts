#!/bin/bash

#./run.sh

for i in {0..9}
do
	./run.sh bt cg $i 0
	sleep 10
	./run.sh xb cg $i 0
	sleep 10
	./run.sh g5 cg $i 0
	sleep 10
done
