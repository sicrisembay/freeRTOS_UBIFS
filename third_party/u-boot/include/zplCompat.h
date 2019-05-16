#ifndef _ZPL_COMPAT_H_
#define _ZPL_COMPAT_H_

#if defined(__TEST_APP__)
#include "stdlib.h"
#include "string.h"
#include "assert.h"
struct mtd_info *mtd;
static inline int fls64(int l) { assert(0); }
static inline int __ffs(int l) { assert(0); }
//#define atomic_set(v, i)     (((v)->counter) = (i))
//#define atomic64_set(v, i)   atomic_set(v, i)
#define local_irq_save(f)
#define local_irq_restore(f)
#include "asm/atomic.h"
#endif
#include "linux/kernel.h"
#include "stddef.h"
#include "stdio.h"
#include "ubiFsConfig.h"

#define _DEBUG      (1)

#if defined(__TEST_APP__)
#define __packed  __attribute__((packed))
#endif

#define CONFIG_SYS_CACHELINE_SIZE   32
#define ARCH_DMA_MINALIGN           CONFIG_SYS_CACHELINE_SIZE
#define CONFIG_SYS_MALLOC_LEN       (1 * 1024 * 1024)
#define CONFIG_MTD_UBI_BEB_LIMIT    20

#if !defined(__TEST_APP__)
#define __always_inline inline
#endif

typedef signed char s8;
typedef signed char __s8;
typedef unsigned char u8;
typedef unsigned char __u8;
typedef unsigned char u_char;
typedef unsigned char  uchar;
typedef volatile unsigned char  vu_char;

typedef signed short s16;
typedef signed short __s16;
typedef unsigned short u16;
typedef unsigned short __u16;
typedef volatile unsigned short vu_short;

#if !defined(__TEST_APP__)
typedef int ssize_t;
#endif
typedef signed int s32;
typedef signed int __s32;
typedef unsigned int u32;
typedef unsigned int __u32;
//typedef unsigned int uint32_t;
typedef unsigned long int ulong;
typedef unsigned long int u_long;
typedef unsigned long phys_addr_t;
typedef unsigned long phys_size_t;
typedef phys_addr_t resource_size_t;
typedef volatile unsigned long  vu_long;

typedef signed long long s64;
typedef signed long long __s64;
typedef unsigned long long u64;
typedef unsigned long long __u64;
typedef unsigned long long uint64_t;
#if !defined(__TEST_APP__)
typedef long long int __quad_t;
typedef unsigned long long int __u_quad_t;
//typedef __u_quad_t dev_t;
//typedef unsigned int uid_t;
//typedef unsigned int gid_t;
typedef __quad_t loff_t;
#endif

#define BITS_PER_LONG 32

#if !defined(__TEST_APP__)
typedef long long int   loff_t;
#endif
typedef unsigned short  __u16;
typedef unsigned int    __u32;
typedef __u16           __le16;
typedef __u32           __le32;

#define LLONG_MAX           ((long long)(((long long)(0)) >> 1))

struct unused {};
typedef struct unused unused_t;
typedef unused_t spinlock_t;

#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
#define ALIGN(x,a)              __ALIGN_MASK((x),(typeof(x))(a)-1)

#define __iomem
#define __user

#define readb(a)            (*(volatile unsigned char *)(a))
#define writeb(v, a)        (*(volatile unsigned char *)(a) = (v))
#define readw(a)            (*(volatile unsigned short *)(a))
#define writew(v, a)        (*(volatile unsigned short *)(a) = (v))

#define unlikely(x)         (x)
#define likely(x)           (x)

#include "linux/bitops.h"
#define hweight32(x)        generic_hweight32(x)
#define hweight16(x)        generic_hweight16(x)
#define hweight8(x)         generic_hweight8(x)

/* from asm/bitops.h */
static inline int test_bit(int nr, const void * addr) {
    return((unsigned char *)addr)[nr >> 3] & (1U << (nr & 7));
}

static inline int __test_and_set_bit(int nr, volatile void *addr) {
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
    unsigned long old = *p;

    *p = old | mask;
    return (old & mask) != 0;
}

static inline int test_and_set_bit(int nr, volatile void * addr) {
    unsigned long flags = 0;
    int out;

//    local_irq_save(flags);   /// TODO: Sicris, put this inside a critical section!
    out = __test_and_set_bit(nr, addr);
//    local_irq_restore(flags);

    return out;
}

#define WATCHDOG_RESET()

/* from compiler.h */
# define __user
# define __kernel
# define __safe
# define __force
# define __nocast
# define __iomem
# define __chk_user_ptr(x) (void)0
# define __chk_io_ptr(x) (void)0
# define __builtin_warning(x, y...) (1)
# define __must_hold(x)
# define __acquires(x)
# define __releases(x)
# define __acquire(x) (void)0
# define __release(x) (void)0
# define __cond_lock(x,c) (c)
# define __percpu
# define __rcu
# define __pmem

/* from little_endian.h */
#define cpu_to_le16(x)      ((__le16)(x))
#define le16_to_cpu(x)      ((__le16)(x))
#define le32_to_cpu(x)      ((__le32)(x))
#define __le32_to_cpu(x)    le32_to_cpu(x)
#define cpu_to_le32(x)      ((u32)(x))
#define __cpu_to_le32(x)    cpu_to_le32(x)
#define le64_to_cpu(x)      ((u64)(x))
#define cpu_to_le64(x)      ((u64)(x))
#define swap16(x)           ((__le16)( \
                                (((__le16)(x) & (__le16)0x00ff) << 8) | \
                                (((__le16)(x) & (__le16)0xff00) >> 8) \
                                ))
#define swap32(x)           ((__le32)( \
                                (((__le32)(x) & (__le32)0x000000ffUL) << 24) | \
                                (((__le32)(x) & (__le32)0x0000ff00UL) << 8) | \
                                (((__le32)(x) & (__le32)0x00ff0000UL) >> 8) | \
                                (((__le32)(x) & (__le32)0xff000000UL) >> 24) \
                                ))
#define swap64(x)           ((u64)( \
                                (((u64)(x) & (u64)0x00000000000000ff) << 56) | \
                                (((u64)(x) & (u64)0x000000000000ff00) << 40) | \
                                (((u64)(x) & (u64)0x0000000000ff0000) << 24) | \
                                (((u64)(x) & (u64)0x00000000ff000000) << 8)  | \
                                (((u64)(x) & (u64)0x000000ff00000000) >> 8)  | \
                                (((u64)(x) & (u64)0x0000ff0000000000) >> 24) | \
                                (((u64)(x) & (u64)0x00ff000000000000) >> 40) | \
                                (((u64)(x) & (u64)0xff00000000000000) >> 56)   \
                                ))
#define be16_to_cpu(x)      swap16(x)
#define cpu_to_be16(x)      swap16(x)
#define be32_to_cpu(x)      swap32(x)
#define cpu_to_be32(x)      swap32(x)
#define be64_to_cpu(x)      swap64(x)
#define cpu_to_be64(x)      swap64(x)
#include "linux/compat.h"

#define __must_check
#include "linux/err.h"

/* from poison.h */
#define LIST_POISON1       ((void *) 0x0)
#define LIST_POISON2       ((void *) 0x0)

/*
 * Output a debug text when condition "cond" is met. The "cond" should be
 * computed by a preprocessor in the best case, allowing for the best
 * optimization.
 */
#if 0
#define debug_cond(cond, fmt, args...)          \
    do {                                   \
        if (cond)                          \
            printf(fmt, ##args);                  \
    } while (0)
#else

#define MAX_LOG_LEN  (1024)
extern char logData[MAX_LOG_LEN+1];
#define debug_cond(cond, fmt, args...)          \
    do {                                   \
        if (cond) {                          \
            snprintf(logData, MAX_LOG_LEN, fmt, ##args); \
            logData[MAX_LOG_LEN] = '\0'; \
            BSP_UART_Send(0, logData, strlen(logData)); \
            BSP_UART_Send(0, "\r", 1); \
        } \
    } while (0)
#endif

/* Show a message if DEBUG is defined in a file */
#define debug(fmt, args...)         \
    debug_cond(_DEBUG, fmt, ##args)

#include "part.h"



#endif // End _ZPL_COMPAT_H_
