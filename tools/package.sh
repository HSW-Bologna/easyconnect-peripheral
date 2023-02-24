#!/usr/bin/env sh

DIR=easyconnect-peripheral-$1_$2_$3
mkdir /tmp/$DIR
cp ./build/bootloader/bootloader.bin /tmp/$DIR
cp ./build/partition_table/partition-table.bin /tmp/$DIR
cp ./build/easyconnect-peripheral.bin /tmp/$DIR
cd /tmp
tar -czf $DIR.tar.gz $DIR
zip -r $DIR.zip $DIR
