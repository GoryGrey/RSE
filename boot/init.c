#include <stdint.h>

#include "rse_syscalls.h"

static uint64_t xorshift64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static void format_name(char *buf, uint32_t index) {
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

static void format_path(char *buf, uint32_t index, int persist) {
    if (persist) {
        const char *prefix = "/persist/";
        uint32_t i = 0;
        for (; prefix[i] != '\0'; ++i) {
            buf[i] = prefix[i];
        }
        format_name(buf + i, index);
        return;
    }
    format_name(buf, index);
}

static uint32_t cstr_len(const char *s) {
    uint32_t len = 0;
    if (!s) {
        return 0;
    }
    while (s[len]) {
        len++;
    }
    return len;
}

static void shell_emit(const struct rse_syscalls *sys, const char *msg) {
    if (!sys || !sys->write || !msg) {
        return;
    }
    sys->write(1, (const uint8_t *)msg, cstr_len(msg));
}

static void shell_cat(const struct rse_syscalls *sys, const char *path) {
    if (!sys || !sys->open || !sys->read || !sys->close || !path) {
        return;
    }
    int fd = sys->open(path, 0x0000u);
    if (fd < 0) {
        shell_emit(sys, "cat: open failed\n");
        return;
    }
    uint8_t buf[128];
    int got = sys->read(fd, buf, sizeof(buf));
    if (got > 0) {
        sys->write(1, buf, (uint32_t)got);
        shell_emit(sys, "\n");
    } else {
        shell_emit(sys, "cat: empty\n");
    }
    sys->close(fd);
}

static void shell_ls(const struct rse_syscalls *sys, const char *path) {
    if (!sys || !sys->list) {
        shell_emit(sys, "ls: unsupported\n");
        return;
    }
    char buf[512];
    int got = sys->list(path, buf, sizeof(buf));
    if (got <= 0) {
        shell_emit(sys, "ls: empty\n");
        return;
    }
    sys->write(1, (const uint8_t *)buf, (uint32_t)got);
}

static void shell_ps(const struct rse_syscalls *sys) {
    if (!sys || !sys->ps || !sys->write) {
        shell_emit(sys, "ps: unsupported\n");
        return;
    }
    char buf[512];
    int got = sys->ps(buf, sizeof(buf));
    if (got <= 0) {
        shell_emit(sys, "ps: empty\n");
        return;
    }
    sys->write(1, (const uint8_t *)buf, (uint32_t)got);
}

static void shell_probe_dev(const struct rse_syscalls *sys, const char *dev) {
    if (!sys || !sys->open || !sys->close || !dev) {
        return;
    }
    int fd = sys->open(dev, 0x0002u);
    shell_emit(sys, "probe ");
    shell_emit(sys, dev);
    shell_emit(sys, fd >= 0 ? " ok\n" : " missing\n");
    if (fd >= 0) {
        sys->close(fd);
    }
}

static void shell_demo(const struct rse_syscalls *sys, int persist) {
    const char *path = persist ? "/persist/hello.txt" : "hello.txt";
    shell_emit(sys, "rse> help\n");
    shell_emit(sys, "help: echo, cat, ls, probe, ps\n");

    shell_emit(sys, "rse> ps\n");
    shell_ps(sys);

    shell_emit(sys, "rse> echo shell-online\n");
    shell_emit(sys, "shell-online\n");

    if (sys && sys->open && sys->write && sys->close) {
        int fd = sys->open(path, 0x0040u | 0x0200u | 0x0002u);
        if (fd >= 0) {
            const char *msg = "hello from RSE shell";
            sys->write(fd, (const uint8_t *)msg, cstr_len(msg));
            sys->close(fd);
        }
    }

    shell_emit(sys, "rse> cat ");
    shell_emit(sys, path);
    shell_emit(sys, "\n");
    shell_cat(sys, path);

    shell_emit(sys, "rse> ls /\n");
    shell_ls(sys, "/");
    shell_emit(sys, "\n");
    shell_emit(sys, "rse> ls /persist\n");
    shell_ls(sys, "/persist");
    shell_emit(sys, "\n");

    shell_emit(sys, "rse> probe devices\n");
    shell_probe_dev(sys, "/dev/blk0");
    shell_probe_dev(sys, "/dev/net0");
    shell_probe_dev(sys, "/dev/loopback");
}

void init_main(const struct rse_syscalls *sys) {
    if (!sys || !sys->log || !sys->log_u64 || !sys->rdtsc) {
        return;
    }

    sys->log("[init] start\n");
    uint32_t torus_id = sys->get_torus_id ? sys->get_torus_id() : 0;
    sys->log("[init] torus=");
    sys->log_u64(torus_id);
    sys->log("\n");

    uint64_t seed = 0xfeedbeefcafebabeULL;
    const uint64_t iters = (torus_id == 1) ? 6000000ULL : 2000000ULL;
    uint64_t acc = 0;
    uint64_t start = sys->rdtsc();
    for (uint64_t i = 0; i < iters; ++i) {
        acc ^= xorshift64(&seed) + (i << 1);
    }
    uint64_t end = sys->rdtsc();
    sys->log("[init] compute ops=");
    sys->log_u64(iters);
    sys->log(" cycles=");
    sys->log_u64(end - start);
    sys->log(" checksum=");
    sys->log_u64(acc);
    sys->log("\n");

    if (!sys->open || !sys->write || !sys->read || !sys->close || !sys->unlink) {
        sys->log("[init] file I/O syscalls missing\n");
        return;
    }

    int persist_mode = 0;

    if (torus_id == 1) {
        uint8_t mem_a[16384];
        uint8_t mem_b[16384];
        for (uint32_t i = 0; i < sizeof(mem_a); ++i) {
            mem_a[i] = (uint8_t)(i ^ 0x5a);
        }
        const uint64_t passes = 1024;
        uint64_t mem_start = sys->rdtsc();
        uint64_t checksum = 0;
        for (uint64_t p = 0; p < passes; ++p) {
            for (uint32_t i = 0; i < sizeof(mem_a); ++i) {
                mem_b[i] = (uint8_t)(mem_a[i] + (uint8_t)p);
                checksum += mem_b[i];
            }
        }
        uint64_t mem_end = sys->rdtsc();
        sys->log("[init] memstress bytes=");
        sys->log_u64((uint64_t)sizeof(mem_a) * passes);
        sys->log(" cycles=");
        sys->log_u64(mem_end - mem_start);
        sys->log(" checksum=");
        sys->log_u64(checksum);
        sys->log("\n");
    }

    uint8_t buf[4096];
    for (uint32_t i = 0; i < sizeof(buf); ++i) {
        buf[i] = (uint8_t)(i ^ 0x5a);
    }

    if (sys->pipe_push && sys->pipe_pop) {
        uint8_t msg[128];
        if (torus_id == 0) {
            const uint32_t packets = 64;
            uint64_t start = sys->rdtsc();
            uint64_t bytes_a = 0;
            uint64_t bytes_b = 0;
            for (uint32_t i = 0; i < packets; ++i) {
                for (uint32_t j = 0; j < sizeof(msg); ++j) {
                    msg[j] = (uint8_t)(i ^ j ^ 0x3a);
                }
                bytes_a += sys->pipe_push(0, msg, sizeof(msg));
                msg[0] ^= 0x55u;
                bytes_b += sys->pipe_push(1, msg, sizeof(msg));
            }
            uint64_t end = sys->rdtsc();
            sys->log("[init] pipe stage0 q0=");
            sys->log_u64(bytes_a);
            sys->log(" q1=");
            sys->log_u64(bytes_b);
            sys->log(" cycles=");
            sys->log_u64(end - start);
            sys->log("\n");
        } else if (torus_id == 1) {
            uint64_t start = sys->rdtsc();
            uint64_t in_bytes = 0;
            uint64_t out_bytes = 0;
            uint64_t checksum = 0;
            for (;;) {
                uint32_t got = sys->pipe_pop(0, msg, sizeof(msg));
                if (got == 0) {
                    break;
                }
                in_bytes += got;
                for (uint32_t i = 0; i < got; ++i) {
                    msg[i] ^= 0xA5u;
                    checksum += msg[i];
                }
                out_bytes += sys->pipe_push(2, msg, got);
            }
            uint64_t end = sys->rdtsc();
            sys->log("[init] pipe stage1 in=");
            sys->log_u64(in_bytes);
            sys->log(" out=");
            sys->log_u64(out_bytes);
            sys->log(" cycles=");
            sys->log_u64(end - start);
            sys->log(" checksum=");
            sys->log_u64(checksum);
            sys->log("\n");
        } else if (torus_id == 2) {
            uint64_t stage_start = sys->rdtsc();
            uint64_t stage_in = 0;
            uint64_t stage_out = 0;
            uint64_t stage_checksum = 0;
            for (;;) {
                uint32_t got = sys->pipe_pop(1, msg, sizeof(msg));
                if (got == 0) {
                    break;
                }
                stage_in += got;
                for (uint32_t i = 0; i < got; ++i) {
                    msg[i] = (uint8_t)(msg[i] ^ 0x3cu);
                    stage_checksum += msg[i];
                }
                stage_out += sys->pipe_push(3, msg, got);
            }
            uint64_t stage_end = sys->rdtsc();
            sys->log("[init] pipe stage2a in=");
            sys->log_u64(stage_in);
            sys->log(" out=");
            sys->log_u64(stage_out);
            sys->log(" cycles=");
            sys->log_u64(stage_end - stage_start);
            sys->log(" checksum=");
            sys->log_u64(stage_checksum);
            sys->log("\n");

            uint64_t start = sys->rdtsc();
            uint64_t in_bytes = 0;
            uint64_t wrote = 0;
            uint64_t checksum = 0;
            int net_fd = sys->open ? sys->open("/dev/net0", 0x0002u) : -1;
            for (;;) {
                uint32_t got_a = sys->pipe_pop(2, msg, sizeof(msg));
                if (got_a) {
                    in_bytes += got_a;
                    for (uint32_t i = 0; i < got_a; ++i) {
                        checksum += msg[i];
                    }
                    if (net_fd >= 0 && sys->write) {
                        int w = sys->write(net_fd, msg, got_a);
                        if (w > 0) {
                            wrote += (uint64_t)w;
                        }
                    }
                }
                uint32_t got_b = sys->pipe_pop(3, msg, sizeof(msg));
                if (got_b) {
                    in_bytes += got_b;
                    for (uint32_t i = 0; i < got_b; ++i) {
                        checksum += msg[i];
                    }
                    if (net_fd >= 0 && sys->write) {
                        int w = sys->write(net_fd, msg, got_b);
                        if (w > 0) {
                            wrote += (uint64_t)w;
                        }
                    }
                }
                if (got_a == 0 && got_b == 0) {
                    break;
                }
            }
            if (net_fd >= 0 && sys->close) {
                sys->close(net_fd);
            }
            uint64_t end = sys->rdtsc();
            sys->log("[init] pipe stage2b in=");
            sys->log_u64(in_bytes);
            sys->log(" wrote=");
            sys->log_u64(wrote);
            sys->log(" cycles=");
            sys->log_u64(end - start);
            sys->log(" checksum=");
            sys->log_u64(checksum);
            sys->log("\n");
            if (sys->report_net) {
                sys->report_net(wrote, end - start);
            }
        }
    }

    if (torus_id == 0) {
        const uint32_t file_count = 128;
        char name[32];
        if (sys->open && sys->close && sys->unlink) {
            int probe = sys->open("/persist/.probe", 0x0040u | 0x0002u);
            if (probe >= 0) {
                sys->close(probe);
                sys->unlink("/persist/.probe");
                persist_mode = 1;
            }
        }
        if (persist_mode) {
            sys->log("[init] using /persist\n");
        } else {
            sys->log("[init] using memfs\n");
        }
        uint64_t ops = 0;
        uint64_t bytes = 0;
        uint64_t io_start = sys->rdtsc();
        for (uint32_t i = 0; i < file_count; ++i) {
            format_path(name, i, persist_mode);
            int fd = sys->open(name, 0x0040u | 0x0200u | 0x0002u);
            if (fd < 0) {
                continue;
            }
            ops++;
            int wrote = sys->write(fd, buf, sizeof(buf));
            if (wrote > 0) {
                bytes += (uint64_t)wrote;
            }
            ops++;
            int read = sys->read(fd, buf, sizeof(buf));
            if (read > 0) {
                bytes += (uint64_t)read;
            }
            ops++;
            sys->close(fd);
            ops++;
        }
        for (uint32_t i = 0; i < file_count; ++i) {
            format_path(name, i, persist_mode);
            sys->unlink(name);
            ops++;
        }
        uint64_t io_end = sys->rdtsc();

        sys->log("[init] memfs ops=");
        sys->log_u64(ops);
        sys->log(" bytes=");
        sys->log_u64(bytes);
        sys->log(" cycles=");
        sys->log_u64(io_end - io_start);
        sys->log("\n");
    }

    // Block device stress test (/dev/blk0)
    if (torus_id == 0) {
        sys->log("[init] block device test\n");
        if (!sys->lseek) {
            sys->log("[init] /dev/blk0 lseek missing\n");
        } else {
            const uint32_t blk_size = 512;
            const uint32_t blocks = 128;
            const uint64_t start_lba = 2048;
            int fd = sys->open("/dev/blk0", 0x0002u);
            if (fd < 0) {
                sys->log("[init] /dev/blk0 not available\n");
            } else {
                uint64_t blk_ops = 0;
                uint64_t blk_bytes = 0;
                uint64_t mismatches = 0;
                uint64_t blk_start = sys->rdtsc();

                sys->lseek(fd, (int64_t)(start_lba * blk_size), 0);
                for (uint32_t i = 0; i < blocks; ++i) {
                    for (uint32_t j = 0; j < blk_size; ++j) {
                        buf[j] = (uint8_t)(j ^ (i & 0xFFu) ^ 0xA5u);
                    }
                    int w = sys->write(fd, buf, blk_size);
                    if (w == (int)blk_size) {
                        blk_bytes += (uint64_t)w;
                        blk_ops++;
                    }
                }

                sys->lseek(fd, (int64_t)(start_lba * blk_size), 0);
                for (uint32_t i = 0; i < blocks; ++i) {
                    int r = sys->read(fd, buf, blk_size);
                    if (r == (int)blk_size) {
                        blk_bytes += (uint64_t)r;
                        blk_ops++;
                        for (uint32_t j = 0; j < blk_size; ++j) {
                            uint8_t expect = (uint8_t)(j ^ (i & 0xFFu) ^ 0xA5u);
                            if (buf[j] != expect) {
                                mismatches++;
                                break;
                            }
                        }
                    }
                }
            uint64_t blk_end = sys->rdtsc();
            sys->close(fd);

            sys->log("[init] /dev/blk0 size=");
            sys->log_u64(blk_size);
            sys->log(" ops=");
            sys->log_u64(blk_ops);
            sys->log(" bytes=");
            sys->log_u64(blk_bytes);
            sys->log(" mismatches=");
            sys->log_u64(mismatches);
            sys->log(" cycles=");
            sys->log_u64(blk_end - blk_start);
            sys->log("\n");
            if (sys->report_block) {
                sys->report_block(blk_bytes, blk_end - blk_start);
            }
        }
    }

        sys->log("[init] loopback test\n");
        int loop_fd = sys->open("/dev/loopback", 0x0002u);
        if (loop_fd >= 0) {
            const uint8_t *msg = (const uint8_t *)"loopback-test";
            int w = sys->write(loop_fd, msg, 13);
            uint8_t loop_buf[32];
            int r = sys->read(loop_fd, loop_buf, 13);
            sys->close(loop_fd);
            sys->log("[init] loopback wrote=");
            sys->log_u64((uint64_t)w);
            sys->log(" read=");
            sys->log_u64((uint64_t)r);
            sys->log("\n");
        } else {
            sys->log("[init] /dev/loopback not available\n");
        }
    }

    // Network device test (/dev/net0)
    if (torus_id == 2) {
        sys->log("[init] net0 test\n");
        int net_fd = sys->open("/dev/net0", 0x0002u);
        if (net_fd >= 0) {
            uint8_t pkt[64];
            for (uint32_t i = 0; i < sizeof(pkt); ++i) {
                pkt[i] = (uint8_t)(i ^ 0x3c);
            }
            uint64_t net_start = sys->rdtsc();
            uint64_t wrote = 0;
            uint64_t read = 0;
            for (uint32_t i = 0; i < 256; ++i) {
                int nw = sys->write(net_fd, pkt, sizeof(pkt));
                if (nw > 0) {
                    wrote += (uint64_t)nw;
                }
                int nr = sys->read(net_fd, pkt, sizeof(pkt));
                if (nr > 0) {
                    read += (uint64_t)nr;
                }
            }
            uint64_t net_end = sys->rdtsc();
            sys->close(net_fd);
            sys->log("[init] net0 wrote=");
            sys->log_u64(wrote);
            sys->log(" read=");
            sys->log_u64(read);
            sys->log(" cycles=");
            sys->log_u64(net_end - net_start);
            sys->log("\n");
            if (sys->report_net) {
                sys->report_net(wrote + read, net_end - net_start);
            }
        } else {
            sys->log("[init] /dev/net0 not available\n");
        }
    }

    if (torus_id == 0) {
        shell_demo(sys, persist_mode);
    }
}
