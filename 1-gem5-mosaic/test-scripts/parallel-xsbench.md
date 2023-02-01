#Copy the below commands to run all xsbench configuration at once.
#This uses a default TOC size of 4 and TOC size of 8, and varies
# the associativity.

```
exec test-scripts/prun.sh xsbench 1 4 3000 1 &
sleep 60
exec test-scripts/prun.sh xsbench 2 4 3001 1 &
sleep 60
exec test-scripts/prun.sh xsbench 4 4 3002 1 &
sleep 60
exec test-scripts/prun.sh xsbench 8 4 3003 1 &
sleep 60
exec test-scripts/prun.sh xsbench 1024 4 3004 1 &
sleep 60
exec test-scripts/prun.sh xsbench 1 8 3005 1 &
sleep 60
exec test-scripts/prun.sh xsbench 2 8 3006 1 &
sleep 60
exec test-scripts/prun.sh xsbench 4 8 3007 1 &
sleep 60
exec test-scripts/prun.sh xsbench 8 8 3008 1 &
sleep 60
exec test-scripts/prun.sh xsbench 1024 8 3009 1 &
```
