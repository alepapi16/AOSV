#! /bin/bash

vm_shared="/home/alessio/VirtualBox_VMs/Ubuntu/shared-folder"

rm -r $vm_shared/*
make build
cp -t $vm_shared -r out/*
make clean
