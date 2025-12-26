#pragma once

#include <cstdint>
#include <cstddef>

/**
 * System Call Interface for Braided OS
 * 
 * POSIX-like syscall API for applications to interact with the OS.
 */

namespace os {

// ========== System Call Numbers ==========

// Process management
constexpr int SYS_FORK      = 1;
constexpr int SYS_EXEC      = 2;
constexpr int SYS_EXIT      = 3;
constexpr int SYS_WAIT      = 4;
constexpr int SYS_GETPID    = 5;
constexpr int SYS_GETPPID   = 6;
constexpr int SYS_KILL      = 7;

// File I/O
constexpr int SYS_OPEN      = 10;
constexpr int SYS_CLOSE     = 11;
constexpr int SYS_READ      = 12;
constexpr int SYS_WRITE     = 13;
constexpr int SYS_LSEEK     = 14;
constexpr int SYS_STAT      = 15;
constexpr int SYS_UNLINK    = 16;
constexpr int SYS_LIST      = 17;

// Memory management
constexpr int SYS_BRK       = 20;
constexpr int SYS_MMAP      = 21;
constexpr int SYS_MUNMAP    = 22;
constexpr int SYS_MPROTECT  = 23;

// IPC
constexpr int SYS_PIPE      = 30;
constexpr int SYS_DUP       = 31;
constexpr int SYS_DUP2      = 32;
constexpr int SYS_SIGNAL    = 33;

// Time
constexpr int SYS_TIME      = 40;
constexpr int SYS_SLEEP     = 41;
constexpr int SYS_NANOSLEEP = 42;

// ========== Error Codes ==========

#ifndef EPERM
constexpr int EPERM     = 1;   // Operation not permitted
#endif
#ifndef ENOENT
constexpr int ENOENT    = 2;   // No such file or directory
#endif
#ifndef ESRCH
constexpr int ESRCH     = 3;   // No such process
#endif
#ifndef EINTR
constexpr int EINTR     = 4;   // Interrupted system call
#endif
#ifndef EIO
constexpr int EIO       = 5;   // I/O error
#endif
#ifndef EBADF
constexpr int EBADF     = 9;   // Bad file descriptor
#endif
#ifndef ECHILD
constexpr int ECHILD    = 10;  // No child processes
#endif
#ifndef ENOMEM
constexpr int ENOMEM    = 12;  // Out of memory
#endif
#ifndef EACCES
constexpr int EACCES    = 13;  // Permission denied
#endif
#ifndef EFAULT
constexpr int EFAULT    = 14;  // Bad address
#endif
#ifndef EINVAL
constexpr int EINVAL    = 22;  // Invalid argument
#endif
#ifndef ENOSYS
constexpr int ENOSYS    = 38;  // Function not implemented
#endif

// ========== File Flags ==========

constexpr int O_RDONLY  = 0x0000;  // Open for reading only
constexpr int O_WRONLY  = 0x0001;  // Open for writing only
constexpr int O_RDWR    = 0x0002;  // Open for reading and writing
constexpr int O_CREAT   = 0x0040;  // Create file if it doesn't exist
constexpr int O_TRUNC   = 0x0200;  // Truncate file to zero length
constexpr int O_APPEND  = 0x0400;  // Append to file
constexpr int O_CLOEXEC = 0x80000; // Close on exec

// ========== Memory Protection Flags ==========

constexpr int PROT_NONE  = 0x00;
constexpr int PROT_READ  = 0x01;
constexpr int PROT_WRITE = 0x02;
constexpr int PROT_EXEC  = 0x04;

// ========== Seek Whence ==========

#ifndef SEEK_SET
constexpr int SEEK_SET  = 0;  // Seek from beginning of file
#endif
#ifndef SEEK_CUR
constexpr int SEEK_CUR  = 1;  // Seek from current position
#endif
#ifndef SEEK_END
constexpr int SEEK_END  = 2;  // Seek from end of file
#endif

// ========== Wait Options ==========

#ifndef WNOHANG
constexpr int WNOHANG   = 1;  // Don't block if no child has exited
#endif

// ========== System Call Handler Type ==========

typedef int64_t (*syscall_handler_t)(uint64_t arg1, uint64_t arg2, 
                                      uint64_t arg3, uint64_t arg4,
                                      uint64_t arg5, uint64_t arg6);

// ========== System Call Function ==========

/**
 * Main syscall entry point.
 * Applications call this to invoke system calls.
 */
extern int64_t syscall(int syscall_num, 
                       uint64_t arg1 = 0, uint64_t arg2 = 0, uint64_t arg3 = 0,
                       uint64_t arg4 = 0, uint64_t arg5 = 0, uint64_t arg6 = 0);

// ========== Convenience Wrappers ==========

/**
 * Process management wrappers
 */
inline int64_t fork() {
    return syscall(SYS_FORK);
}

inline int64_t execve(const char* path, const char* const* argv, const char* const* envp) {
    return syscall(SYS_EXEC, (uint64_t)path, (uint64_t)argv, (uint64_t)envp);
}

inline int64_t exec(const char* path) {
    return execve(path, nullptr, nullptr);
}

inline int64_t exit(int status) {
    return syscall(SYS_EXIT, status);
}

inline int64_t wait(int* status) {
    return syscall(SYS_WAIT, (uint64_t)status);
}

inline int64_t getpid() {
    return syscall(SYS_GETPID);
}

inline int64_t getppid() {
    return syscall(SYS_GETPPID);
}

inline int64_t kill(int pid, int sig) {
    return syscall(SYS_KILL, pid, sig);
}

/**
 * File I/O wrappers
 */
inline int64_t open(const char* path, int flags) {
    return syscall(SYS_OPEN, (uint64_t)path, flags);
}

inline int64_t close(int fd) {
    return syscall(SYS_CLOSE, fd);
}

inline int64_t read(int fd, void* buf, size_t count) {
    return syscall(SYS_READ, fd, (uint64_t)buf, count);
}

inline int64_t write(int fd, const void* buf, size_t count) {
    return syscall(SYS_WRITE, fd, (uint64_t)buf, count);
}

inline int64_t lseek(int fd, int64_t offset, int whence) {
    return syscall(SYS_LSEEK, fd, offset, whence);
}

inline int64_t unlink(const char* path) {
    return syscall(SYS_UNLINK, (uint64_t)path);
}

inline int64_t list(const char* path, void* buf, size_t count) {
    return syscall(SYS_LIST, (uint64_t)path, (uint64_t)buf, count);
}

/**
 * Memory management wrappers
 */
inline int64_t brk(void* addr) {
    return syscall(SYS_BRK, (uint64_t)addr);
}

inline int64_t mmap(void* addr, size_t size, int prot) {
    return syscall(SYS_MMAP, (uint64_t)addr, size, prot);
}

inline int64_t munmap(void* addr, size_t size) {
    return syscall(SYS_MUNMAP, (uint64_t)addr, size);
}

inline int64_t mprotect(void* addr, size_t size, int prot) {
    return syscall(SYS_MPROTECT, (uint64_t)addr, size, prot);
}

} // namespace os
