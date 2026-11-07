/* AURELIA OS - freestanding string and memory routines */
#pragma once

#include <aurelia/types.h>

void  *memset(void *dst, int value, size_t count);
void  *memcpy(void *dst, const void *src, size_t count);
void  *memmove(void *dst, const void *src, size_t count);
int    memcmp(const void *a, const void *b, size_t count);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);
int    atoi(const char *s);
u64    strtou64(const char *s, int base, const char **end);
bool   str_starts_with(const char *s, const char *prefix);
bool   str_ends_with(const char *s, const char *suffix);
void   str_tolower(char *s);
u32    hash_string(const char *s);
