#include "./../kernel/types.h"
#include "./../kernel/stat.h"
#include "./../kernel/fcntl.h"
#include "./../user/user.h"

//
// wrapper so that it's OK if main() does not call exit().
//
void
_main() {
  extern int main();
  main();
  exit(0);
}

char *
strcpy(char *s, const char *t) {
  char *os;

  os = s;
  while ((*s++ = *t++) != 0);
  return os;
}

int
strcmp(const char *p, const char *q) {
  while (*p && *p == *q)
    p++, q++;
  return (uchar) *p - (uchar) *q;
}

uint
strlen(const char *s) {
  int n;

  for (n = 0; s[n]; n++);
  return n;
}

void *
memset(void *dst, int c, uint n) {
  char *cdst = (char *) dst;
  int i;
  for (i = 0; i < n; i++) {
    cdst[i] = c;
  }
  return dst;
}

char *
strchr(const char *s, char c) {
  for (; *s; s++)
    if (*s == c)
      return (char *) s;
  return 0;
}

char *
gets(char *buf, int max) {
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    cc = read(0, &c, 1);
    if (cc < 1 || c == '\n' || c == '\r')
      break;
    buf[i++] = c;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st) {
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if (fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s) {
  if (strlen(s) == 0)
    return 0;

  uint8 negate = 0;
  if (s[0] == '-' || s[0] == '+') {
    if (s[0] == '-')
      negate = 1;
    s += 1;
  }

  int n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';

  return negate ? -n : n;
}

void *
memmove(void *vdst, const void *vsrc, int n) {
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  if (src > dst) {
    while (n-- > 0)
      *dst++ = *src++;
  } else {
    dst += n;
    src += n;
    while (n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

int
memcmp(const void *s1, const void *s2, uint n) {
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

void *
memcpy(void *dst, const void *src, uint n) {
  return memmove(dst, src, n);
}

char *
strlower(char *str) {
  for (uint i = 0; i < strlen(str); i++) {
    if ((65 <= str[i]) && (str[i] <= 90)) {
      str[i] += 32;
    }
  }

  return str;
}
