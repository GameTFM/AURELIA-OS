/* AURELIA OS - freestanding string and memory routines */
#include <aurelia/kstring.h>

void *memset(void *dst, int value, size_t count)
{
    u8 *d = (u8 *)dst;
    for (size_t i = 0; i < count; i++) {
        d[i] = (u8)value;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t count)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t count)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    if (d == s || count == 0) {
        return dst;
    }
    if (d < s) {
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t count)
{
    const u8 *x = (const u8 *)a;
    const u8 *y = (const u8 *)b;
    for (size_t i = 0; i < count; i++) {
        if (x[i] != y[i]) {
            return (int)x[i] - (int)y[i];
        }
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

size_t strnlen(const char *s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n]) {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0') {
            return (int)(u8)a[i] - (int)(u8)b[i];
        }
    }
    return 0;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) {
    }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = '\0';
    }
    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return (c == 0) ? (char *)s : NULLPTR;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULLPTR;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return (c == 0) ? (char *)s : (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle) {
        return (char *)haystack;
    }
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) {
            return (char *)haystack;
        }
    }
    return NULLPTR;
}

int atoi(const char *s)
{
    int sign = 1;
    int value = 0;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }
    return value * sign;
}

u64 strtou64(const char *s, int base, const char **end)
{
    u64 value = 0;
    bool any = false;
    while (*s == ' ') {
        s++;
    }
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    for (;; s++) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'f') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'F') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }
        if (digit >= base) {
            break;
        }
        value = value * (u64)base + (u64)digit;
        any = true;
    }
    if (end) {
        *end = any ? s : NULLPTR;
    }
    return value;
}

bool str_starts_with(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) {
            return false;
        }
    }
    return true;
}

bool str_ends_with(const char *s, const char *suffix)
{
    size_t sl = strlen(s);
    size_t fl = strlen(suffix);
    if (fl > sl) {
        return false;
    }
    return strcmp(s + (sl - fl), suffix) == 0;
}

void str_tolower(char *s)
{
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z') {
            *s = (char)(*s - 'A' + 'a');
        }
    }
}

u32 hash_string(const char *s)
{
    u32 hash = 2166136261u;
    while (*s) {
        hash ^= (u8)*s++;
        hash *= 16777619u;
    }
    return hash;
}
