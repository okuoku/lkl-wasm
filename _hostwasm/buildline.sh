#!/bin/sh

exec ../_warp/bin/warp-cc -g -nostdlib -Wl,--no-entry -Wl,--export=init -Wl,--error-limit=0 \
-Wl,--verbose -Wl,--Map=out.map -o lin.wasm -I ../arch/lkl/include ../lib/lib.a ../vmlinux.a hostwasm_main.c  hostwasm_lklops.c

