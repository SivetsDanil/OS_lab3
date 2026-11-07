#ifndef COMMON_H
#define COMMON_H

#include <unistd.h>

#define BUF_SIZE 4096

typedef struct {
    int len;
    char data[BUF_SIZE];
} shm_t;

static inline void put_err(const char* s) {
    int len = 0;
    while (s[len]) len++;
    if (write(2, s, len) < 0) return;
}

static inline void make_name(char* out, const char* prefix, long id) {
    int len = 0;
    while (prefix[len]) { out[len] = prefix[len]; len++; }
    char tmp[24]; int t = 0;
    do { tmp[t++] = (char)('0' + id % 10); id /= 10; } while (id > 0);
    while (t > 0) out[len++] = tmp[--t];
    out[len] = '\0';
}

#endif
