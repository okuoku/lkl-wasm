#!/bin/sh
../_warp/bin/warp-cc -shared -nostdlib -nostdinc -fPIC -g -Wl,--no-entry -Wl,--export=main -Wl,--error-limit=0 -Wl,--Map=user.map -Wl,--import-memory -Wl,--import-table -isystem /usr/lib/clang/18/include -o user.wasm testuser.c && \
../../wabt/build/wasm2c --module-name=user --enable-threads -o user.c user.wasm

