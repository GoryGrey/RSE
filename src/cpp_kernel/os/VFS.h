#pragma once

#include "FileDescriptor.h"
#include "MemFS.h"
#include "Device.h"
#include "BlockDevice.h"
#include "BlockFS.h"
#include <cstdint>
#ifdef RSE_KERNEL
#include "KernelStubs.h"
#else
#include <iostream>
#endif

/**
 * Virtual File System (VFS) for Braided OS
 * 
 * Provides unified interface for file operations.
 */

namespace os {

class VFS {
private:
    MemFS* fs_;
    FileDescriptorTable* fd_table_;
    DeviceManager* device_mgr_;
    BlockFS* blockfs_;
    
public:
    VFS(MemFS* fs, FileDescriptorTable* fd_table)
        : fs_(fs), fd_table_(fd_table), device_mgr_(nullptr), blockfs_(nullptr) {}

    void setDeviceManager(DeviceManager* mgr) {
        device_mgr_ = mgr;
    }

    void setBlockFS(BlockFS* fs) {
        blockfs_ = fs;
    }

    Device* lookupDevice(const char* path) const {
        if (!device_mgr_ || !path) {
            return nullptr;
        }
        const char* prefix = "/dev/";
        for (int i = 0; prefix[i] != '\0'; ++i) {
            if (path[i] != prefix[i]) {
                return nullptr;
            }
        }
        const char* name = path + 5;
        if (name[0] == '\0') {
            return nullptr;
        }
        return device_mgr_->lookup(name);
    }

    const char* persistName(const char* path) const {
        if (!path) {
            return nullptr;
        }
        const char* prefix = "/persist/";
        for (int i = 0; prefix[i] != '\0'; ++i) {
            if (path[i] != prefix[i]) {
                return nullptr;
            }
        }
        const char* name = path + 9;
        if (name[0] == '\0') {
            return nullptr;
        }
        return name;
    }
    
    /**
     * Open a file.
     * 
     * @param path File path
     * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND)
     * @param mode Permissions (if creating)
     * @return File descriptor, or -1 on error
     */
    int32_t open(const char* path, uint32_t flags, uint32_t mode = 0644) {
        // Device nodes (/dev/*)
        Device* dev = lookupDevice(path);
        if (dev) {
            if (dev->open) {
                dev->open(dev);
            }
            int32_t fd = fd_table_->allocateDevice(dev, flags);
            if (fd < 0) {
                std::cerr << "[VFS] Failed to allocate device FD" << std::endl;
                return -1;
            }
            return fd;
        }

        const char* persist = persistName(path);
        if (persist) {
            if (!blockfs_ || !blockfs_->isMounted()) {
                std::cerr << "[VFS] BlockFS not mounted" << std::endl;
                return -1;
            }
            BlockFSEntry* entry = blockfs_->open(persist, (flags & O_CREAT) != 0);
            if (!entry) {
                std::cerr << "[VFS] BlockFS open failed: " << persist << std::endl;
                return -1;
            }
            if (flags & O_TRUNC) {
                blockfs_->truncate(entry);
            }
            int32_t fd = fd_table_->allocateBlock(entry, flags);
            if (fd < 0) {
                std::cerr << "[VFS] Failed to allocate BlockFS FD" << std::endl;
                return -1;
            }
            if (flags & O_APPEND) {
                FileDescriptor* desc = fd_table_->get(fd);
                if (desc) {
                    desc->offset = entry->size;
                }
            }
            return fd;
        }

        // Look up file
        MemFSFile* file = fs_->lookup(path);
        
        // Create if doesn't exist and O_CREAT is set
        if (!file && (flags & O_CREAT)) {
            file = fs_->create(path, mode);
            if (!file) {
                std::cerr << "[VFS] Failed to create file: " << path << std::endl;
                return -1;
            }
        }
        
        // File not found
        if (!file) {
            std::cerr << "[VFS] File not found: " << path << std::endl;
            return -1;
        }
        
        // Truncate if O_TRUNC is set
        if (flags & O_TRUNC) {
            file->truncate();
        }
        
        // Allocate file descriptor
        int32_t fd = fd_table_->allocate(file, flags);
        if (fd < 0) {
            std::cerr << "[VFS] Failed to allocate FD" << std::endl;
            return -1;
        }
        
        // If O_APPEND, set offset to end
        if (flags & O_APPEND) {
            FileDescriptor* desc = fd_table_->get(fd);
            if (desc) {
                desc->offset = file->size;
            }
        }
        
        return fd;
    }
    
    /**
     * Read from a file.
     * 
     * @param fd File descriptor
     * @param buf Buffer to read into
     * @param count Number of bytes to read
     * @return Number of bytes read, or -1 on error
     */
    int64_t read(int32_t fd, void* buf, uint32_t count) {
        // Get file descriptor
        FileDescriptor* desc = fd_table_->get(fd);
        if (!desc) {
            std::cerr << "[VFS] Invalid FD: " << fd << std::endl;
            return -1;
        }

        if (desc->isBlockFile()) {
            if (!blockfs_ || !desc->block_file) {
                return -1;
            }
            int64_t bytes_read = blockfs_->read(desc->block_file, desc->offset,
                                                (uint8_t*)buf, count);
            if (bytes_read < 0) {
                return -1;
            }
            desc->offset += (uint64_t)bytes_read;
            return bytes_read;
        }

        if (desc->isDevice()) {
            if (!desc->device) {
                return -1;
            }
            if (desc->device->type == DeviceType::BLOCK) {
                BlockDeviceData* data = (BlockDeviceData*)desc->device->private_data;
                if (!data || data->block_size == 0) {
                    return -1;
                }
                uint64_t block_size = data->block_size;
                uint64_t offset = desc->offset;
                uint8_t* out = (uint8_t*)buf;
                uint32_t remaining = count;
                if (remaining == 0) {
                    return 0;
                }

                uint8_t* scratch = nullptr;
                if ((offset % block_size) != 0 || (remaining % block_size) != 0) {
                    scratch = new uint8_t[block_size];
                    if (!scratch) {
                        return -ENOMEM;
                    }
                }

                // Leading partial block.
                if ((offset % block_size) != 0) {
                    uint64_t lba = offset / block_size;
                    if (rse_block_read(lba, scratch, 1) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    uint32_t block_off = (uint32_t)(offset % block_size);
                    uint32_t take = (uint32_t)(block_size - block_off);
                    if (take > remaining) {
                        take = remaining;
                    }
                    std::memcpy(out, scratch + block_off, take);
                    offset += take;
                    out += take;
                    remaining -= take;
                }

                // Full blocks.
                uint32_t full_bytes = (uint32_t)((remaining / block_size) * block_size);
                if (full_bytes > 0) {
                    uint64_t lba = offset / block_size;
                    uint32_t blocks = full_bytes / block_size;
                    if (rse_block_read(lba, out, blocks) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    offset += full_bytes;
                    out += full_bytes;
                    remaining -= full_bytes;
                }

                // Trailing partial block.
                if (remaining > 0) {
                    uint64_t lba = offset / block_size;
                    if (rse_block_read(lba, scratch, 1) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    std::memcpy(out, scratch, remaining);
                    offset += remaining;
                    out += remaining;
                    remaining = 0;
                }

                delete[] scratch;
                desc->offset = offset;
                return count;
            }
            if (!desc->device->read) {
                return -1;
            }
            return desc->device->read(desc->device, buf, count);
        }
        
        // Check if readable
        if (!desc->isReadable()) {
            std::cerr << "[VFS] FD not readable: " << fd << std::endl;
            return -1;
        }
        
        // Read from file
        int64_t bytes_read = desc->file->read((uint8_t*)buf, desc->offset, count);
        if (bytes_read < 0) {
            return -1;
        }
        
        // Update offset
        desc->offset += bytes_read;
        
        return bytes_read;
    }
    
    /**
     * Write to a file.
     * 
     * @param fd File descriptor
     * @param buf Buffer to write from
     * @param count Number of bytes to write
     * @return Number of bytes written, or -1 on error
     */
    int64_t write(int32_t fd, const void* buf, uint32_t count) {
        // Get file descriptor
        FileDescriptor* desc = fd_table_->get(fd);
        if (!desc) {
            std::cerr << "[VFS] Invalid FD: " << fd << std::endl;
            return -1;
        }

        if (desc->isBlockFile()) {
            if (!blockfs_ || !desc->block_file) {
                return -1;
            }
            int64_t bytes_written = blockfs_->write(desc->block_file, desc->offset,
                                                   (const uint8_t*)buf, count);
            if (bytes_written < 0) {
                return -1;
            }
            desc->offset += (uint64_t)bytes_written;
            return bytes_written;
        }

        if (desc->isDevice()) {
            if (!desc->device) {
                return -1;
            }
            if (desc->device->type == DeviceType::BLOCK) {
                BlockDeviceData* data = (BlockDeviceData*)desc->device->private_data;
                if (!data || data->block_size == 0) {
                    return -1;
                }
                uint64_t block_size = data->block_size;
                uint64_t offset = desc->offset;
                const uint8_t* in = (const uint8_t*)buf;
                uint32_t remaining = count;
                if (remaining == 0) {
                    return 0;
                }

                uint8_t* scratch = nullptr;
                if ((offset % block_size) != 0 || (remaining % block_size) != 0) {
                    scratch = new uint8_t[block_size];
                    if (!scratch) {
                        return -ENOMEM;
                    }
                }

                // Leading partial block (read-modify-write).
                if ((offset % block_size) != 0) {
                    uint64_t lba = offset / block_size;
                    if (rse_block_read(lba, scratch, 1) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    uint32_t block_off = (uint32_t)(offset % block_size);
                    uint32_t take = (uint32_t)(block_size - block_off);
                    if (take > remaining) {
                        take = remaining;
                    }
                    std::memcpy(scratch + block_off, in, take);
                    if (rse_block_write(lba, scratch, 1) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    offset += take;
                    in += take;
                    remaining -= take;
                }

                // Full blocks.
                uint32_t full_bytes = (uint32_t)((remaining / block_size) * block_size);
                if (full_bytes > 0) {
                    uint64_t lba = offset / block_size;
                    uint32_t blocks = full_bytes / block_size;
                    if (rse_block_write(lba, in, blocks) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    offset += full_bytes;
                    in += full_bytes;
                    remaining -= full_bytes;
                }

                // Trailing partial block (read-modify-write).
                if (remaining > 0) {
                    uint64_t lba = offset / block_size;
                    if (rse_block_read(lba, scratch, 1) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    std::memcpy(scratch, in, remaining);
                    if (rse_block_write(lba, scratch, 1) != 0) {
                        delete[] scratch;
                        return -EIO;
                    }
                    offset += remaining;
                    remaining = 0;
                }

                delete[] scratch;
                desc->offset = offset;
                return count;
            }
            if (!desc->device->write) {
                return -1;
            }
            return desc->device->write(desc->device, buf, count);
        }
        
        // Check if writable
        if (!desc->isWritable()) {
            std::cerr << "[VFS] FD not writable: " << fd << std::endl;
            return -1;
        }
        
        // Write to file
        int64_t bytes_written = desc->file->write((const uint8_t*)buf, desc->offset, count);
        if (bytes_written < 0) {
            return -1;
        }
        
        // Update offset
        desc->offset += bytes_written;
        
        return bytes_written;
    }
    
    /**
     * Close a file descriptor.
     * 
     * @param fd File descriptor
     * @return 0 on success, -1 on error
     */
    int32_t close(int32_t fd) {
        FileDescriptor* desc = fd_table_->get(fd);
        if (!desc) {
            std::cerr << "[VFS] Invalid FD: " << fd << std::endl;
            return -1;
        }

        if (desc->isDevice() && desc->device && desc->device->close) {
            desc->device->close(desc->device);
        }
        
        fd_table_->free(fd);
        
        return 0;
    }
    
    /**
     * Seek to a position in a file.
     * 
     * @param fd File descriptor
     * @param offset Offset to seek to
     * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
     * @return New offset, or -1 on error
     */
    int64_t lseek(int32_t fd, int64_t offset, int whence) {
        FileDescriptor* desc = fd_table_->get(fd);
        if (!desc) {
            std::cerr << "[VFS] Invalid FD: " << fd << std::endl;
            return -1;
        }
        if (desc->isBlockFile()) {
            int64_t new_offset = 0;
            uint64_t file_size = desc->block_file ? desc->block_file->size : 0;
            switch (whence) {
                case SEEK_SET:
                    new_offset = offset;
                    break;
                case SEEK_CUR:
                    new_offset = (int64_t)desc->offset + offset;
                    break;
                case SEEK_END:
                    new_offset = (int64_t)file_size + offset;
                    break;
                default:
                    return -EINVAL;
            }
            if (new_offset < 0) {
                new_offset = 0;
            }
            desc->offset = (uint64_t)new_offset;
            return new_offset;
        }
        if (desc->isDevice()) {
            if (desc->device && desc->device->type == DeviceType::BLOCK) {
                int64_t new_offset = 0;
                switch (whence) {
                    case SEEK_SET:
                        new_offset = offset;
                        break;
                    case SEEK_CUR:
                        new_offset = (int64_t)desc->offset + offset;
                        break;
                    case SEEK_END:
                        new_offset = (int64_t)desc->offset + offset;
                        break;
                    default:
                        return -EINVAL;
                }
                if (new_offset < 0) {
                    new_offset = 0;
                }
                desc->offset = (uint64_t)new_offset;
                return new_offset;
            }
            return -EINVAL;
        }
        
        int64_t new_offset = 0;
        
        switch (whence) {
            case SEEK_SET:
                new_offset = offset;
                break;
            
            case SEEK_CUR:
                new_offset = desc->offset + offset;
                break;
            
            case SEEK_END:
                new_offset = desc->file->size + offset;
                break;
            
            default:
                std::cerr << "[VFS] Invalid whence: " << whence << std::endl;
                return -1;
        }
        
        // Check bounds
        if (new_offset < 0) {
            new_offset = 0;
        }
        
        desc->offset = new_offset;
        
        return new_offset;
    }
    
    /**
     * Delete a file.
     * 
     * @param path File path
     * @return 0 on success, -1 on error
     */
    int32_t unlink(const char* path) {
        const char* persist = persistName(path);
        if (persist) {
            if (!blockfs_ || !blockfs_->isMounted()) {
                return -1;
            }
            return blockfs_->remove(persist) ? 0 : -1;
        }
        if (lookupDevice(path)) {
            return -EINVAL;
        }
        if (fs_->remove(path)) {
            return 0;
        }
        return -1;
    }

    int32_t list(const char* path, char* buf, uint32_t max) const {
        if (!buf || max == 0) {
            return -EINVAL;
        }
        if (path && (strcmp(path, "/persist") == 0 || strcmp(path, "/persist/") == 0)) {
            if (!blockfs_ || !blockfs_->isMounted()) {
                return -1;
            }
            return (int32_t)blockfs_->list(buf, max);
        }
        return (int32_t)fs_->list(buf, max);
    }
    
    /**
     * Print VFS statistics.
     */
    void printStats() const {
        fs_->printStats();
        fd_table_->printStats();
        if (blockfs_) {
            blockfs_->printStats();
        }
    }
};

} // namespace os
