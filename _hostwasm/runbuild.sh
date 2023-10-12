#!/bin/sh

make -j10 -C ../tools/lkl ARCH=lkl CC=`pwd`/../_warp/bin/warp-cc LD=`pwd`/../_warp/bin/warp-ld \
AR=`pwd`/../_warp/bin/warp-ar NM=`pwd`/../_warp/bin/warp-nm OBJCOPY=llvm-objcopy \
KALLSYMS_EXTRA_PASS=1 CONFIG_OUTPUT_FORMAT=wasm32 CROSS_COMPILE=wasm32 V=1

exec sh ./buildline.sh
