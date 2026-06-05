#include <stdarg.h>

#include "user.h"

static void putc_fd(int fd, char c) {
  write(fd, &c, 1);
}

int strlen(const char *s) {
  int n = 0;
  while (s[n] != '\0')
    n++;
  return n;
}

int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
  char *ret = dst;
  while ((*dst++ = *src++) != '\0')
    ;
  return ret;
}

void *memset(void *dst, int c, int n) {
  unsigned char *p = (unsigned char *)dst;
  for (int i = 0; i < n; i++)
    p[i] = (unsigned char)c;
  return dst;
}

void *memmove(void *dst, const void *src, int n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  if (d == s || n <= 0)
    return dst;

  if (d < s) {
    for (int i = 0; i < n; i++)
      d[i] = s[i];
  } else {
    for (int i = n - 1; i >= 0; i--)
      d[i] = s[i];
  }
  return dst;
}

void puts(const char *s) {
  write(1, (char *)s, strlen(s));
}

void print_int(int x) {
  char buf[16];
  int i = 0;
  int neg = 0;

  if (x == 0) {
    write(1, "0", 1);
    return;
  }

  if (x < 0) {
    neg = 1;
    x = -x;
  }

  while (x > 0) {
    buf[i++] = '0' + (x % 10);
    x /= 10;
  }

  if (neg)
    buf[i++] = '-';

  for (int l = 0, r = i - 1; l < r; l++, r--) {
    char t = buf[l];
    buf[l] = buf[r];
    buf[r] = t;
  }

  write(1, buf, i);
}

void print_hex(unsigned long x) {
  char buf[2 + sizeof(unsigned long) * 2];
  int i = 0;

  if (x == 0) {
    write(1, "0", 1);
    return;
  }

  while (x > 0) {
    int d = x & 0xF;
    buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
    x >>= 4;
  }

  for (int l = 0, r = i - 1; l < r; l++, r--) {
    char t = buf[l];
    buf[l] = buf[r];
    buf[r] = t;
  }

  write(1, buf, i);
}

void uprintf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] != '%') {
      putc_fd(1, fmt[i]);
      continue;
    }

    i++;
    if (fmt[i] == '\0')
      break;

    switch (fmt[i]) {
    case 'd':
      print_int(va_arg(ap, int));
      break;
    case 'x':
      print_hex((unsigned long)va_arg(ap, unsigned int));
      break;
    case 'p':
      puts("0x");
      print_hex(va_arg(ap, unsigned long));
      break;
    case 's': {
      char *s = va_arg(ap, char *);
      if (s == 0)
        s = "(null)";
      puts(s);
      break;
    }
    case 'c':
      putc_fd(1, (char)va_arg(ap, int));
      break;
    case '%':
      putc_fd(1, '%');
      break;
    default:
      putc_fd(1, '%');
      putc_fd(1, fmt[i]);
      break;
    }
  }

  va_end(ap);
}
