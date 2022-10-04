#!/bin/bash

#enable and make performance counters available, run it with sudo
modprobe msr
echo "2" > /sys/devices/cpu/rdpmc

#enable timed Mwait
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xe2); ENABLED=$(printf "%x" $((0x$CUR | 2147483648))); wrmsr -a 0xe2 0x$ENABLED'
