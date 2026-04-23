/* string.c — 通用内存/字符串工具函数 */

#include "types.h"

void* memset(void *dst, int c, uint64 n) {
    char *d = (char*)dst;
    while (n--) {
        *d++ = (char)c;
    }
    return dst;
}

// void* memmove(void *dst, const void *src, uint64 n) {
//     const char *s = src;
//     char *d = dst;
//     while (n--) *d++ = *s++;
//     return dst;
// }

void* memmove(void *dst, const void *src, uint64 n) {
    const char *s = src;
    char *d = dst;
    if (n == 0) return dst;
    /* 处理 src 和 dst 重叠的情况（从后往前拷贝）*/
    if (s < d && s + n > d) {
        s += n; d += n;
        while (n-- > 0) *--d = *--s;
    } else {
        while (n-- > 0) *d++ = *s++;
    }
    return dst;
}

void* memcpy(void *dst, const void *src, uint64 n) {
    return memmove(dst, src, n);
}

/* 1 - diff, 0 - equal */
int memcmp(const void *v1, const void *v2, uint64 n) {
    const uint8 *s1 = v1, *s2 = v2;
    while (n-- > 0) {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++; s2++;
    }
    return 0;
}

int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

int strncmp(const char *p, const char *q, uint64 n) {
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0) return 0;
    return (uint8)*p - (uint8)*q;
}

char* strncpy(char *s, const char *t, int n) {
    char *os = s;
    while (n-- > 0 && (*s++ = *t++) != 0);
    while (n-- > 0) *s++ = 0;
    return os;
}

char* safestrcpy(char *s, const char *t, int n) {
    char *os = s;
    if (n <= 0) return os;
    while (--n > 0 && (*s++ = *t++) != 0);
    *s = 0;
    return os;
}