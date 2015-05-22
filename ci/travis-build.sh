#!/bin/sh -ex

PROOT=`pwd`

mkdir $PROOT/build
cd $PROOT/build

cmake -DCMAKE_BUILD_TYPE=Release ../
make