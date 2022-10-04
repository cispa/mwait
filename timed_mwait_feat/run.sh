#!/bin/bash
sudo ./enable-msr.sh
make
sudo insmod hello.ko && sleep 1 && sudo rmmod hello

sudo dmesg | grep "Avg:"
