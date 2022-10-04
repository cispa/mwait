#!/bin/bash

rm -f log.txt log.csv
touch log.txt log.csv
for to in $(seq 20000 1000 200000);
do
	echo $to
	echo ": $to" >> log.txt
	sudo wrmsr -a 0xe1 $(printf "0x%x" $to)
	./main 3 | tee res.txt
	cat res.txt >> log.txt
	echo $to,$(tail -1 res.txt) >> log.csv
done
sudo wrmsr -a 0xe1 0x186a0
