/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_INSTRUCTION_POINTER_H
#define _LINUX_INSTRUCTION_POINTER_H

#ifndef __wasm__
#define _RET_IP_		(unsigned long)__builtin_return_address(0)
#else
#define _RET_IP_		(unsigned long)0xdeadcafe
#endif

#ifndef __wasm__
#define _THIS_IP_  ({ __label__ __here; __here: (unsigned long)&&__here; })
#else
#define _THIS_IP_ (unsigned long)0xdeadcafe
#endif

#endif /* _LINUX_INSTRUCTION_POINTER_H */
