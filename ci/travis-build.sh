#!/bin/sh -ex

cat /etc/issue
PROOT=`pwd`

mkdir $PROOT/build
cd $PROOT/build

cmake -DCMAKE_BUILD_TYPE=Release ../
make
