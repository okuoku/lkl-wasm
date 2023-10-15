#!/bin/sh
cmake -P geninittbl.cmake && \
../_warp/bin/warp-cc -c -g -I ../include -I ../include/uapi -I ../arch/lkl/include -I ../arch/lkl/include/generated -I ../arch/lkl/include/generated/uapi -o initschedclasses.o initschedclasses.c && \
../_warp/bin/warp-cc -g -nostdlib -Wl,--no-entry -Wl,--export=init -Wl,--error-limit=0 \
-Wl,--Map=lin.map -o lin.wasm -I ../arch/lkl/include ../lib/lib.a ../vmlinux.a initschedclasses.o hostwasm_main.c hostwasm_lklops.c initsyms.gen.c && wasm2c -o lin.c lin.wasm

