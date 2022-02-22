#!/bin/bash

# shall run as root to change sysfs
if [ $EUID -ne 0 ];
  then echo "Please run as root"
  exit
fi

rm /etc/udev/rules.d/83-groups.rules
udevadm control -R

rmmod groups
