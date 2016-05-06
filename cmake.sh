#!/bin/sh

dir_base=`pwd`
dir_prj=$(cd $(dirname $0); pwd)
#dir_build=$dir_prj/.build

cd $dir_prj

make $@
RET=$?

cd $dir_base
exit $RET
