#!/bin/bash

# shall run as root to change sysfs
if [ $EUID -ne 0 ];
  then echo "Please run as root"
  exit
fi

cp ../udev/83-groups.rules /etc/udev/rules.d
udevadm control -R

insmod ../obj/kernel/groups.ko
