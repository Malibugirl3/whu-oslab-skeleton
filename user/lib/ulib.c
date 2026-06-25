#include <stdarg.h>

#include "user.h"

static void putc_fd(int fd, char c) {
  write(fd, &c, 1);
}

static void print_int_width(int x, int width, int left_align) {
  char buf[16];
  int i = 0;
  int neg = 0;
  int len;

  if (x == 0) {
    buf[i++] = '0';
  } else {
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
  }

  len = i;
  if (!left_align) {
    for (int p = len; p < width; p++)
      write(1, " ", 1);
  }
  write(1, buf, len);
  if (left_align) {
    for (int p = len; p < width; p++)
      write(1, " ", 1);
  }
}

static void print_str_width(const char *s, int width, int left_align) {
  int len = strlen(s);

  if (len > width)
    len = width;
  if (!left_align) {
    for (int p = len; p < width; p++)
      write(1, " ", 1);
  }
  write(1, (char *)s, len);
  if (left_align) {
    for (int p = len; p < width; p++)
      write(1, " ", 1);
  }
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
  print_int_width(x, 0, 0);
}

void print_padded(const char *s, int width) {
  print_str_width(s, width, 1);
}

void print_int_padded(int x, int width) {
  print_int_width(x, width, 0);
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

    int left_align = 0;
    int width = 0;

    if (fmt[i] == '-') {
      left_align = 1;
      i++;
    }

    while (fmt[i] >= '0' && fmt[i] <= '9') {
      width = width * 10 + (fmt[i] - '0');
      i++;
    }

    if (fmt[i] == '\0')
      break;

    switch (fmt[i]) {
    case 'd':
      print_int_width(va_arg(ap, int), width, left_align);
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
      if (width > 0)
        print_str_width(s, width, left_align);
      else
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
