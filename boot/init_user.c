#include <stdint.h>
#include <stddef.h>

#define SYS_EXEC      2
#define SYS_EXIT      3
#define SYS_OPEN      10
#define SYS_CLOSE     11
#define SYS_READ      12
#define SYS_WRITE     13
#define SYS_LSEEK     14
#define SYS_STAT      15
#define SYS_UNLINK    16
#define SYS_LIST      17
#define SYS_MKDIR     18
#define SYS_GETPID    5
#define SYS_TORUS_ID  9
#define SYS_YIELD     34

#define INIT_TORUS0_COMPUTE_ITERS 1000000ULL
#define INIT_TORUS1_COMPUTE_ITERS 2000000ULL
#define INIT_MEMSTRESS_PASSES 128U
#define INIT_FILE_BENCH_FILES 32U
#define INIT_BLOCK_BENCH_BLOCKS 32U
#define INIT_NET_BENCH_ITERS 64U

#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_RDWR    0x0002
#define O_CREAT   0x0040
#define O_TRUNC   0x0200

struct rse_stat {
    uint64_t size;
    uint32_t mode;
    uint32_t type;
};

static inline int64_t rse_syscall6(uint64_t num, uint64_t a1, uint64_t a2,
                                   uint64_t a3, uint64_t a4, uint64_t a5,
                                   uint64_t a6) {
    int64_t ret;
    register uint64_t r10 __asm__("r10") = a4;
    register uint64_t r8 __asm__("r8") = a5;
    register uint64_t r9 __asm__("r9") = a6;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory",
          "xmm0", "xmm1", "xmm2", "xmm3",
          "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11",
          "xmm12", "xmm13", "xmm14", "xmm15");
    return ret;
}

__attribute__((unused)) static inline int64_t sys_exec(const char* path) {
    return rse_syscall6(SYS_EXEC, (uint64_t)path, 0, 0, 0, 0, 0);
}

static inline int64_t sys_exit(int64_t code) {
    return rse_syscall6(SYS_EXIT, (uint64_t)code, 0, 0, 0, 0, 0);
}

static inline int32_t sys_open(const char* path, uint32_t flags, uint32_t mode) {
    return (int32_t)rse_syscall6(SYS_OPEN, (uint64_t)path, flags, mode, 0, 0, 0);
}

static inline int32_t sys_close(int32_t fd) {
    return (int32_t)rse_syscall6(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
}

static inline int64_t sys_read(int32_t fd, void* buf, uint32_t len) {
    return rse_syscall6(SYS_READ, (uint64_t)fd, (uint64_t)buf, len, 0, 0, 0);
}

static inline int64_t sys_write(int32_t fd, const void* buf, uint32_t len) {
    return rse_syscall6(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, len, 0, 0, 0);
}

static inline int64_t sys_lseek(int32_t fd, int64_t offset, int32_t whence) {
    return rse_syscall6(SYS_LSEEK, (uint64_t)fd, (uint64_t)offset,
                        (uint64_t)whence, 0, 0, 0);
}

static inline int64_t sys_unlink(const char* path) {
    return rse_syscall6(SYS_UNLINK, (uint64_t)path, 0, 0, 0, 0, 0);
}

static inline int64_t sys_list(const char* path, char* buf, uint32_t len) {
    return rse_syscall6(SYS_LIST, (uint64_t)path, (uint64_t)buf, len, 0, 0, 0);
}

static inline int64_t sys_stat(const char* path, struct rse_stat* out) {
    return rse_syscall6(SYS_STAT, (uint64_t)path, (uint64_t)out, 0, 0, 0, 0);
}

__attribute__((unused)) static inline int64_t sys_mkdir(const char* path, uint32_t mode) {
    return rse_syscall6(SYS_MKDIR, (uint64_t)path, mode, 0, 0, 0, 0);
}

static inline int64_t sys_getpid(void) {
    return rse_syscall6(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

static inline int64_t sys_torus_id(void) {
    return rse_syscall6(SYS_TORUS_ID, 0, 0, 0, 0, 0, 0);
}

static inline int64_t sys_yield(void) {
    return rse_syscall6(SYS_YIELD, 0, 0, 0, 0, 0, 0);
}

static inline uint64_t rdtsc(void) {
    uint32_t lo;
    uint32_t hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint32_t cstr_len(const char* s) {
    uint32_t len = 0;
    if (!s) {
        return 0;
    }
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

static void write_str(const char* msg) {
    if (!msg) {
        return;
    }
    sys_write(1, msg, cstr_len(msg));
}

static void write_u64(uint64_t value) {
    char buf[32];
    uint32_t idx = 0;
    if (value == 0) {
        buf[idx++] = '0';
    } else {
        while (value && idx < sizeof(buf)) {
            buf[idx++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    for (uint32_t i = 0; i < idx / 2; ++i) {
        char t = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = t;
    }
    sys_write(1, buf, idx);
}

static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static void format_name(char* buf, uint32_t index) {
    buf[0] = 'f';
    buf[1] = 'i';
    buf[2] = 'l';
    buf[3] = 'e';
    buf[4] = (char)('0' + ((index / 1000) % 10));
    buf[5] = (char)('0' + ((index / 100) % 10));
    buf[6] = (char)('0' + ((index / 10) % 10));
    buf[7] = (char)('0' + (index % 10));
    buf[8] = '\0';
}

static void format_path(char* buf, uint32_t index, int persist) {
    if (persist) {
        const char* prefix = "/persist/";
        uint32_t i = 0;
        for (; prefix[i] != '\0'; ++i) {
            buf[i] = prefix[i];
        }
        format_name(buf + i, index);
        return;
    }
    format_name(buf, index);
}

static uint8_t io_buf[4096];
static uint8_t blk_buf[512];
static uint8_t mem_a[16384];
static uint8_t mem_b[16384];

static void log_stat(const char* path) {
    struct rse_stat st;
    int64_t rc = sys_stat(path, &st);
    if (rc < 0) {
        write_str("stat failed: ");
        write_str(path);
        write_str("\n");
        return;
    }
    write_str("stat ");
    write_str(path);
    write_str(" size=");
    write_u64(st.size);
    write_str(" type=");
    switch (st.type) {
        case 1: write_str("file"); break;
        case 2: write_str("dir"); break;
        case 3: write_str("device"); break;
        default: write_str("unknown"); break;
    }
    write_str("\n");
}

static void log_list(const char* path) {
    char buf[512];
    int64_t got = sys_list(path, buf, sizeof(buf));
    if (got <= 0) {
        write_str("ls: empty\n");
        return;
    }
    sys_write(1, buf, (uint32_t)got);
}

static void init_main(void) {
    write_str("[init] ring3 start\n");
    int64_t torus = sys_torus_id();
    if (torus < 0) {
        torus = 0;
    }
    write_str("[init] torus=");
    write_u64((uint64_t)torus);
    write_str(" pid=");
    write_u64((uint64_t)sys_getpid());
    write_str("\n");

    uint64_t seed = 0xfeedbeefcafebabeULL;
    uint64_t iters = (torus == 1) ? INIT_TORUS1_COMPUTE_ITERS : INIT_TORUS0_COMPUTE_ITERS;
    uint64_t acc = 0;
    uint64_t start = rdtsc();
    for (uint64_t i = 0; i < iters; ++i) {
        acc ^= xorshift64(&seed) + (i << 1);
    }
    uint64_t end = rdtsc();
    write_str("[init] compute ops=");
    write_u64(iters);
    write_str(" cycles=");
    write_u64(end - start);
    write_str(" checksum=");
    write_u64(acc);
    write_str("\n");
    for (uint32_t i = 0; i < 2; ++i) {
        sys_yield();
    }

    if (torus == 1) {
        for (uint32_t i = 0; i < sizeof(mem_a); ++i) {
            mem_a[i] = (uint8_t)(i ^ 0x5a);
        }
        const uint64_t passes = INIT_MEMSTRESS_PASSES;
        uint64_t checksum = 0;
        uint64_t mem_start = rdtsc();
        for (uint64_t p = 0; p < passes; ++p) {
            for (uint32_t i = 0; i < sizeof(mem_a); ++i) {
                mem_b[i] = (uint8_t)(mem_a[i] + (uint8_t)p);
                checksum += mem_b[i];
            }
        }
        uint64_t mem_end = rdtsc();
        write_str("[init] memstress bytes=");
        write_u64((uint64_t)sizeof(mem_a) * passes);
        write_str(" cycles=");
        write_u64(mem_end - mem_start);
        write_str(" checksum=");
        write_u64(checksum);
        write_str("\n");
        for (uint32_t i = 0; i < 2; ++i) {
            sys_yield();
        }
    }

    for (uint32_t i = 0; i < sizeof(io_buf); ++i) {
        io_buf[i] = (uint8_t)(i ^ 0x5a);
    }

    if (torus == 0) {
        int persist_mode = 0;
        int probe = sys_open("/persist/.probe", O_CREAT | O_WRONLY, 0644);
        if (probe >= 0) {
            sys_close(probe);
            sys_unlink("/persist/.probe");
            persist_mode = 1;
        }
        write_str(persist_mode ? "[init] using /persist\n" : "[init] using memfs\n");

        uint64_t ops = 0;
        uint64_t bytes = 0;
        uint64_t io_start = rdtsc();
        const uint32_t file_count = INIT_FILE_BENCH_FILES;
        char name[32];
        for (uint32_t i = 0; i < file_count; ++i) {
            format_path(name, i, persist_mode);
            int fd = sys_open(name, O_CREAT | O_TRUNC | O_RDWR, 0644);
            if (fd < 0) {
                continue;
            }
            ops++;
            int64_t wrote = sys_write(fd, io_buf, sizeof(io_buf));
            if (wrote > 0) {
                bytes += (uint64_t)wrote;
            }
            ops++;
            int64_t read = sys_read(fd, io_buf, sizeof(io_buf));
            if (read > 0) {
                bytes += (uint64_t)read;
            }
            ops++;
            sys_close(fd);
            ops++;
        }
        for (uint32_t i = 0; i < file_count; ++i) {
            format_path(name, i, persist_mode);
            sys_unlink(name);
            ops++;
        }
        uint64_t io_end = rdtsc();
        write_str("[init] file ops=");
        write_u64(ops);
        write_str(" bytes=");
        write_u64(bytes);
        write_str(" cycles=");
        write_u64(io_end - io_start);
        write_str("\n");

        write_str("[init] ls /\n");
        log_list("/");
        write_str("\n");
        write_str("[init] ls /persist\n");
        log_list("/persist");
        write_str("\n");
        log_stat("/dev/blk0");
    }

    if (torus == 0) {
        int loop_fd = sys_open("/dev/loopback", O_RDWR, 0);
        if (loop_fd >= 0) {
            const char* msg = "loopback-test";
            sys_write(loop_fd, msg, cstr_len(msg));
            char buf[32];
            int64_t got = sys_read(loop_fd, buf, sizeof(buf));
            sys_close(loop_fd);
            write_str("[init] loopback read=");
            write_u64((uint64_t)(got > 0 ? got : 0));
            write_str("\n");
        } else {
            write_str("[init] /dev/loopback unavailable\n");
        }
    }

    if (torus == 0) {
        int fd = sys_open("/dev/blk0", O_RDWR, 0);
        if (fd >= 0) {
            const uint32_t blk_size = sizeof(blk_buf);
            const uint32_t blocks = INIT_BLOCK_BENCH_BLOCKS;
            const uint64_t start_lba = 2048;
            uint64_t mismatches = 0;
            uint64_t bytes = 0;
            uint64_t ops = 0;
            uint64_t blk_start = rdtsc();
            int64_t seek_start = sys_lseek(fd, (int64_t)(start_lba * blk_size), 0);
            if (seek_start < 0) {
                write_str("[init] /dev/blk0 seek start failed\n");
            }
            for (uint32_t i = 0; i < blocks; ++i) {
                for (uint32_t j = 0; j < blk_size; ++j) {
                    blk_buf[j] = (uint8_t)(j ^ (i & 0xFFu) ^ 0xA5u);
                }
                int64_t w = sys_write(fd, blk_buf, blk_size);
                if (w == (int64_t)blk_size) {
                    bytes += (uint64_t)w;
                    ops++;
                }
            }
            int64_t seek_read = sys_lseek(fd, (int64_t)(start_lba * blk_size), 0);
            if (seek_read < 0) {
                write_str("[init] /dev/blk0 seek read failed\n");
            }
            for (uint32_t i = 0; i < blocks; ++i) {
                int64_t r = sys_read(fd, blk_buf, blk_size);
                if (r == (int64_t)blk_size) {
                    bytes += (uint64_t)r;
                    ops++;
                    for (uint32_t j = 0; j < blk_size; ++j) {
                        uint8_t expect = (uint8_t)(j ^ (i & 0xFFu) ^ 0xA5u);
                        if (blk_buf[j] != expect) {
                            mismatches++;
                            break;
                        }
                    }
                }
            }
            uint64_t blk_end = rdtsc();
            sys_close(fd);
            write_str("[init] /dev/blk0 ops=");
            write_u64(ops);
            write_str(" bytes=");
            write_u64(bytes);
            write_str(" mismatches=");
            write_u64(mismatches);
            write_str(" cycles=");
            write_u64(blk_end - blk_start);
            write_str("\n");
        } else {
            write_str("[init] /dev/blk0 unavailable\n");
        }
    }

    if (torus == 2) {
        int net_fd = sys_open("/dev/net0", O_RDWR, 0);
        if (net_fd >= 0) {
            uint8_t pkt[64];
            for (uint32_t i = 0; i < sizeof(pkt); ++i) {
                pkt[i] = (uint8_t)(i ^ 0x3c);
            }
            uint64_t wrote = 0;
            uint64_t read = 0;
            uint64_t net_start = rdtsc();
            for (uint32_t i = 0; i < INIT_NET_BENCH_ITERS; ++i) {
                int64_t w = sys_write(net_fd, pkt, sizeof(pkt));
                if (w > 0) {
                    wrote += (uint64_t)w;
                }
                int64_t r = sys_read(net_fd, pkt, sizeof(pkt));
                if (r > 0) {
                    read += (uint64_t)r;
                }
            }
            uint64_t net_end = rdtsc();
            sys_close(net_fd);
            write_str("[init] net0 wrote=");
            write_u64(wrote);
            write_str(" read=");
            write_u64(read);
            write_str(" cycles=");
            write_u64(net_end - net_start);
            write_str("\n");
        } else {
            write_str("[init] /dev/net0 unavailable\n");
        }
    }
    for (uint32_t i = 0; i < 2; ++i) {
        sys_yield();
    }
}

__attribute__((noreturn)) void _start(void) {
    init_main();
    sys_exit(0);
    for (;;) {
        __asm__ volatile("hlt");
    }
}
