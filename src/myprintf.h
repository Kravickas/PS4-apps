#pragma once

// Minimal printf implementation — no libc dependency
static void _put_char(char* buf, int* pos, int max, char c) {
    if (*pos < max - 1) buf[*pos] = c;
    (*pos)++;
}

static void _put_str(char* buf, int* pos, int max, const char* s) {
    while (*s) _put_char(buf, pos, max, *s++);
}

static void _put_int(char* buf, int* pos, int max, int n) {
    if (n < 0) { _put_char(buf, pos, max, '-'); n = -n; }
    if (n == 0) { _put_char(buf, pos, max, '0'); return; }
    char tmp[16]; int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) _put_char(buf, pos, max, tmp[--i]);
}

static void _put_uint(char* buf, int* pos, int max, unsigned int n) {
    if (n == 0) { _put_char(buf, pos, max, '0'); return; }
    char tmp[16]; int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) _put_char(buf, pos, max, tmp[--i]);
}

static void _put_hex(char* buf, int* pos, int max, unsigned long long n) {
    _put_str(buf, pos, max, "0x");
    if (n == 0) { _put_char(buf, pos, max, '0'); return; }
    char tmp[17]; int i = 0;
    while (n > 0) { int d = n & 0xF; tmp[i++] = d < 10 ? '0'+d : 'A'+d-10; n >>= 4; }
    while (i > 0) _put_char(buf, pos, max, tmp[--i]);
}

// Minimal vsnprintf: supports %d, %u, %s, %x, %X, %llu, %llX, %p, %%
static int my_vsnprintf(char* buf, int max, const char* fmt, __builtin_va_list ap) {
    int pos = 0;
    while (*fmt) {
        if (*fmt != '%') { _put_char(buf, &pos, max, *fmt++); continue; }
        fmt++;
        if (*fmt == '%') { _put_char(buf, &pos, max, '%'); fmt++; continue; }
        // Skip width/padding
        while (*fmt >= '0' && *fmt <= '9') fmt++;
        bool is_long_long = false;
        if (*fmt == 'l') { fmt++; if (*fmt == 'l') { is_long_long = true; fmt++; } }
        switch (*fmt) {
            case 'd': _put_int(buf, &pos, max, __builtin_va_arg(ap, int)); break;
            case 'u':
                if (is_long_long) _put_uint(buf, &pos, max, (unsigned int)__builtin_va_arg(ap, unsigned long long));
                else _put_uint(buf, &pos, max, __builtin_va_arg(ap, unsigned int));
                break;
            case 'x': case 'X':
                if (is_long_long) _put_hex(buf, &pos, max, __builtin_va_arg(ap, unsigned long long));
                else _put_hex(buf, &pos, max, (unsigned long long)__builtin_va_arg(ap, unsigned int));
                break;
            case 'p': _put_hex(buf, &pos, max, (unsigned long long)(unsigned long)__builtin_va_arg(ap, void*)); break;
            case 's': _put_str(buf, &pos, max, __builtin_va_arg(ap, const char*)); break;
            case 'c': _put_char(buf, &pos, max, (char)__builtin_va_arg(ap, int)); break;
            default: _put_char(buf, &pos, max, '%'); _put_char(buf, &pos, max, *fmt); break;
        }
        fmt++;
    }
    if (pos < max) buf[pos] = '\0'; else if (max > 0) buf[max-1] = '\0';
    return pos;
}
