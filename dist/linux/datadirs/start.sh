#!/bin/bash

mkdir -p ./datadir_main
cp ./rngcoin.main.conf ./datadir_main/rngcoin.conf
../src/qt/rngcoin-qt -datadir=./datadir_main &

for i in {1..2}
do
	mkdir -p ./datadir_${i}
	export DATADIR_I=$i
	envsubst < ./rngcoin.sub.conf > ./datadir_${i}/rngcoin.conf
	../src/qt/rngcoin-qt -datadir=./datadir_${i} &
	sleep 1
done
