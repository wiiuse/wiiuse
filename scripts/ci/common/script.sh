#!/bin/bash
set -ev

mkdir -p build
cd build
cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..
make
#make install
