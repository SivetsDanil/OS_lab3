#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "common.h"

static char shm_name[64], empty_name[64], full_name[64];
static shm_t* shm = MAP_FAILED;
static sem_t* empty = SEM_FAILED;
static sem_t* full = SEM_FAILED;

static void cleanup(void) {
    if (shm != MAP_FAILED) munmap(shm, sizeof(shm_t));
    if (empty != SEM_FAILED) sem_close(empty);
    if (full != SEM_FAILED) sem_close(full);
    shm_unlink(shm_name);
    sem_unlink(empty_name);
    sem_unlink(full_name);
}

static int read_line(int fd, char* buf, int cap) {
    int len = 0;
    char c;
    while (len < cap - 1 && read(fd, &c, 1) == 1) {
        if (c == '\n') break;
        buf[len++] = c;
    }
    buf[len] = '\0';
    return len;
}

static int wait_empty(pid_t pid, int* status) {
    for (;;) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        if (sem_timedwait(empty, &ts) == 0) return 0;
        if (errno == EINTR) continue;
        if (errno != ETIMEDOUT) return -1;
        if (waitpid(pid, status, WNOHANG) == pid) return 1;
    }
}

int main() {
    char fname[256];
    if (read_line(0, fname, sizeof(fname)) <= 0) {
        put_err("parent: expected filename on first line\n");
        return 1;
    }
    long id = getpid();
    make_name(shm_name, "/lab3_shm_", id);
    make_name(empty_name, "/lab3_empty_", id);
    make_name(full_name, "/lab3_full_", id);
    int fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) {
        put_err("parent: shm_open failed\n");
        return 1;
    }
    if (ftruncate(fd, sizeof(shm_t)) == -1) {
        put_err("parent: ftruncate failed\n");
        close(fd);
        cleanup();
        return 1;
    }
    shm = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) {
        put_err("parent: mmap failed\n");
        cleanup();
        return 1;
    }
    empty = sem_open(empty_name, O_CREAT | O_EXCL, 0600, 1);
    full = sem_open(full_name, O_CREAT | O_EXCL, 0600, 0);
    if (empty == SEM_FAILED || full == SEM_FAILED) {
        put_err("parent: sem_open failed\n");
        cleanup();
        return 1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        put_err("parent: fork failed\n");
        cleanup();
        return 1;
    }
    if (pid == 0) {
        execl("./child", "child", fname, shm_name, empty_name, full_name, (char *)NULL);
        put_err("parent: exec ./child failed\n");
        _exit(1);
    }
    char buf[BUF_SIZE];
    int status = 0, exited = 0;
    for (;;) {
        ssize_t n = read(0, buf, sizeof(buf));
        if (n < 0) {
            put_err("parent: read failed\n");
            n = 0;
        }
        int r = wait_empty(pid, &status);
        if (r == 1) {
            exited = 1;
            break;
        }
        if (r == -1) {
            put_err("parent: sem_wait failed\n");
            break;
        }
        for (ssize_t i = 0; i < n; i++) shm->data[i] = buf[i];
        shm->len = (int)n;
        sem_post(full);
        if (n == 0) break;
    }
    if (!exited && waitpid(pid, &status, 0) == -1) {
        put_err("parent: waitpid failed\n");
        cleanup();
        return 1;
    }
    cleanup();
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0) {
            put_err("parent: child reported error\n");
            return code;
        }
        return 0;
    }
    put_err("parent: child terminated abnormally\n");
    return 1;
}
