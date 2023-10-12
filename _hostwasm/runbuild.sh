#!/bin/sh

make -j10 -C ../tools/lkl ARCH=lkl CC=`pwd`/../_warp/bin/warp-cc LD=`pwd`/../_warp/bin/warp-ld \
AR=`pwd`/../_warp/bin/warp-ar NM=`pwd`/../_warp/bin/warp-nm OBJCOPY=llvm-objcopy \
KALLSYMS_EXTRA_PASS=1 CONFIG_OUTPUT_FORMAT=wasm32 CROSS_COMPILE=wasm32 V=1

../_warp/bin/warp-cc -g -nostdlib -Wl,--no-entry -Wl,--export=init -Wl,--error-limit=0 \
-Wl,--verbose -Wl,--Map=out.map -o lin.wasm ../lib/lib.a ../vmlinux.a hostwasm_main.c
