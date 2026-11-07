/* AURELIA OS - fundamental types and compiler attributes */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#define AURELIA_NAME     "AURELIA OS"
#define AURELIA_VERSION  "1.0.0"
#define AURELIA_CODENAME "Solstice"
#define AURELIA_BUILD    "alpha"
#define AURELIA_ARCH     "x86_64"

#define PACKED      __attribute__((packed))
#define NORETURN    __attribute__((noreturn))
#define UNUSED      __attribute__((unused))
#define ALIGNED(n)  __attribute__((aligned(n)))
#define SECTION(s)  __attribute__((section(s)))
#define USED        __attribute__((used))

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uintptr_t uptr;
typedef intptr_t  iptr;

#define NULLPTR ((void *)0)

static inline u64 align_up(u64 value, u64 align) {
    return (value + align - 1) & ~(align - 1);
}

static inline u64 align_down(u64 value, u64 align) {
    return value & ~(align - 1);
}

static inline u64 u64_min(u64 a, u64 b) { return a < b ? a : b; }
static inline u64 u64_max(u64 a, u64 b) { return a > b ? a : b; }
static inline i32 i32_abs(i32 v) { return v < 0 ? -v : v; }
static inline i32 i32_min(i32 a, i32 b) { return a < b ? a : b; }
static inline i32 i32_max(i32 a, i32 b) { return a > b ? a : b; }
static inline i32 i32_clamp(i32 v, i32 lo, i32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Common privilege-independent segment selectors. */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_DS   0x18
#define USER_CS   0x20

/* Serial + kernel log */
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3
