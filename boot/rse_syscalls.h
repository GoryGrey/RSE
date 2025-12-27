#ifndef RSE_SYSCALLS_H
#define RSE_SYSCALLS_H

#include <stdint.h>

struct rse_syscalls {
    void (*log)(const char *msg);
    void (*log_u64)(uint64_t value);
    uint64_t (*rdtsc)(void);
    uint32_t (*get_torus_id)(void);
    uint32_t (*pipe_push)(uint32_t queue_id, const uint8_t *buf, uint32_t len);
    uint32_t (*pipe_pop)(uint32_t queue_id, uint8_t *buf, uint32_t max_len);
    void (*report_block)(uint64_t bytes, uint64_t cycles);
    void (*report_net)(uint64_t bytes, uint64_t cycles);
    int (*open)(const char *name, uint32_t flags);
    int (*close)(int fd);
    int (*write)(int fd, const uint8_t *buf, uint32_t len);
    int (*read)(int fd, uint8_t *buf, uint32_t len);
    int (*unlink)(const char *name);
    int (*lseek)(int fd, int64_t offset, int whence);
    int (*list)(const char *path, char *buf, uint32_t len);
    int (*ps)(char *buf, uint32_t len);
};

#endif
