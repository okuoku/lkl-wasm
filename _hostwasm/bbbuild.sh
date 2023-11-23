#!/bin/sh
../../wabt/build/wasm2c --module-name=user --enable-threads -o user.c ../../busybox/busybox_unstripped \
&& echo "#include \"w2cfixup.h\"" >> user.h

