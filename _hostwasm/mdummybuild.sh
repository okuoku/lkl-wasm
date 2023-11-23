#!/bin/sh
../_warp/bin/warp-cc -nostdlib -nostdinc -fPIC -g -c -o dummymath.o dummymath.c && \
    rm -rf _lib && \
    mkdir -p _lib && \
    ../_warp/bin/warp-ar crs _lib/libm.a dummymath.o
