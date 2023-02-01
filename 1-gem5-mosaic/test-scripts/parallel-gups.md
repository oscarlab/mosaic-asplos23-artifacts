#Copy the below commands to run all gups configuration at once. 
#This uses a default TOC size of 4 and TOC size of 8, and varies 
# the associativity.

```
exec test-scripts/prun.sh gups 1 4 6000 1 &
sleep 60
exec test-scripts/prun.sh gups 2 4 6001 1 &
sleep 60
exec test-scripts/prun.sh gups 4 4 6002 1 &
sleep 60
exec test-scripts/prun.sh gups 8 4 6003 1 &
sleep 60
exec test-scripts/prun.sh gups 1024 4 6004 1 &
sleep 60
exec test-scripts/prun.sh gups 1 8 6005 1 &
sleep 60
exec test-scripts/prun.sh gups 2 8 6006 1 &
sleep 60
exec test-scripts/prun.sh gups 4 8 6007 1 &
sleep 60
exec test-scripts/prun.sh gups 8 8 6008 1 &
sleep 60
exec test-scripts/prun.sh gups 1024 8 6009 1 &
```
