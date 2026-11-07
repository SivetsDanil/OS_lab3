#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include "common.h"

static void write_sum(int fd, float sum, int decimals) {
    if (decimals > 7) {
        decimals = 7;
    }
    char buf[64]; int len = 0;
    if (sum < 0) { buf[len++] = '-'; sum = -sum; }
    long whole = (long)sum;
    long scale = 1;
    for (int i = 0; i < decimals; i++) scale *= 10;
    long frac = (long)((float)(sum - (float)whole) * (float)scale + 0.5);
    if (frac == scale) { whole += 1; frac = 0; }
    char tmp[48]; int t = 0;
    if (whole == 0) tmp[t++] = '0';
    while (whole > 0) { tmp[t++] = (char)('0' + whole % 10); whole /= 10; }
    for (int i = t - 1; i >= 0; i--) buf[len++] = tmp[i];
    if (decimals > 0) {
        buf[len++] = '.';
        t = 0;
        for (int i = 0; i < decimals; i++) { tmp[t++] = (char)('0' + frac % 10); frac /= 10; }
        for (int i = t - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len++] = '\n';
    if (write(fd, buf, len) != len) put_err("child: write to file failed\n");
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        put_err("usage: child filename shm_name empty_name full_name\n");
        return 1;
    }
    int out = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        put_err("child: cannot open file\n");
        return 1;
    }
    int fd = shm_open(argv[2], O_RDWR, 0);
    if (fd == -1) {
        put_err("child: shm_open failed\n");
        return 1;
    }
    shm_t* shm = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) {
        put_err("child: mmap failed\n");
        return 1;
    }
    sem_t* empty = sem_open(argv[3], 0);
    sem_t* full = sem_open(argv[4], 0);
    if (empty == SEM_FAILED || full == SEM_FAILED) {
        put_err("child: sem_open failed\n");
        return 1;
    }
    float sum = 0.0f, total = 0.0f;
    int cnt = 0, rc = 0;
    char tok[64];
    int tlen = 0;
    for (;;) {
        if (sem_wait(full) == -1) {
            if (errno == EINTR) continue;
            put_err("child: sem_wait failed\n");
            rc = 1;
            break;
        }
        int len = shm->len;
        int n = len > 0 ? len : 1;
        for (int i = 0; i < n; i++) {
            char c = len > 0 ? shm->data[i] : '\n';
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
                if (tlen > 0) {
                    tok[tlen] = '\0';
                    float v = (float)atof(tok);
                    sum += v;
                    total += v;
                    cnt++;
                    tlen = 0;
                }
                if (c == '\n' && cnt > 0) {
                    write_sum(out, sum, 2);
                    sum = 0.0f;
                    cnt = 0;
                }
            } else if (tlen < 63) {
                tok[tlen++] = c;
            }
        }
        sem_post(empty);
        if (len == 0) break;
    }
    if (rc == 0) write_sum(out, total, 2);
    munmap(shm, sizeof(shm_t));
    sem_close(empty);
    sem_close(full);
    close(out);
    return rc;
}
