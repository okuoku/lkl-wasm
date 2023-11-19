#!/bin/sh
../_warp/bin/warp-cc -shared -nostdlib -nostdinc -fPIC -g -Wl,--no-entry -Wl,--export=_start_c -Wl,--error-limit=0 -Wl,--Map=user.map \
    -Wl,--import-memory -Wl,--import-table \
    -isystem /usr/lib/clang/18/include -isystem /home/oku/repos/musl/prefix/include -o user.wasm dummymath.c /home/oku/repos/musl/prefix/lib/crt1.o testlibc.c /home/oku/repos/musl/prefix/lib/libc.a && \
../../wabt/build/wasm2c --module-name=user --enable-threads -o user.c user.wasm \
&& echo "#include \"w2cfixup.h\"" >> user.h

