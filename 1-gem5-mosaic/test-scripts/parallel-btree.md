#Copy the below commands to run all btree configuration at once. 
#This uses a default TOC size of 4 and TOC size of 8, and varies 
# the associativity.

```
exec test-scripts/prun.sh btree 1 4 4000 1 &
sleep 60
exec test-scripts/prun.sh btree 2 4 4001 1 &
sleep 60
exec test-scripts/prun.sh btree 4 4 4002 1 &
sleep 60
exec test-scripts/prun.sh btree 8 4 4003 1 &
sleep 60
exec test-scripts/prun.sh btree 1024 4 4004 1 &
sleep 60
exec test-scripts/prun.sh btree 1 8 4005 1 &
sleep 60
exec test-scripts/prun.sh btree 2 8 4006 1 &
sleep 60
exec test-scripts/prun.sh btree 4 8 4007 1 &
sleep 60
exec test-scripts/prun.sh btree 8 8 4008 1 &
sleep 60
exec test-scripts/prun.sh btree 1024 8 4009 1 &
```
