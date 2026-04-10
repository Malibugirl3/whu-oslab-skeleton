/* string.c — 通用内存/字符串工具函数 */

#include "types.h"

void* memset(void *dst, int c, uint64 n) {
    char *d = (char*)dst;
    while (n--) {
        *d++ = (char)c;
    }
    return dst;
}

void* memmove(void *dst, const void *src, uint64 n) {
    const char *s = src;
    char *d = dst;
    while (n--) *d++ = *s++;
    return dst;
}

void* memcpy(void *dst, const void *src, uint64 n) {
    return memmove(dst, src, n);
}