#ifndef SGX_STDIO_UTIL_H
#define SGX_STDIO_UTIL_H

//#include <libio.h>
#include <tlibc/stdio.h>
#include "struct/sgx_stdio_struct.h"
#include "proxy/sgx_stdio_t.h"
#include <stdlib.h>
#include <string.h>

#define _IO_off_t __off_t
#ifdef _IO_MTSAFE_IO
/* _IO_lock_t defined in internal headers during the glibc build.  */
#else
typedef void _IO_lock_t;
#endif
//struct _IO_FILE {
//	int _flags;		/* High-order word is _IO_MAGIC; rest is flags. */
//#define _IO_file_flags _flags
//
//	/* The following pointers correspond to the C++ streambuf protocol. */
//	/* Note:  Tk uses the _IO_read_ptr and _IO_read_end fields directly. */
//	char* _IO_read_ptr;	/* Current read pointer */
//	char* _IO_read_end;	/* End of get area. */
//	char* _IO_read_base;	/* Start of putback+get area. */
//	char* _IO_write_base;	/* Start of put area. */
//	char* _IO_write_ptr;	/* Current put pointer. */
//	char* _IO_write_end;	/* End of put area. */
//	char* _IO_buf_base;	/* Start of reserve area. */
//	char* _IO_buf_end;	/* End of reserve area. */
//	/* The following fields are used to support backing up and undo. */
//	char *_IO_save_base; /* Pointer to start of non-current get area. */
//	char *_IO_backup_base;  /* Pointer to first valid character of backup area */
//	char *_IO_save_end; /* Pointer to end of non-current get area. */
//
//	struct _IO_marker *_markers;
//
//	struct _IO_FILE *_chain;
//
//	int _fileno;
//#if 0
//	int _blksize;
//#else
//	int _flags2;
//#endif
//	_IO_off_t _old_offset; /* This used to be _offset but it's too small.  */
//
//#define __HAVE_COLUMN /* temporary */
//	/* 1+column number of pbase(); 0 is unknown. */
//	unsigned short _cur_column;
//	signed char _vtable_offset;
//	char _shortbuf[1];
//
//	/*  char* _save_gptr;  char* _save_egptr; */
//
//	_IO_lock_t *_lock;
//#ifdef _IO_USE_OLD_IO_FILE
//	};
//
//struct _IO_FILE_complete
//{
//  struct _IO_FILE _file;
//#endif
//#if defined _G_IO_IO_FILE_VERSION && _G_IO_IO_FILE_VERSION == 0x20001
//	_IO_off64_t _offset;
//# if defined _LIBC || defined _GLIBCPP_USE_WCHAR_T
//	/* Wide character stream stuff.  */
//  struct _IO_codecvt *_codecvt;
//  struct _IO_wide_data *_wide_data;
//  struct _IO_FILE *_freeres_list;
//  void *_freeres_buf;
//# else
//	void *__pad1;
//	void *__pad2;
//	void *__pad3;
//	void *__pad4;
//# endif
//	size_t __pad5;
//	int _mode;
//	/* Make sure we don't get into trouble again.  */
//	char _unused2[15 * sizeof (int) - 4 * sizeof (void *) - sizeof (size_t)];
//#endif
//};

typedef struct _IO_FILE FILE;

#define WRAPBUFSIZ 15000



int printf(const char *fmt, ...);

int fprintf(FILE* fp, const char* fmt, ...);

int rename (const char *__old, const char *__new);

// Fix me: Proposed wrapper of asprintf
int asprintf(char **s, const char* format, ...);

FILE* fdopen(int fd, const char* mode);

int fileno(FILE* fp);

int fclose(FILE *fp);

int fputs(const char *str, FILE* fp);

char* fgets(char* str, int num, FILE* fp);

int feof(FILE* file);

size_t fread(void* ptr, size_t size, size_t nmemb, FILE *fp);

size_t fwrite(const void* ptr, size_t size, size_t count, FILE *fp);

int fseeko(FILE* fp, off_t offset, int whence);

off_t  ftello(FILE *fp);

int fseek(FILE *fp, off_t offset, int whence);

off_t  ftell(FILE *fp);

int ferror(FILE *fp);

int fflush(FILE *fp);

int vfprintf(FILE *fp, const char* format, void *val);

int vprintf(const char* format, void* val);

int vsprintf(char* string, const char* format, void* val);

int vasprintf(char** string, const char* format, void* val);

int getc(FILE *fp);

int vfscanf(FILE *fp, const char *format, void *ap);

int vscanf(const char *format, void* ap);

int vsscanf(const char* s, const char *format, void* ap);

int putchar(int c);

int putc(int c, FILE *fp) ;

int fputc(int c, FILE *fp);

int fscanf(FILE *fp, const char* format, ...);

FILE* fopen(const char* filename, const char* mode);

void rewind(FILE *fp);

int sprintf (char *__restrict __s,
					const char *__restrict __format, ...);

int sscanf (const char *__restrict __s,
            const char *__restrict __format, ...);

int remove (const char *__filename);

#ifdef USE_INNER_SSCANF

#include <ctype.h>
#include <limits.h>
#include <assert.h>

#define HIDDEN_MAX_SCANF_WIDTH 9999
#define HIDDEN_TOR_ISDIGIT(A) isdigit(A)
#define HIDDEN_TOR_ISXDIGIT(A) isxdigit(A)
#define HIDDEN_TOR_ISSPACE(A) isspace(A)
#define hidden_tor_assert(A) assert(A)

static inline int
hidden_hex_decode_digit(char c)
{
  switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'A': case 'a': return 10;
    case 'B': case 'b': return 11;
    case 'C': case 'c': return 12;
    case 'D': case 'd': return 13;
    case 'E': case 'e': return 14;
    case 'F': case 'f': return 15;
    default:
      return -1;
  }
}

/** Helper: given an ASCII-encoded decimal digit, return its numeric value.
 * NOTE: requires that its input be in-bounds. */
static inline int
hidden_digit_to_num(char d)
{
  int num = ((int)d) - (int)'0';
  hidden_tor_assert(num <= 9 && num >= 0);
  return num;
}

/** Helper: Read an unsigned int from *<b>bufp</b> of up to <b>width</b>
 * characters.  (Handle arbitrary width if <b>width</b> is less than 0.)  On
 * success, store the result in <b>out</b>, advance bufp to the next
 * character, and return 0.  On failure, return -1. */
static inline int
hidden_scan_unsigned(const char **bufp, unsigned long *out, int width, int base)
{
  unsigned long result = 0;
  int scanned_so_far = 0;
  const int hex = base==16;
  hidden_tor_assert(base == 10 || base == 16);
  if (!bufp || !*bufp || !out)
    return -1;
  if (width<0)
    width=HIDDEN_MAX_SCANF_WIDTH;

  while (**bufp && (hex?HIDDEN_TOR_ISXDIGIT(**bufp):HIDDEN_TOR_ISDIGIT(**bufp))
         && scanned_so_far < width) {
    int digit = hex?hidden_hex_decode_digit(*(*bufp)++):hidden_digit_to_num(*(*bufp)++);
    unsigned long new_result = result * base + digit;
    if (new_result < result)
      return -1; /* over/underflow. */
    result = new_result;
    ++scanned_so_far;
  }

  if (!scanned_so_far) /* No actual digits scanned */
    return -1;

  *out = result;
  return 0;
}

/** Helper: Read an signed int from *<b>bufp</b> of up to <b>width</b>
 * characters.  (Handle arbitrary width if <b>width</b> is less than 0.)  On
 * success, store the result in <b>out</b>, advance bufp to the next
 * character, and return 0.  On failure, return -1. */
static inline int
hidden_scan_signed(const char **bufp, long *out, int width)
{
  int neg = 0;
  unsigned long result = 0;

  if (!bufp || !*bufp || !out)
    return -1;
  if (width<0)
    width=HIDDEN_MAX_SCANF_WIDTH;

  if (**bufp == '-') {
    neg = 1;
    ++*bufp;
    --width;
  }

  if (hidden_scan_unsigned(bufp, &result, width, 10) < 0)
    return -1;

  if (neg) {
    if (result > ((unsigned long)LONG_MAX) + 1)
      return -1; /* Underflow */
    *out = -(long)result;
  } else {
    if (result > LONG_MAX)
      return -1; /* Overflow */
    *out = (long)result;
  }

  return 0;
}

/** Helper: Read a decimal-formatted double from *<b>bufp</b> of up to
 * <b>width</b> characters.  (Handle arbitrary width if <b>width</b> is less
 * than 0.)  On success, store the result in <b>out</b>, advance bufp to the
 * next character, and return 0.  On failure, return -1. */
static inline int
hidden_scan_double(const char **bufp, double *out, int width)
{
  int neg = 0;
  double result = 0;
  int scanned_so_far = 0;

  if (!bufp || !*bufp || !out)
    return -1;
  if (width<0)
    width=HIDDEN_MAX_SCANF_WIDTH;

  if (**bufp == '-') {
    neg = 1;
    ++*bufp;
  }

  while (**bufp && HIDDEN_TOR_ISDIGIT(**bufp) && scanned_so_far < width) {
    const int digit = hidden_digit_to_num(*(*bufp)++);
    result = result * 10 + digit;
    ++scanned_so_far;
  }
  if (**bufp == '.') {
    double fracval = 0, denominator = 1;
    ++*bufp;
    ++scanned_so_far;
    while (**bufp && HIDDEN_TOR_ISDIGIT(**bufp) && scanned_so_far < width) {
      const int digit = hidden_digit_to_num(*(*bufp)++);
      fracval = fracval * 10 + digit;
      denominator *= 10;
      ++scanned_so_far;
    }
    result += fracval / denominator;
  }

  if (!scanned_so_far) /* No actual digits scanned */
    return -1;

  *out = neg ? -result : result;
  return 0;
}

/** Helper: copy up to <b>width</b> non-space characters from <b>bufp</b> to
 * <b>out</b>.  Make sure <b>out</b> is nul-terminated. Advance <b>bufp</b>
 * to the next non-space character or the EOS. */
static inline int
hidden_scan_string(const char **bufp, char *out, int width)
{
  int scanned_so_far = 0;
  if (!bufp || !out || width < 0)
    return -1;
  while (**bufp && ! HIDDEN_TOR_ISSPACE(**bufp) && scanned_so_far < width) {
    *out++ = *(*bufp)++;
    ++scanned_so_far;
  }
  *out = '\0';
  return 0;
}

/** Locale-independent, minimal, no-surprises scanf variant, accepting only a
 * restricted pattern format.  For more info on what it supports, see
 * tor_sscanf() documentation.  */
static inline int
hidden_tor_vsscanf(const char *buf, const char *pattern, va_list ap)
{
  int n_matched = 0;

  while (*pattern) {
    if (*pattern != '%') {
      if (*buf == *pattern) {
        ++buf;
        ++pattern;
        continue;
      } else {
        return n_matched;
      }
    } else {
      int width = -1;
      int longmod = 0;
      ++pattern;
      if (HIDDEN_TOR_ISDIGIT(*pattern)) {
        width = hidden_digit_to_num(*pattern++);
        while (HIDDEN_TOR_ISDIGIT(*pattern)) {
          width *= 10;
          width += hidden_digit_to_num(*pattern++);
          if (width > HIDDEN_MAX_SCANF_WIDTH)
            return -1;
        }
        if (!width) /* No zero-width things. */
          return -1;
      }
      if (*pattern == 'l') {
        longmod = 1;
        ++pattern;
      }
      if (*pattern == 'u' || *pattern == 'x') {
        unsigned long u;
        const int base = (*pattern == 'u') ? 10 : 16;
        if (!*buf)
          return n_matched;
        if (hidden_scan_unsigned(&buf, &u, width, base)<0)
          return n_matched;
        if (longmod) {
          unsigned long *out = va_arg(ap, unsigned long *);
          *out = u;
        } else {
          unsigned *out = va_arg(ap, unsigned *);
          if (u > UINT_MAX)
            return n_matched;
          *out = (unsigned) u;
        }
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'f') {
        double *d = va_arg(ap, double *);
        if (!longmod)
          return -1; /* float not supported */
        if (!*buf)
          return n_matched;
        if (hidden_scan_double(&buf, d, width)<0)
          return n_matched;
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'd') {
        long lng=0;
        if (hidden_scan_signed(&buf, &lng, width)<0)
          return n_matched;
        if (longmod) {
          long *out = va_arg(ap, long *);
          *out = lng;
        } else {
          int *out = va_arg(ap, int *);
          if (lng < INT_MIN || lng > INT_MAX)
            return n_matched;
          *out = (int)lng;
        }
        ++pattern;
        ++n_matched;
      } else if (*pattern == 's') {
        char *s = va_arg(ap, char *);
        if (longmod)
          return -1;
        if (width < 0)
          return -1;
        if (hidden_scan_string(&buf, s, width)<0)
          return n_matched;
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'c') {
        char *ch = va_arg(ap, char *);
        if (longmod)
          return -1;
        if (width != -1)
          return -1;
        if (!*buf)
          return n_matched;
        *ch = *buf++;
        ++pattern;
        ++n_matched;
      } else if (*pattern == '%') {
        if (*buf != '%')
          return n_matched;
        if (longmod)
          return -1;
        ++buf;
        ++pattern;
      } else {
        return -1; /* Unrecognized pattern component. */
      }
    }
  }

  return n_matched;
}

/** Minimal sscanf replacement: parse <b>buf</b> according to <b>pattern</b>
 * and store the results in the corresponding argument fields.  Differs from
 * sscanf in that:
 * <ul><li>It only handles %u, %lu, %x, %lx, %[NUM]s, %d, %ld, %lf, and %c.
 *     <li>It only handles decimal inputs for %lf. (12.3, not 1.23e1)
 *     <li>It does not handle arbitrarily long widths.
 *     <li>Numbers do not consume any space characters.
 *     <li>It is locale-independent.
 *     <li>%u and %x do not consume any space.
 *     <li>It returns -1 on malformed patterns.</ul>
 *
 * (As with other locale-independent functions, we need this to parse data that
 * is in ASCII without worrying that the C library's locale-handling will make
 * miscellaneous characters look like numbers, spaces, and so on.)
 */
static inline int
hidden_tor_sscanf(const char *buf, const char *pattern, ...)
{
  sgx_wrapper_printf("Fix me: Call untested hidden_tor_sscanf: %s", buf);
  int r;
  va_list ap;
  va_start(ap, pattern);
  r = hidden_tor_vsscanf(buf, pattern, ap);
  va_end(ap);
  sgx_wrapper_printf("return: %d", r);
  return r;
}

#define sscanf(A, B, ...) hidden_tor_sscanf(A, B, ##__VA_ARGS__)

#endif

#define stdin SGX_STDIN
#define stdout SGX_STDOUT
#define stderr SGX_STDERR

#endif
