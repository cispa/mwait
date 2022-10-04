#!/bin/bash

function usage {
    echo "collect <url> <label> <count>"
    exit 1
}

if [ "$1" == "" ];
then
    usage
fi
if [ "$2" == "" ];
then
    usage
fi
if [ "$3" == "" ];
then
    usage
fi

for i in $(seq $3);
do
    ./main-monitorx $1
    mv log.txt $2_$i.txt
done

