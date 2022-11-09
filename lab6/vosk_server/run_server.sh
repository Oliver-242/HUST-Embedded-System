#!/bin/sh

DIR=`dirname $0`
cd $DIR

export LD_LIBRARY_PATH="`pwd`"

./server

