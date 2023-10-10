#include <uapi/asm/unistd.h>

__SYSCALL(__NR_virtio_mmio_device_add, sys_virtio_mmio_device_add)

#define __SC_ASCII(t, a) #t "," #a

#define __ASCII_MAP0(m,...)
#define __ASCII_MAP1(m,t,a) m(t,a)
#define __ASCII_MAP2(m,t,a,...) m(t,a) "," __ASCII_MAP1(m,__VA_ARGS__)
#define __ASCII_MAP3(m,t,a,...) m(t,a) "," __ASCII_MAP2(m,__VA_ARGS__)
#define __ASCII_MAP4(m,t,a,...) m(t,a) "," __ASCII_MAP3(m,__VA_ARGS__)
#define __ASCII_MAP5(m,t,a,...) m(t,a) "," __ASCII_MAP4(m,__VA_ARGS__)
#define __ASCII_MAP6(m,t,a,...) m(t,a) "," __ASCII_MAP5(m,__VA_ARGS__)
#define __ASCII_MAP(n,...) __ASCII_MAP##n(__VA_ARGS__)

#ifdef __MINGW32__
#define SECTION_ATTRS "n0"
#define SECTION_ATTRS_TAIL
#define SECTION_ATTRS_TAIL_TEXT
#elif defined(__wasm__)
#define SECTION_ATTRS ""
#define SECTION_ATTRS_TAIL ",@"
#define SECTION_ATTRS_TAIL_TEXT ",\"\",@"
#else
#define SECTION_ATTRS "a"
#define SECTION_ATTRS_TAIL
#define SECTION_ATTRS_TAIL_TEXT
#endif

#define __SYSCALL_DEFINE_ARCH(x, name, ...)				\
	asm(".section .syscall_defs,\"" SECTION_ATTRS "\"" SECTION_ATTRS_TAIL "\n"		\
	    ".ascii \"#ifdef __NR" #name "\\n\"\n"			\
	    ".ascii \"SYSCALL_DEFINE" #x "(" #name ","			\
	    __ASCII_MAP(x, __SC_ASCII, __VA_ARGS__) ")\\n\"\n"		\
	    ".ascii \"#endif\\n\"\n"					\
	    ".section .text" SECTION_ATTRS_TAIL_TEXT "\n");
