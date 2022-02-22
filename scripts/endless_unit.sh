#!/bin/bash

x=0
n=1
rm -f ../bin/logs/endless_unit.log
touch ../bin/logs/endless_unit.log
while [ $x == 0 ]
do
	echo -ne "Test n. "; echo $n  # Print on screen
	echo -ne "Test n. " >> ../bin/logs/unit.log ; echo $n >> ../bin/logs/unit.log  # Print on file
	x=$(./install.sh
		sleep 0.5
		cd ../bin
		./unit.out >> logs/unit.log  # Run test
		x=$?  # Store return value
		cd ../scripts
		./remove.sh
		echo $x  # Forward return value
	)
	n=$(($n+1-$x))  # Increase accumulator
done
echo "\t\tFailed."
