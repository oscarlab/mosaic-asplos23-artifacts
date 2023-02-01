#Copy the below commands to run all graph500 configuration at once. 
#This uses a default TOC size of 4 and TOC size of 8, and varies 
# the associativity.

```
exec test-scripts/prun.sh graph500 1 4 5000 1 &
sleep 60
exec test-scripts/prun.sh graph500 2 4 5001 1 &
sleep 60
exec test-scripts/prun.sh graph500 4 4 5002 1 &
sleep 60
exec test-scripts/prun.sh graph500 8 4 5003 1 &
sleep 60
exec test-scripts/prun.sh graph500 1024 4 5004 1 &
sleep 60
exec test-scripts/prun.sh graph500 1 8 5005 1 &
sleep 60
exec test-scripts/prun.sh graph500 2 8 5006 1 &
sleep 60
exec test-scripts/prun.sh graph500 4 8 5007 1 &
sleep 60
exec test-scripts/prun.sh graph500 8 8 5008 1 &
sleep 60
exec test-scripts/prun.sh graph500 1024 8 5009 1 &
```
