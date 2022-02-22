#!/bin/bash

# shall run as root to change sysfs
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

cd ../bin/benchmark
../test.out install benchmark > /dev/null
status=$?
[ $status -eq 0 ] && group=benchmark || group=groups

# store previous values
msg=$(../test.out sysfs_read $group max_message_size)
stor=$(../test.out sysfs_read $group max_storage_size)

# set proper values of max_message[/storage]_size
../test.out sysfs_write $group max_message_size 10000 > /dev/null
../test.out sysfs_write $group max_storage_size 1000000000 > /dev/null

# launch benchmark
echo -ne '<                                     >\r'
echo -ne '< '
for x in 2 4 6 8 # nr. threads
do
	echo -ne $x
	echo -ne 'th '
	for y in 10 1000 5000 10000 # message size
	do
		echo -ne '.'
		for z in {1..10} # test iterations
		do
			./rw_tps.out $x $y $group || exit 1;
		done
	done
	echo -ne ' '
done
echo '>'

# restore previous values
../test.out sysfs_write $group max_message_size $msg > /dev/null
../test.out sysfs_write $group max_message_size $stor > /dev/null

# launch python plotting script (anaconda required)
# python plot.py

cd ../../scripts
