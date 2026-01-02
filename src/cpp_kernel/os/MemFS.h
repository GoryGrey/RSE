#pragma once

#include "FileDescriptor.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#ifdef RSE_KERNEL
#include "KernelStubs.h"
extern "C" {
void* malloc(size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
}
#else
#include <iostream>
#endif

/**
 * Simple In-Memory File System (MemFS)
 * 
 * Files stored in RAM, no persistence.
 * Supports nested directories in a single namespace.
 */

namespace os {

/**
 * In-memory file.
 */
struct MemFSFile {
    char name[256];            // File name
    uint8_t* data;             // File contents
    uint32_t size;             // Current size
    uint32_t capacity;         // Allocated capacity
    uint32_t mode;             // Permissions
    uint16_t uid;              // Owner user id
    uint16_t gid;              // Owner group id
    bool in_use;               // Is this file slot used?
    bool is_dir;               // Directory entry
    
    MemFSFile() 
        : data(nullptr),
          size(0),
          capacity(0),
          mode(0644),
          uid(0),
          gid(0),
          in_use(false),
          is_dir(false) {
        name[0] = '\0';
    }
    
    ~MemFSFile() {
        if (data) {
            free(data);
            data = nullptr;
        }
    }
    
    /**
     * Ensure capacity for at least `new_size` bytes.
     */
    bool ensureCapacity(uint32_t new_size) {
        if (new_size <= capacity) {
            return true;
        }
        
        // Round up to next power of 2
        uint32_t new_capacity = capacity;
        if (new_capacity == 0) {
            new_capacity = 4096;  // Start with 4KB
        }
        
        while (new_capacity < new_size) {
            new_capacity *= 2;
        }
        
        // Reallocate
        uint8_t* new_data = (uint8_t*)realloc(data, new_capacity);
        if (!new_data) {
            std::cerr << "[MemFSFile] Out of memory!" << std::endl;
            return false;
        }
        
        data = new_data;
        capacity = new_capacity;
        
        return true;
    }
    
    /**
     * Read from file at offset.
     */
    int64_t read(uint8_t* buf, uint64_t offset, uint32_t count) {
        if (offset >= size) {
            return 0;  // EOF
        }
        
        uint32_t available = size - offset;
        uint32_t to_read = (count < available) ? count : available;
        
        std::memcpy(buf, data + offset, to_read);
        
        return to_read;
    }
    
    /**
     * Write to file at offset.
     */
    int64_t write(const uint8_t* buf, uint64_t offset, uint32_t count) {
        // Ensure capacity
        uint32_t new_size = offset + count;
        if (!ensureCapacity(new_size)) {
            return -1;  // Out of memory
        }
        
        // Write data
        std::memcpy(data + offset, buf, count);
        
        // Update size if we wrote past end
        if (new_size > size) {
            size = new_size;
        }
        
        return count;
    }
    
    /**
     * Truncate file to zero length.
     */
    void truncate() {
        size = 0;
    }
};

/**
 * In-memory file system.
 */
class MemFS {
private:
    static constexpr uint32_t kMaxFiles = 1024;
    static constexpr uint32_t kNameMax = 255;
    MemFSFile files_[kMaxFiles];
    uint32_t num_files_;

    static uint32_t default_mode(bool is_dir) {
        return is_dir ? 0755u : 0644u;
    }

    static bool is_valid_path(const char* path, bool allow_root) {
        if (!path || path[0] == '\0') {
            return false;
        }
        if (path[0] != '/') {
            return false;
        }
        if (path[1] == '\0') {
            return allow_root;
        }
        uint32_t len = 0;
        bool in_segment = false;
        const char* seg_start = path + 1;
        uint32_t seg_len = 0;
        for (const char* p = path + 1; ; ++p) {
            char c = *p;
            if (c == '/' || c == '\0') {
                if (!in_segment) {
                    return false;
                }
                if ((seg_len == 1 && seg_start[0] == '.') ||
                    (seg_len == 2 && seg_start[0] == '.' && seg_start[1] == '.')) {
                    return false;
                }
                if (c == '\0') {
                    break;
                }
                in_segment = false;
                seg_start = p + 1;
                seg_len = 0;
            } else {
                in_segment = true;
                seg_len++;
            }
            if (*p == '\\') {
                return false;
            }
            if (++len > kNameMax) {
                return false;
            }
        }
        return in_segment;
    }

    bool normalize_path(const char* path, char* out, uint32_t out_size,
                        bool allow_root) const {
        if (!out || out_size == 0) {
            return false;
        }
        if (!is_valid_path(path, allow_root)) {
            return false;
        }
        if (path[1] == '\0') {
            out[0] = '\0';
            return true;
        }
        const char* start = path + 1;
        size_t len = std::strlen(start);
        if (len >= out_size) {
            return false;
        }
        std::memcpy(out, start, len);
        out[len] = '\0';
        return true;
    }

    MemFSFile* lookup_internal(const char* name) {
        if (!name) {
            return nullptr;
        }
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (files_[i].in_use && std::strcmp(files_[i].name, name) == 0) {
                return &files_[i];
            }
        }
        return nullptr;
    }

    const MemFSFile* lookup_internal(const char* name) const {
        if (!name) {
            return nullptr;
        }
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (files_[i].in_use && std::strcmp(files_[i].name, name) == 0) {
                return &files_[i];
            }
        }
        return nullptr;
    }

    bool parent_dir_exists(const char* name) const {
        if (!name) {
            return false;
        }
        const char* slash = std::strrchr(name, '/');
        if (!slash) {
            return true;
        }
        if (slash == name) {
            return false;
        }
        char parent[kNameMax + 1];
        size_t len = (size_t)(slash - name);
        if (len == 0 || len > kNameMax) {
            return false;
        }
        std::memset(parent, 0, sizeof(parent));
        std::memcpy(parent, name, len);
        parent[len] = '\0';
        const MemFSFile* entry = lookup_internal(parent);
        return entry && entry->in_use && entry->is_dir;
    }

    bool has_children(const char* name) const {
        if (!name) {
            return false;
        }
        size_t len = std::strlen(name);
        if (len == 0) {
            return false;
        }
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!files_[i].in_use) {
                continue;
            }
            const char* entry = files_[i].name;
            if (std::memcmp(entry, name, len) == 0 && entry[len] == '/') {
                return true;
            }
        }
        return false;
    }

public:
    MemFS() : num_files_(0) {}

    bool isValidPath(const char* path, bool allow_root) const {
        return is_valid_path(path, allow_root);
    }

    /**
     * Create a new file.
     * Returns pointer to file, or nullptr if failed.
     */
    MemFSFile* create(const char* path, uint32_t mode, uint16_t uid = 0, uint16_t gid = 0) {
        char name[kNameMax + 1];
        if (!normalize_path(path, name, sizeof(name), false)) {
            return nullptr;
        }
        // Check if file already exists
        MemFSFile* existing = lookup_internal(name);
        if (existing) {
            return existing;  // Already exists
        }
        if (!parent_dir_exists(name)) {
            return nullptr;
        }

        // Find free slot
        for (uint32_t i = 0; i < kMaxFiles; i++) {
            if (!files_[i].in_use) {
                size_t name_len = std::strlen(name);
                if (name_len > kNameMax) {
                    name_len = kNameMax;
                }
                std::memcpy(files_[i].name, name, name_len);
                files_[i].name[name_len] = '\0';
                files_[i].mode = mode != 0 ? mode : default_mode(false);
                files_[i].uid = uid;
                files_[i].gid = gid;
                files_[i].in_use = true;
                files_[i].is_dir = false;
                files_[i].size = 0;
                files_[i].capacity = 0;
                files_[i].data = nullptr;
                num_files_++;

                std::cout << "[MemFS] Created file: /" << name << std::endl;

                return &files_[i];
            }
        }

        std::cerr << "[MemFS] No free file slots!" << std::endl;
        return nullptr;
    }

    /**
     * Look up a file by name.
     * Returns pointer to file, or nullptr if not found.
     */
    MemFSFile* lookup(const char* path) {
        char name[kNameMax + 1];
        if (!normalize_path(path, name, sizeof(name), false)) {
            return nullptr;
        }
        return lookup_internal(name);
    }

    const MemFSFile* lookup(const char* path) const {
        char name[kNameMax + 1];
        if (!normalize_path(path, name, sizeof(name), false)) {
            return nullptr;
        }
        return lookup_internal(name);
    }

    bool mkdir(const char* path, uint32_t mode, uint16_t uid = 0, uint16_t gid = 0) {
        char name[kNameMax + 1];
        if (!normalize_path(path, name, sizeof(name), false)) {
            return false;
        }
        if (lookup_internal(name)) {
            return false;
        }
        if (!parent_dir_exists(name)) {
            return false;
        }
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!files_[i].in_use) {
                size_t name_len = std::strlen(name);
                if (name_len > kNameMax) {
                    name_len = kNameMax;
                }
                std::memcpy(files_[i].name, name, name_len);
                files_[i].name[name_len] = '\0';
                files_[i].mode = mode != 0 ? mode : default_mode(true);
                files_[i].uid = uid;
                files_[i].gid = gid;
                files_[i].in_use = true;
                files_[i].is_dir = true;
                files_[i].size = 0;
                files_[i].capacity = 0;
                files_[i].data = nullptr;
                num_files_++;

                std::cout << "[MemFS] Created dir: /" << name << std::endl;
                return true;
            }
        }
        std::cerr << "[MemFS] No free file slots!" << std::endl;
        return false;
    }

    /**
     * Delete a file.
     */
    bool remove(const char* path) {
        char name[kNameMax + 1];
        if (!normalize_path(path, name, sizeof(name), false)) {
            return false;
        }
        for (uint32_t i = 0; i < kMaxFiles; i++) {
            if (files_[i].in_use && std::strcmp(files_[i].name, name) == 0) {
                if (files_[i].is_dir && has_children(name)) {
                    return false;
                }
                if (files_[i].data) {
                    free(files_[i].data);
                    files_[i].data = nullptr;
                }
                files_[i].in_use = false;
                files_[i].is_dir = false;
                files_[i].size = 0;
                files_[i].capacity = 0;
                files_[i].mode = default_mode(false);
                files_[i].uid = 0;
                files_[i].gid = 0;
                files_[i].name[0] = '\0';
                num_files_--;

                std::cout << "[MemFS] Deleted file: /" << name << std::endl;

                return true;
            }
        }

        std::cerr << "[MemFS] File not found: " << path << std::endl;
        return false;
    }

    /**
     * List all files.
     */
    void list() const {
        std::cout << "[MemFS] Files (" << num_files_ << "):" << std::endl;
        for (uint32_t i = 0; i < kMaxFiles; i++) {
            if (files_[i].in_use) {
                std::cout << "  " << files_[i].name
                          << (files_[i].is_dir ? "/" : "")
                          << " (" << files_[i].size << " bytes)" << std::endl;
            }
        }
    }

    /**
     * Get statistics.
     */
    void printStats() const {
        uint64_t total_size = 0;
        for (uint32_t i = 0; i < kMaxFiles; i++) {
            if (files_[i].in_use) {
                total_size += files_[i].size;
            }
        }

        std::cout << "[MemFS] Files: " << num_files_ << " / " << kMaxFiles
                  << ", Total size: " << (total_size / 1024) << " KB"
                  << std::endl;
    }

    int32_t list(const char* path, char* out, uint32_t max) const {
        if (!out || max == 0) {
            return -EINVAL;
        }
        const char* target = path ? path : "/";
        char prefix[kNameMax + 1];
        const char* prefix_ptr = nullptr;
        uint32_t prefix_len = 0;
        if (std::strcmp(target, "/") != 0 && target[0] != '\0') {
            if (!normalize_path(target, prefix, sizeof(prefix), false)) {
                return -EINVAL;
            }
            const MemFSFile* entry = lookup_internal(prefix);
            if (!entry || !entry->is_dir) {
                return -ENOENT;
            }
            prefix_ptr = prefix;
            prefix_len = (uint32_t)std::strlen(prefix);
        }

        struct SeenEntry {
            char name[kNameMax + 1];
            bool is_dir;
        };
        SeenEntry seen[kMaxFiles];
        uint32_t seen_count = 0;
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!files_[i].in_use) {
                continue;
            }
            const char* name = files_[i].name;
            if (prefix_ptr) {
                if (std::memcmp(name, prefix_ptr, prefix_len) != 0 ||
                    name[prefix_len] != '/') {
                    continue;
                }
                name += prefix_len + 1;
            }
            if (name[0] == '\0') {
                continue;
            }
            const char* slash = std::strchr(name, '/');
            uint32_t seg_len = slash ? (uint32_t)(slash - name)
                                     : (uint32_t)std::strlen(name);
            if (seg_len == 0) {
                continue;
            }
            bool is_dir = slash != nullptr || files_[i].is_dir;
            bool found = false;
            for (uint32_t j = 0; j < seen_count; ++j) {
                if (std::strncmp(seen[j].name, name, seg_len) == 0 &&
                    seen[j].name[seg_len] == '\0') {
                    seen[j].is_dir = seen[j].is_dir || is_dir;
                    found = true;
                    break;
                }
            }
            if (!found && seen_count < kMaxFiles) {
                std::memset(seen[seen_count].name, 0, sizeof(seen[seen_count].name));
                std::memcpy(seen[seen_count].name, name, seg_len);
                seen[seen_count].name[seg_len] = '\0';
                seen[seen_count].is_dir = is_dir;
                ++seen_count;
            }
        }
        for (uint32_t i = 0; i + 1 < seen_count; ++i) {
            for (uint32_t j = i + 1; j < seen_count; ++j) {
                if (std::strcmp(seen[j].name, seen[i].name) < 0) {
                    SeenEntry tmp = seen[i];
                    seen[i] = seen[j];
                    seen[j] = tmp;
                }
            }
        }

        uint32_t written = 0;
        for (uint32_t i = 0; i < seen_count; ++i) {
            const char* name = seen[i].name;
            for (uint32_t j = 0; name[j] && written + 1 < max; ++j) {
                out[written++] = name[j];
            }
            if (seen[i].is_dir && written + 1 < max) {
                out[written++] = '/';
            }
            if (written + 1 >= max) {
                break;
            }
            out[written++] = '\n';
        }
        out[written] = '\0';
        return (int32_t)written;
    }

    bool stat(const char* path, uint32_t* size, bool* is_dir, uint32_t* mode,
              uint16_t* uid, uint16_t* gid) const {
        if (!path || !size || !is_dir || !mode) {
            return false;
        }
        if (std::strcmp(path, "/") == 0 || std::strcmp(path, "") == 0) {
            *size = 0;
            *is_dir = true;
            *mode = 0755u;
            if (uid) {
                *uid = 0;
            }
            if (gid) {
                *gid = 0;
            }
            return true;
        }
        char name[kNameMax + 1];
        if (!normalize_path(path, name, sizeof(name), false)) {
            return false;
        }
        const MemFSFile* entry = lookup_internal(name);
        if (!entry || !entry->in_use) {
            return false;
        }
        *size = entry->is_dir ? 0u : entry->size;
        *is_dir = entry->is_dir;
        *mode = entry->mode != 0 ? entry->mode : default_mode(entry->is_dir);
        if (uid) {
            *uid = entry->uid;
        }
        if (gid) {
            *gid = entry->gid;
        }
        return true;
    }
};

} // namespace os
