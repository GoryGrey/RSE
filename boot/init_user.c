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
#define SYS_SOCKET    50
#define SYS_BIND      51
#define SYS_LISTEN    52
#define SYS_ACCEPT    53
#define SYS_CONNECT   54

#define INIT_TORUS0_COMPUTE_ITERS 1000000ULL
#define INIT_TORUS1_COMPUTE_ITERS 2000000ULL
#define INIT_MEMSTRESS_PASSES 128U
#define INIT_FILE_BENCH_FILES 32U
#define INIT_BLOCK_BENCH_BLOCKS 32U
#define INIT_NET_BENCH_ITERS 64U
#define INIT_COMPUTE_YIELD_STRIDE 65536ULL
#define INIT_MEMSTRESS_YIELD_PASSES 8U
#define INIT_FILEIO_YIELD_STRIDE 4U
#define INIT_BLOCK_YIELD_STRIDE 4U
#define INIT_NET_YIELD_STRIDE 8U

#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_RDWR    0x0002
#define O_CREAT   0x0040
#define O_TRUNC   0x0200

#define EAGAIN        11
#define EISCONN       106
#define ECONNREFUSED  111

#ifndef RSE_NET_RAW
#define RSE_NET_RAW 0
#endif
#ifndef RSE_RAWTCP_ONLY
#define RSE_RAWTCP_ONLY 0
#endif

#define RSE_AF_INET        2
#define RSE_SOCK_STREAM    1
#define RSE_PROTO_TCP      2
#define RSE_ADDR_LOOPBACK  0x7F000001u

struct rse_stat {
    uint64_t size;
    uint32_t mode;
    uint32_t type;
    uint32_t uid;
    uint32_t gid;
};

struct rse_sockaddr {
    uint16_t family;
    uint16_t port;
    uint32_t addr;
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

static inline int64_t sys_socket(uint16_t domain, uint16_t type, uint16_t protocol) {
    return rse_syscall6(SYS_SOCKET, domain, type, protocol, 0, 0, 0);
}

static inline int64_t sys_bind(int32_t fd, const struct rse_sockaddr* addr, uint32_t len) {
    return rse_syscall6(SYS_BIND, (uint64_t)fd, (uint64_t)addr, len, 0, 0, 0);
}

static inline int64_t sys_listen(int32_t fd, uint32_t backlog) {
    return rse_syscall6(SYS_LISTEN, (uint64_t)fd, backlog, 0, 0, 0, 0);
}

static inline int64_t sys_accept(int32_t fd, struct rse_sockaddr* addr, uint32_t* len) {
    return rse_syscall6(SYS_ACCEPT, (uint64_t)fd, (uint64_t)addr, (uint64_t)len, 0, 0, 0);
}

static inline int64_t sys_connect(int32_t fd, const struct rse_sockaddr* addr, uint32_t len) {
    return rse_syscall6(SYS_CONNECT, (uint64_t)fd, (uint64_t)addr, len, 0, 0, 0);
}

static inline void maybe_yield(uint64_t step, uint64_t stride) {
    if (stride != 0 && step != 0 && (step % stride) == 0) {
        sys_yield();
    }
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

static int cstr_eq(const char* a, const char* b) {
    if (!a || !b) {
        return 0;
    }
    uint32_t idx = 0;
    while (a[idx] && b[idx]) {
        if (a[idx] != b[idx]) {
            return 0;
        }
        idx++;
    }
    return a[idx] == '\0' && b[idx] == '\0';
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

static void write_rc(const char* label, int64_t rc) {
    write_str(label);
    if (rc < 0) {
        write_str("-");
        write_u64((uint64_t)(-rc));
    } else {
        write_u64((uint64_t)rc);
    }
    write_str("\n");
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

static uint64_t parse_u64(const char* s, uint64_t fallback) {
    if (!s || *s == '\0') {
        return fallback;
    }
    uint64_t value = 0;
    const char* p = s;
    while (*p >= '0' && *p <= '9') {
        value = (value * 10u) + (uint64_t)(*p - '0');
        ++p;
    }
    return (p == s) ? fallback : value;
}

static const char* skip_ws(const char* s) {
    if (!s) {
        return s;
    }
    while (*s == ' ' || *s == '\t' || *s == '\r') {
        ++s;
    }
    return s;
}

static const char* next_token(const char* s, char* out, uint32_t cap) {
    if (!out || cap == 0) {
        return s;
    }
    out[0] = '\0';
    s = skip_ws(s);
    if (!s || *s == '\0') {
        return s;
    }
    uint32_t idx = 0;
    while (*s && *s != ' ' && *s != '\t' && *s != '\r') {
        if (idx + 1 < cap) {
            out[idx++] = *s;
        }
        ++s;
    }
    out[idx] = '\0';
    return s;
}

static int read_file(const char* path, char* out, uint32_t cap) {
    if (!path || !out || cap == 0) {
        return -1;
    }
    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    uint32_t used = 0;
    while (used + 1 < cap) {
        int64_t got = sys_read(fd, out + used, cap - 1 - used);
        if (got <= 0) {
            break;
        }
        used += (uint32_t)got;
    }
    out[used] = '\0';
    sys_close(fd);
    return (int)used;
}

static int detect_persist(void) {
    int persist_mode = 0;
    int probe = sys_open("/persist/.probe", O_CREAT | O_WRONLY, 0644);
    if (probe >= 0) {
        sys_close(probe);
        sys_unlink("/persist/.probe");
        persist_mode = 1;
    }
    return persist_mode;
}

static void run_compute(uint64_t iters) {
    uint64_t seed = 0xfeedbeefcafebabeULL;
    uint64_t acc = 0;
    uint64_t start = rdtsc();
    for (uint64_t i = 0; i < iters; ++i) {
        acc ^= xorshift64(&seed) + (i << 1);
        maybe_yield(i, INIT_COMPUTE_YIELD_STRIDE);
    }
    uint64_t end = rdtsc();
    write_str("[init] compute ops=");
    write_u64(iters);
    write_str(" cycles=");
    write_u64(end - start);
    write_str(" checksum=");
    write_u64(acc);
    write_str("\n");
}

static void run_memstress(uint64_t passes) {
    for (uint32_t i = 0; i < sizeof(mem_a); ++i) {
        mem_a[i] = (uint8_t)(i ^ 0x5a);
    }
    uint64_t checksum = 0;
    uint64_t mem_start = rdtsc();
    for (uint64_t p = 0; p < passes; ++p) {
        for (uint32_t i = 0; i < sizeof(mem_a); ++i) {
            mem_b[i] = (uint8_t)(mem_a[i] + (uint8_t)p);
            checksum += mem_b[i];
        }
        maybe_yield(p, INIT_MEMSTRESS_YIELD_PASSES);
    }
    uint64_t mem_end = rdtsc();
    write_str("[init] memstress bytes=");
    write_u64((uint64_t)sizeof(mem_a) * passes);
    write_str(" cycles=");
    write_u64(mem_end - mem_start);
    write_str(" checksum=");
    write_u64(checksum);
    write_str("\n");
}

static void run_file_io(uint32_t file_count, int persist_mode) {
    for (uint32_t i = 0; i < sizeof(io_buf); ++i) {
        io_buf[i] = (uint8_t)(i ^ 0x5a);
    }
    uint64_t ops = 0;
    uint64_t bytes = 0;
    uint64_t io_start = rdtsc();
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
        maybe_yield(i, INIT_FILEIO_YIELD_STRIDE);
    }
    for (uint32_t i = 0; i < file_count; ++i) {
        format_path(name, i, persist_mode);
        sys_unlink(name);
        ops++;
        maybe_yield(i, INIT_FILEIO_YIELD_STRIDE);
    }
    uint64_t io_end = rdtsc();
    write_str("[init] file ops=");
    write_u64(ops);
    write_str(" bytes=");
    write_u64(bytes);
    write_str(" cycles=");
    write_u64(io_end - io_start);
    write_str("\n");
}

static void run_loopback(void) {
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

static void run_blk0(uint32_t blocks) {
    int fd = sys_open("/dev/blk0", O_RDWR, 0);
    if (fd >= 0) {
        const uint32_t blk_size = sizeof(blk_buf);
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
            maybe_yield(i, INIT_BLOCK_YIELD_STRIDE);
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
            maybe_yield(i, INIT_BLOCK_YIELD_STRIDE);
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

static void run_net0(uint32_t iters) {
    int net_fd = sys_open("/dev/net0", O_RDWR, 0);
    if (net_fd >= 0) {
        uint8_t pkt[64];
        for (uint32_t i = 0; i < sizeof(pkt); ++i) {
            pkt[i] = (uint8_t)(i ^ 0x3c);
        }
        uint64_t wrote = 0;
        uint64_t read = 0;
        uint64_t net_start = rdtsc();
        for (uint32_t i = 0; i < iters; ++i) {
            int64_t w = sys_write(net_fd, pkt, sizeof(pkt));
            if (w > 0) {
                wrote += (uint64_t)w;
            }
            int64_t r = sys_read(net_fd, pkt, sizeof(pkt));
            if (r > 0) {
                read += (uint64_t)r;
            }
            maybe_yield(i, INIT_NET_YIELD_STRIDE);
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

#if RSE_NET_RAW
static int rawtcp_read_exact(int32_t fd, uint8_t* buf, uint32_t len) {
    uint32_t offset = 0;
    for (uint32_t attempt = 0; attempt < 64 && offset < len; ++attempt) {
        int64_t got = sys_read(fd, buf + offset, len - offset);
        if (got > 0) {
            offset += (uint32_t)got;
            continue;
        }
        if (got == -EAGAIN) {
            sys_yield();
            continue;
        }
        return 0;
    }
    return offset == len;
}

static int rawtcp_write_exact(int32_t fd, const uint8_t* buf, uint32_t len) {
    uint32_t offset = 0;
    for (uint32_t attempt = 0; attempt < 32 && offset < len; ++attempt) {
        int64_t wrote = sys_write(fd, buf + offset, len - offset);
        if (wrote > 0) {
            offset += (uint32_t)wrote;
            continue;
        }
        if (wrote == -EAGAIN) {
            sys_yield();
            continue;
        }
        return 0;
    }
    return offset == len;
}

static int rawtcp_buf_eq(const uint8_t* a, const uint8_t* b, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static void run_rawtcp_smoke(void) {
    write_str("[init] rawtcp start\n");
    int64_t server_fd = sys_socket(RSE_AF_INET, RSE_SOCK_STREAM, RSE_PROTO_TCP);
    if (server_fd < 0) {
        write_rc("[init] rawtcp socket rc=", server_fd);
        return;
    }
    write_str("[init] rawtcp socket ok\n");
    struct rse_sockaddr addr = { RSE_AF_INET, 5052, 0 };
    int64_t bind_rc = sys_bind((int32_t)server_fd, &addr, sizeof(addr));
    if (bind_rc < 0) {
        write_rc("[init] rawtcp bind rc=", bind_rc);
        sys_close((int32_t)server_fd);
        return;
    }
    write_str("[init] rawtcp bind ok\n");
    int64_t listen_rc = sys_listen((int32_t)server_fd, 1);
    if (listen_rc < 0) {
        write_rc("[init] rawtcp listen rc=", listen_rc);
        sys_close((int32_t)server_fd);
        return;
    }
    write_str("[init] rawtcp listen ok\n");
    int64_t client_fd = sys_socket(RSE_AF_INET, RSE_SOCK_STREAM, RSE_PROTO_TCP);
    if (client_fd < 0) {
        write_rc("[init] rawtcp client socket rc=", client_fd);
        sys_close((int32_t)server_fd);
        return;
    }
    write_str("[init] rawtcp client socket ok\n");
    int64_t connect_rc = sys_connect((int32_t)client_fd, &addr, sizeof(addr));
    write_str("[init] rawtcp connect rc=");
    if (connect_rc < 0) {
        write_str("-");
        write_u64((uint64_t)(-connect_rc));
    } else {
        write_u64((uint64_t)connect_rc);
    }
    write_str("\n");
    if (connect_rc < 0 && connect_rc != -EAGAIN && connect_rc != -EISCONN) {
        write_rc("[init] rawtcp connect rc=", connect_rc);
        sys_close((int32_t)client_fd);
        sys_close((int32_t)server_fd);
        return;
    }
    write_str("[init] rawtcp connect waiting\n");

    int64_t accept_fd = -1;
    struct rse_sockaddr peer = {};
    uint32_t peer_len = sizeof(peer);
    for (uint32_t attempt = 0; attempt < 64; ++attempt) {
        accept_fd = sys_accept((int32_t)server_fd, &peer, &peer_len);
        if (accept_fd >= 0) {
            break;
        }
        if (accept_fd != -EAGAIN) {
            write_rc("[init] rawtcp accept rc=", accept_fd);
            break;
        }
        int64_t retry = sys_connect((int32_t)client_fd, &addr, sizeof(addr));
        if (retry < 0 && retry != -EAGAIN && retry != -EISCONN) {
            write_rc("[init] rawtcp connect rc=", retry);
            break;
        }
        sys_yield();
    }
    if (accept_fd < 0) {
        write_rc("[init] rawtcp accept rc=", accept_fd);
        sys_close((int32_t)client_fd);
        sys_close((int32_t)server_fd);
        return;
    }
    write_str("[init] rawtcp accept ok\n");

    const uint8_t msg[] = "rawtcp";
    uint8_t buf[16] = {};
    if (!rawtcp_write_exact((int32_t)client_fd, msg, (uint32_t)(sizeof(msg) - 1))) {
        write_str("[init] rawtcp write fail\n");
        goto rawtcp_cleanup;
    }
    if (!rawtcp_read_exact((int32_t)accept_fd, buf, (uint32_t)(sizeof(msg) - 1))) {
        write_str("[init] rawtcp read fail\n");
        goto rawtcp_cleanup;
    }
    if (!rawtcp_buf_eq(msg, buf, (uint32_t)(sizeof(msg) - 1))) {
        write_str("[init] rawtcp mismatch\n");
        goto rawtcp_cleanup;
    }
    const uint8_t reply[] = "rawpong";
    if (!rawtcp_write_exact((int32_t)accept_fd, reply, (uint32_t)(sizeof(reply) - 1))) {
        write_str("[init] rawtcp reply write fail\n");
        goto rawtcp_cleanup;
    }
    if (!rawtcp_read_exact((int32_t)client_fd, buf, (uint32_t)(sizeof(reply) - 1))) {
        write_str("[init] rawtcp reply read fail\n");
        goto rawtcp_cleanup;
    }
    if (!rawtcp_buf_eq(reply, buf, (uint32_t)(sizeof(reply) - 1))) {
        write_str("[init] rawtcp reply mismatch\n");
        goto rawtcp_cleanup;
    }
    write_str("[init] rawtcp ok\n");

rawtcp_cleanup:
    sys_close((int32_t)accept_fd);
    sys_close((int32_t)client_fd);
    sys_close((int32_t)server_fd);
}
#endif

static void run_help(void) {
    write_str("commands: help, compute, memstress, fileio, ls, cat, stat, blk, net, loop, run, yield, exit\n");
}

static void run_command(const char* line, int persist_mode) {
    char cmd[16];
    char arg[64];
    const char* rest = next_token(line, cmd, sizeof(cmd));
    if (cmd[0] == '\0') {
        return;
    }
    rest = next_token(rest, arg, sizeof(arg));
    if (cmd[0] == '#') {
        return;
    }
    if (!cstr_len(cmd)) {
        return;
    }
    if (cstr_eq(cmd, "help")) {
        run_help();
    } else if (cstr_eq(cmd, "compute")) {
        uint64_t iters = parse_u64(arg, 1000000ULL);
        run_compute(iters);
    } else if (cstr_eq(cmd, "memstress")) {
        uint64_t passes = parse_u64(arg, INIT_MEMSTRESS_PASSES);
        run_memstress(passes);
    } else if (cstr_eq(cmd, "fileio")) {
        uint32_t count = (uint32_t)parse_u64(arg, INIT_FILE_BENCH_FILES);
        run_file_io(count, persist_mode);
    } else if (cstr_eq(cmd, "ls")) {
        const char* path = (arg[0] != '\0') ? arg : "/";
        log_list(path);
        write_str("\n");
    } else if (cstr_eq(cmd, "cat")) {
        if (arg[0] == '\0') {
            write_str("cat: missing path\n");
            return;
        }
        int fd = sys_open(arg, O_RDONLY, 0);
        if (fd < 0) {
            write_str("cat: open failed\n");
            return;
        }
        char buf[256];
        int64_t got = sys_read(fd, buf, sizeof(buf));
        if (got > 0) {
            sys_write(1, buf, (uint32_t)got);
        }
        sys_close(fd);
        write_str("\n");
    } else if (cstr_eq(cmd, "stat")) {
        if (arg[0] == '\0') {
            write_str("stat: missing path\n");
            return;
        }
        log_stat(arg);
    } else if (cstr_eq(cmd, "blk")) {
        run_blk0(INIT_BLOCK_BENCH_BLOCKS);
    } else if (cstr_eq(cmd, "net")) {
        run_net0(INIT_NET_BENCH_ITERS);
    } else if (cstr_eq(cmd, "loop")) {
        run_loopback();
    } else if (cstr_eq(cmd, "run")) {
        if (arg[0] == '\0') {
            write_str("run: missing path\n");
            return;
        }
        int64_t rc = sys_exec(arg);
        write_str("run rc=");
        if (rc < 0) {
            write_str("-");
            write_u64((uint64_t)(-rc));
        } else {
            write_u64((uint64_t)rc);
        }
        write_str("\n");
    } else if (cstr_eq(cmd, "yield")) {
        sys_yield();
    } else if (cstr_eq(cmd, "exit")) {
        sys_exit(0);
    } else {
        write_str("unknown command: ");
        write_str(cmd);
        write_str("\n");
    }
}

static int run_script(const char* path, int persist_mode) {
    char script[2048];
    int got = read_file(path, script, sizeof(script));
    if (got <= 0) {
        return 0;
    }
    write_str("[init] script ");
    write_str(path);
    write_str("\n");
    const char* p = script;
    const char* end = script + got;
    char line[128];
    while (p < end) {
        uint32_t len = 0;
        while (p < end && *p != '\n') {
            if (len + 1 < sizeof(line)) {
                line[len++] = *p;
            }
            ++p;
        }
        if (p < end && *p == '\n') {
            ++p;
        }
        line[len] = '\0';
        const char* trimmed = skip_ws(line);
        if (!trimmed || *trimmed == '\0') {
            continue;
        }
        if (*trimmed == '#') {
            continue;
        }
        run_command(trimmed, persist_mode);
        sys_yield();
    }
    return 1;
}

static void init_main(void) {
    write_str("[init] ring3 start\n");
    int64_t torus = sys_torus_id();
    if (torus < 0) {
        torus = 0;
    }
    int64_t pid = sys_getpid();
    if (pid < 0) {
        pid = 0;
    }
    write_str("[init] torus=");
    write_u64((uint64_t)torus);
    write_str(" pid=");
    write_u64((uint64_t)pid);
    write_str("\n");

#if RSE_RAWTCP_ONLY
#if RSE_NET_RAW
    if (torus == 2) {
        if (pid == 2) {
            run_rawtcp_smoke();
        } else {
            write_str("[init] rawtcp-only skip pid=");
            write_u64((uint64_t)pid);
            write_str("\n");
        }
    } else {
        write_str("[init] rawtcp-only skip torus=");
        write_u64((uint64_t)torus);
        write_str("\n");
    }
#else
    write_str("[init] rawtcp-only requires RSE_NET_RAW\n");
#endif
    return;
#endif

    uint64_t iters = (torus == 1) ? INIT_TORUS1_COMPUTE_ITERS : INIT_TORUS0_COMPUTE_ITERS;
    run_compute(iters);
    for (uint32_t i = 0; i < 2; ++i) {
        sys_yield();
    }

    if (torus == 1) {
        run_memstress(INIT_MEMSTRESS_PASSES);
        for (uint32_t i = 0; i < 2; ++i) {
            sys_yield();
        }
    }

    if (torus == 0) {
        int persist_mode = detect_persist();
        write_str(persist_mode ? "[init] using /persist\n" : "[init] using memfs\n");
        if (!run_script("/persist/boot.rc", persist_mode) &&
            !run_script("/boot.rc", persist_mode)) {
            run_file_io(INIT_FILE_BENCH_FILES, persist_mode);
            write_str("[init] ls /\n");
            log_list("/");
            write_str("\n");
            write_str("[init] ls /persist\n");
            log_list("/persist");
            write_str("\n");
            log_stat("/dev/blk0");
            run_loopback();
            run_blk0(INIT_BLOCK_BENCH_BLOCKS);
        }
    }

    if (torus == 2) {
        run_net0(INIT_NET_BENCH_ITERS);
#if RSE_NET_RAW
        if (pid == 2) {
            run_rawtcp_smoke();
        } else {
            write_str("[init] rawtcp skip pid=");
            write_u64((uint64_t)pid);
            write_str("\n");
        }
#endif
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
