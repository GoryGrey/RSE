#pragma once

#include "Syscall.h"
#include <cstdint>
#include <cstring>
#ifdef RSE_KERNEL
#include "KernelStubs.h"
#else
#include <iostream>
#endif

/**
 * File Descriptor Management for Braided OS
 * 
 * Per-process file descriptor table.
 */

namespace os {

// Seek whence (use system defines from unistd.h)
// SEEK_SET, SEEK_CUR, SEEK_END are already defined

// Forward declarations
struct MemFSFile;
struct BlockFSEntry;
struct Device;

enum class FDKind : uint8_t {
    FILE,
    BLOCK_FILE,
    DEVICE
};

/**
 * File Descriptor
 * 
 * Represents an open file in a process.
 */
struct FileDescriptor {
    int32_t fd;                // File descriptor number
    MemFSFile* file;           // Pointer to file
    BlockFSEntry* block_file;  // Pointer to block-backed file
    Device* device;            // Pointer to device
    uint64_t offset;           // Current read/write position
    uint32_t flags;            // Open flags (O_RDONLY, O_WRONLY, etc.)
    uint32_t ref_count;        // Reference count (for dup)
    FDKind kind;               // File vs device
    bool in_use;               // Is this FD allocated?
    
    FileDescriptor() 
        : fd(-1), file(nullptr), block_file(nullptr), device(nullptr), offset(0), flags(0),
          ref_count(0), kind(FDKind::FILE), in_use(false) {}
    
    bool isReadable() const {
        return (flags & O_WRONLY) == 0;
    }
    
    bool isWritable() const {
        return (flags & O_WRONLY) != 0 || (flags & O_RDWR) != 0;
    }

    bool closeOnExec() const {
        return (flags & O_CLOEXEC) != 0;
    }

    void clearCloseOnExec() {
        flags &= ~O_CLOEXEC;
    }
    
    bool isDevice() const {
        return kind == FDKind::DEVICE;
    }

    bool isBlockFile() const {
        return kind == FDKind::BLOCK_FILE;
    }
};

/**
 * File Descriptor Table
 * 
 * Per-process table of open files.
 */
class FileDescriptorTable {
private:
    static constexpr uint32_t MAX_FDS = 1024;
    FileDescriptor fds_[MAX_FDS];
    
public:
    FileDescriptorTable() {
        // Initialize all FDs as unused
        for (uint32_t i = 0; i < MAX_FDS; i++) {
            fds_[i].fd = i;
            fds_[i].in_use = false;
        }
        
        // Reserve FDs 0, 1, 2 for stdin, stdout, stderr
        fds_[0].in_use = true;  // stdin
        fds_[1].in_use = true;  // stdout
        fds_[2].in_use = true;  // stderr
    }
    
    /**
     * Allocate a new file descriptor.
     * Returns FD number, or -1 if no FDs available.
     */
    int32_t allocate(MemFSFile* file, uint32_t flags) {
        // Find first free FD (skip 0, 1, 2)
        for (uint32_t i = 3; i < MAX_FDS; i++) {
            if (!fds_[i].in_use) {
                fds_[i].file = file;
                fds_[i].block_file = nullptr;
                fds_[i].device = nullptr;
                fds_[i].offset = 0;
                fds_[i].flags = flags;
                fds_[i].ref_count = 1;
                fds_[i].kind = FDKind::FILE;
                fds_[i].in_use = true;
                return i;
            }
        }
        
        std::cerr << "[FileDescriptorTable] No free FDs!" << std::endl;
        return -1;
    }

    /**
     * Allocate a new device descriptor.
     */
    int32_t allocateDevice(Device* device, uint32_t flags) {
        if (!device) {
            return -1;
        }
        for (uint32_t i = 3; i < MAX_FDS; i++) {
            if (!fds_[i].in_use) {
                fds_[i].file = nullptr;
                fds_[i].block_file = nullptr;
                fds_[i].device = device;
                fds_[i].offset = 0;
                fds_[i].flags = flags;
                fds_[i].ref_count = 1;
                fds_[i].kind = FDKind::DEVICE;
                fds_[i].in_use = true;
                return i;
            }
        }
        return -1;
    }

    /**
     * Allocate a new block-backed file descriptor.
     */
    int32_t allocateBlock(BlockFSEntry* file, uint32_t flags) {
        if (!file) {
            return -1;
        }
        for (uint32_t i = 3; i < MAX_FDS; i++) {
            if (!fds_[i].in_use) {
                fds_[i].file = nullptr;
                fds_[i].block_file = file;
                fds_[i].device = nullptr;
                fds_[i].offset = 0;
                fds_[i].flags = flags;
                fds_[i].ref_count = 1;
                fds_[i].kind = FDKind::BLOCK_FILE;
                fds_[i].in_use = true;
                return i;
            }
        }
        return -1;
    }
    
    /**
     * Free a file descriptor.
     */
    void free(int32_t fd) {
        if (fd < 0 || static_cast<uint32_t>(fd) >= MAX_FDS) {
            std::cerr << "[FileDescriptorTable] Invalid FD: " << fd << std::endl;
            return;
        }
        
        if (fd <= 2) {
            std::cerr << "[FileDescriptorTable] Cannot close stdin/stdout/stderr!" << std::endl;
            return;
        }
        
        if (!fds_[fd].in_use) {
            std::cerr << "[FileDescriptorTable] FD not in use: " << fd << std::endl;
            return;
        }
        
        fds_[fd].ref_count--;
        if (fds_[fd].ref_count == 0) {
            fds_[fd].file = nullptr;
            fds_[fd].block_file = nullptr;
            fds_[fd].device = nullptr;
            fds_[fd].offset = 0;
            fds_[fd].flags = 0;
            fds_[fd].kind = FDKind::FILE;
            fds_[fd].in_use = false;
        }
    }
    
    /**
     * Get file descriptor by number.
     * Returns nullptr if invalid or not in use.
     */
    FileDescriptor* get(int32_t fd) {
        if (fd < 0 || static_cast<uint32_t>(fd) >= MAX_FDS) {
            return nullptr;
        }
        
        if (!fds_[fd].in_use) {
            return nullptr;
        }
        
        return &fds_[fd];
    }
    
    /**
     * Duplicate a file descriptor (like dup()).
     */
    int32_t duplicate(int32_t old_fd) {
        FileDescriptor* old_desc = get(old_fd);
        if (!old_desc) {
            return -1;
        }
        
        // Find free FD
        for (uint32_t i = 3; i < MAX_FDS; i++) {
            if (!fds_[i].in_use) {
                fds_[i] = *old_desc;
                fds_[i].fd = i;
                fds_[i].ref_count = 1;
                old_desc->ref_count++;
                return i;
            }
        }
        
        return -1;
    }

    void closeOnExec() {
        for (uint32_t i = 3; i < MAX_FDS; i++) {
            if (fds_[i].in_use && fds_[i].closeOnExec()) {
                free(static_cast<int32_t>(i));
            }
        }
    }

    /**
     * Bind stdin/stdout/stderr to a device.
     */
    void bindStandardDevices(Device* device) {
        if (!device) {
            return;
        }
        for (int i = 0; i < 3; i++) {
            fds_[i].file = nullptr;
            fds_[i].block_file = nullptr;
            fds_[i].device = device;
            fds_[i].offset = 0;
            fds_[i].flags = O_RDWR;
            fds_[i].ref_count = 1;
            fds_[i].kind = FDKind::DEVICE;
            fds_[i].in_use = true;
        }
    }
    
    /**
     * Get number of open file descriptors.
     */
    uint32_t count() const {
        uint32_t c = 0;
        for (uint32_t i = 0; i < MAX_FDS; i++) {
            if (fds_[i].in_use) {
                c++;
            }
        }
        return c;
    }
    
    /**
     * Print FD table statistics.
     */
    void printStats() const {
        uint32_t open_fds = count();
        std::cout << "[FileDescriptorTable] Open FDs: " << open_fds 
                  << " / " << MAX_FDS << std::endl;
    }
};

} // namespace os
