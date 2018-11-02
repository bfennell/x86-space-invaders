/*
  Copyright (c) 2018 Brendan Fennell <bfennell@skynet.ie>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#include "x86.h"

#include "stdio.h"

#define COM1 0x3f8

#define PRINT_BUF_SIZE 256

static char* lookup = "0123456789abcdef";

static int serial_tx_is_empty (void)
{
    return (inport8(COM1 + 5) & 0x20);
}

int putchar (int c)
{
    while(!serial_tx_is_empty());
    outport8(COM1, c);
    return 0;
}

static inline uint8_t get_digit (unsigned long val, int i)
{
    unsigned long div = 1;
    while (i--) {
        div *= 10;
    }
    unsigned long v = val/div;
    return (v % 10);
}

static inline uint8_t get_nibble (const unsigned long val, const int i)
{
    return (((val) >> (i*4)) & 0xf);
}

static inline uint8_t get_bit (const unsigned long val, const int i)
{
    return (((val) >> i) & 0x1);
}

static int put (const char* str)
{
    int count = 0;
    while (*str) {
        putchar (*str);
        str++;
        count++;
    }
    return count;
}

int puts (const char* str)
{
    int count = put (str);

    putchar('\r');
    putchar('\n');

    return count;
}

static inline int isdigit (int c)
{
    return ((c >= 0x30) && (c <= 0x39));
}

static inline int dtoi (const char c)
{
    return (c - 0x30);
}

typedef struct {
    bool pad_zero;
    bool pad_space;
    unsigned pad_len;
} flags_t;

static inline void reset_flags (flags_t* flags)
{
    flags->pad_zero = false;
    flags->pad_space = false;
    flags->pad_len = 0;
}

int print_dec (char* str, size_t size, flags_t flags, unsigned long val)
{
    const unsigned max_digits = 20; /* 1.8x10^19 */
    char buf[max_digits+1];

    unsigned nr_digits = 0;
    unsigned long tmp = val;
    while (tmp) {
        tmp /= 10;
        nr_digits++;
    }
    int nr_pad = (flags.pad_len >= nr_digits) ? (flags.pad_len - nr_digits) : 0;
    char pad_val = (flags.pad_zero) ? '0' : ' ';

    nr_pad = ((nr_pad + nr_digits) > max_digits) ? (((nr_pad + nr_digits) > max_digits)) : nr_pad;

    unsigned j = 0;
    while (nr_pad--) {
        buf[j++] = pad_val;
    }

    if (nr_digits == 0) {
        nr_digits = 1;
    }

    for (int i = nr_digits - 1; i >= 0; --i) {
        buf[j++] = lookup[get_digit (val, i)];
    }
    buf[j] = '\0';

    j = 0;
    while (buf[j] && (j < size)) {
        str[j] = buf[j];
        j++;
    }
    return j;
}

int print_bin (char* str, size_t size, flags_t flags, unsigned long val)
{
    const unsigned max_bits = sizeof(val)*CHAR_BIT;
    char buf[max_bits+1];

    unsigned nr_bits = 0;
    unsigned long tmp = val;
    while (tmp) {
        tmp >>= 1;
        nr_bits++;
    }
    int nr_pad = (flags.pad_len >= nr_bits) ? (flags.pad_len - nr_bits) : 0;
    char pad_val = (flags.pad_zero) ? '0' : ' ';

    nr_pad = ((nr_pad + nr_bits) > max_bits) ? (((nr_pad + nr_bits) > max_bits)) : nr_pad;

    unsigned j = 0;
    while (nr_pad--) {
        buf[j++] = pad_val;
    }

    if (nr_bits == 0) {
        nr_bits = 1;
    }

    for (int i = nr_bits - 1; i >= 0; --i) {
        buf[j++] = lookup[get_bit (val, i)];
    }
    buf[j] = '\0';

    j = 0;
    while (buf[j] && (j < size)) {
        str[j] = buf[j];
        j++;
    }
    return j;
}

int print_hex (char* str, size_t size, flags_t flags, unsigned long val)
{
    const unsigned max_nibbles = sizeof(val)*2;
    char buf[max_nibbles+1];

    unsigned nr_nibbles = 0;
    unsigned long tmp = val;
    while (tmp) {
        tmp >>= 4;
        nr_nibbles++;
    }
    int nr_pad = (flags.pad_len >= nr_nibbles) ? (flags.pad_len - nr_nibbles) : 0;
    char pad_val = (flags.pad_zero) ? '0' : ' ';

    nr_pad = ((nr_pad + nr_nibbles) > max_nibbles) ? (((nr_pad + nr_nibbles) > max_nibbles)) : nr_pad;

    unsigned j = 0;
    while (nr_pad--) {
        buf[j++] = pad_val;
    }

    if (nr_nibbles == 0) {
        nr_nibbles = 1;
    }

    for (int i = nr_nibbles - 1; i >= 0; --i) {
        buf[j++] = lookup[get_nibble (val, i)];
    }
    buf[j] = '\0';

    j = 0;
    while (buf[j] && (j < size)) {
        str[j] = buf[j];
        j++;
    }
    return j;
}

static size_t strlen(const char *s)
{
    int count = 0;
    while (*s++) {
        count++;
    }
    return count;
}

static int print_str (char* str, size_t size, flags_t flags, char* s)
{
    unsigned len = strlen(s);

    int nr_pad = (flags.pad_len >= len) ? (flags.pad_len - len) : 0;
    char pad_val = (flags.pad_zero) ? '0' : ' ';

    unsigned i = 0;
    while (nr_pad-- && (i < size)) {
        str[i++] = pad_val;
    }
    while (*s && (i < size)) {
        str[i++] = *s++;
    }
    return i;
}

int vsnprintf (char *str, size_t size, const char *format, va_list ap)
{
    unsigned i = 0;
    const char* pos = format;
    flags_t flags;

    reset_flags (&flags);

    while (*format && (i < (size-1))) {
        /* maybe a supported format specifier, remember the location in case it isn't */
        pos = format;
        if (*format == '%') {
            if (*++format == '%') {
                /* %% -> % */
                str[i++] = *format++;
                pos = format;
                continue;
            } else {
                reset_flags (&flags);

                if (!*format) {
                    break;
                }
                if (isdigit(*format) && *format == '0') {
                    flags.pad_zero = 1;
                    format++;
                } else if (isdigit(*format)) {
                    flags.pad_space = 1;
                }

                if (!*format) {
                    break;
                }

                int pad_len = 0;
                while (*format && isdigit(*format)) {
                    pad_len *= 10;
                    pad_len += dtoi(*format++);
                }
                flags.pad_len = pad_len;

                if (!*format) {
                    break;
                }

                switch (*format) {
                case 'b': {
                    ++format;
                    i += print_bin (&str[i], (size - 1 - i), flags, va_arg(ap,unsigned long));
                    break;
                }
                case 'd':
                case 'u': {
                    ++format;
                    i += print_dec (&str[i], (size - 1 - i), flags, va_arg(ap,unsigned long));
                    break;
                }
                case 'p':
                case 'x': {
                    ++format;
                    i += print_hex (&str[i], (size - 1 - i), flags, va_arg(ap,unsigned long));
                    break;
                }
                case 's': {
                    ++format;
                    i += print_str (&str[i], (size - 1 - i), flags, (char*)va_arg(ap,uintptr_t));
                    break;
                }
                default: {
                    /* unkown specifier */
                    while (pos != format && (i < (size-1))) {
                        str[i++] = *pos++;
                    }
                    if (i < (size-1)) {
                        str[i++] = *format++;
                    }
                    break;
                }
                }
                pos = format;
            }
        } else {
            str[i++] = *format++;
            pos = format;
        }
    }

    if (pos != format) {
        while (pos != format && (i < size-1)) {
            str[i++] = *pos++;
        }
    }
    str[i] = '\0';

    return i;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int count;

    va_start(args,format);
    count = vsnprintf (str, size, format, args);
    va_end(args);

    return count;
}

int printf (const char *format, ...)
{
    char buf[PRINT_BUF_SIZE];
    va_list args;
    int count;

    va_start(args,format);
    count = vsnprintf (buf, PRINT_BUF_SIZE, format, args);
    va_end(args);

    put (buf);

    return count;
}
