#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#ifdef RSE_KERNEL
#include "KernelStubs.h"
#else
#include <iostream>
#endif

namespace os {

#ifdef RSE_KERNEL
extern "C" int rse_block_read(uint64_t lba, void* buf, uint32_t blocks);
extern "C" int rse_block_write(uint64_t lba, const void* buf, uint32_t blocks);
extern "C" uint64_t rse_block_total_blocks(void);
#else
struct BlockMemoryStore {
    uint32_t block_size;
    uint64_t total_blocks;
    uint8_t* data;

    BlockMemoryStore() : block_size(512u), total_blocks(8192u), data(nullptr) {}
};

inline BlockMemoryStore& block_store() {
    static BlockMemoryStore store;
    return store;
}

inline bool ensure_block_store() {
    BlockMemoryStore& store = block_store();
    if (store.data) {
        return true;
    }
    uint64_t bytes = store.total_blocks * store.block_size;
    store.data = new (std::nothrow) uint8_t[bytes];
    if (!store.data) {
        return false;
    }
    std::memset(store.data, 0, bytes);
    return true;
}

inline void rse_block_configure(uint32_t block_size, uint64_t total_blocks) {
    if (block_size == 0 || total_blocks == 0) {
        return;
    }
    BlockMemoryStore& store = block_store();
    if (store.data) {
        delete[] store.data;
        store.data = nullptr;
    }
    store.block_size = block_size;
    store.total_blocks = total_blocks;
    (void)ensure_block_store();
}

inline int rse_block_read(uint64_t lba, void* buf, uint32_t blocks) {
    if (!buf || blocks == 0) {
        return -1;
    }
    if (!ensure_block_store()) {
        return -1;
    }
    BlockMemoryStore& store = block_store();
    if (lba + blocks > store.total_blocks) {
        return -1;
    }
    uint64_t offset = lba * store.block_size;
    uint64_t bytes = (uint64_t)blocks * store.block_size;
    std::memcpy(buf, store.data + offset, bytes);
    return 0;
}

inline int rse_block_write(uint64_t lba, const void* buf, uint32_t blocks) {
    if (!buf || blocks == 0) {
        return -1;
    }
    if (!ensure_block_store()) {
        return -1;
    }
    BlockMemoryStore& store = block_store();
    if (lba + blocks > store.total_blocks) {
        return -1;
    }
    uint64_t offset = lba * store.block_size;
    uint64_t bytes = (uint64_t)blocks * store.block_size;
    std::memcpy(store.data + offset, buf, bytes);
    return 0;
}

inline uint64_t rse_block_total_blocks(void) {
    if (!ensure_block_store()) {
        return 0;
    }
    return block_store().total_blocks;
}
#endif

struct BlockFSEntry {
    char name[32];
    uint32_t size;
    uint32_t slot_index;
    uint8_t in_use;
    uint8_t reserved[3];
};

struct BlockFSHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t slot_size;
    uint32_t max_files;
    uint32_t table_blocks;
    uint64_t start_lba;
    uint64_t data_start_lba;
    uint64_t region_blocks;
    uint32_t reserved[6];
};

class BlockFS {
public:
    static constexpr uint32_t kMagic = 0x52534501u;
    static constexpr uint32_t kVersion = 1u;
    static constexpr uint32_t kMaxFiles = 256u;
    static constexpr uint32_t kSlotBytes = 16384u;
    static constexpr uint32_t kNameMax = 31u;
    static constexpr uint32_t kGptGuardBlocks = 34u;

    BlockFS() : mounted_(false), block_size_(0), slot_size_(0),
                slot_blocks_(0), table_blocks_(0), start_lba_(0),
                data_start_lba_(0), region_blocks_(0) {
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            entries_[i].name[0] = '\0';
            entries_[i].size = 0;
            entries_[i].slot_index = i;
            entries_[i].in_use = 0;
        }
        std::memset(&header_, 0, sizeof(header_));
    }

    bool mount(uint32_t block_size, uint64_t total_blocks) {
        if (block_size == 0 || total_blocks == 0) {
            return false;
        }
        if (block_size > 4096u) {
            return false;
        }
        block_size_ = block_size;
        slot_blocks_ = (kSlotBytes + block_size - 1u) / block_size;
        slot_size_ = slot_blocks_ * block_size;
        table_blocks_ = blocks_for_bytes(sizeof(BlockFSEntry) * kMaxFiles, block_size);
        region_blocks_ = 1 + table_blocks_ + (uint64_t)slot_blocks_ * kMaxFiles;
        if (total_blocks <= region_blocks_ + kGptGuardBlocks + 1) {
            return false;
        }
        start_lba_ = total_blocks - region_blocks_ - kGptGuardBlocks;
        data_start_lba_ = start_lba_ + 1 + table_blocks_;

        BlockFSHeader on_disk = {};
        if (!read_header(on_disk)) {
            return false;
        }
        if (is_valid_header(on_disk)) {
            header_ = on_disk;
            if (!load_entries()) {
                return false;
            }
            mounted_ = true;
            return true;
        }

        init_fresh();
        mounted_ = true;
        return true;
    }

    bool isMounted() const {
        return mounted_;
    }

    BlockFSEntry* open(const char* name, bool create) {
        if (!mounted_ || !name || name[0] == '\0') {
            return nullptr;
        }
        BlockFSEntry* entry = find_entry(name);
        if (entry) {
            return entry;
        }
        if (!create) {
            return nullptr;
        }
        BlockFSEntry* free_slot = find_free();
        if (!free_slot) {
            return nullptr;
        }
        write_name(free_slot->name, name);
        free_slot->size = 0;
        free_slot->in_use = 1;
        sync_entries();
        return free_slot;
    }

    int64_t read(BlockFSEntry* entry, uint64_t offset, uint8_t* buf, uint32_t count) {
        if (!mounted_ || !entry || !buf) {
            return -1;
        }
        if (!entry->in_use) {
            return -1;
        }
        if (offset >= entry->size) {
            return 0;
        }
        uint32_t available = entry->size - (uint32_t)offset;
        uint32_t to_read = (count < available) ? count : available;
        if (to_read == 0) {
            return 0;
        }
        uint64_t file_off = offset;
        uint64_t lba_base = data_start_lba_ + (uint64_t)entry->slot_index * slot_blocks_;
        return block_read_at(lba_base, file_off, buf, to_read);
    }

    int64_t write(BlockFSEntry* entry, uint64_t offset, const uint8_t* buf, uint32_t count) {
        if (!mounted_ || !entry || !buf) {
            return -1;
        }
        if (!entry->in_use) {
            return -1;
        }
        if (offset >= slot_size_) {
            return 0;
        }
        uint32_t remaining = count;
        uint64_t max_len = slot_size_ - offset;
        if (remaining > max_len) {
            remaining = (uint32_t)max_len;
        }
        if (remaining == 0) {
            return 0;
        }
        uint64_t file_off = offset;
        uint64_t lba_base = data_start_lba_ + (uint64_t)entry->slot_index * slot_blocks_;
        int64_t wrote = block_write_at(lba_base, file_off, buf, remaining);
        if (wrote > 0) {
            uint64_t new_size = offset + (uint64_t)wrote;
            if (new_size > entry->size) {
                entry->size = (uint32_t)new_size;
                sync_entries();
            }
        }
        return wrote;
    }

    int truncate(BlockFSEntry* entry) {
        if (!mounted_ || !entry) {
            return -1;
        }
        if (!entry->in_use) {
            return -1;
        }
        entry->size = 0;
        sync_entries();
        return 0;
    }

    bool remove(const char* name) {
        if (!mounted_ || !name) {
            return false;
        }
        BlockFSEntry* entry = find_entry(name);
        if (!entry) {
            return false;
        }
        entry->name[0] = '\0';
        entry->size = 0;
        entry->in_use = 0;
        sync_entries();
        return true;
    }

    void printStats() const {
        if (!mounted_) {
            std::cout << "[BlockFS] not mounted" << std::endl;
            return;
        }
        uint32_t used = 0;
        uint64_t bytes = 0;
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (entries_[i].in_use) {
                used++;
                bytes += entries_[i].size;
            }
        }
        std::cout << "[BlockFS] files=" << used
                  << " bytes=" << bytes
                  << " slot_bytes=" << slot_size_
                  << std::endl;
    }

    uint32_t list(char* out, uint32_t max) const {
        if (!mounted_ || !out || max == 0) {
            return 0;
        }
        uint32_t written = 0;
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!entries_[i].in_use) {
                continue;
            }
            const char* name = entries_[i].name;
            uint32_t j = 0;
            while (name[j] && written + 1 < max) {
                out[written++] = name[j++];
            }
            if (written + 1 >= max) {
                break;
            }
            out[written++] = '\n';
        }
        out[written] = '\0';
        return written;
    }

private:
    bool mounted_;
    uint32_t block_size_;
    uint32_t slot_size_;
    uint32_t slot_blocks_;
    uint32_t table_blocks_;
    uint64_t start_lba_;
    uint64_t data_start_lba_;
    uint64_t region_blocks_;
    BlockFSHeader header_;
    BlockFSEntry entries_[kMaxFiles];

    static uint32_t blocks_for_bytes(uint32_t bytes, uint32_t block_size) {
        return (bytes + block_size - 1u) / block_size;
    }

    bool is_valid_header(const BlockFSHeader& hdr) const {
        return hdr.magic == kMagic &&
               hdr.version == kVersion &&
               hdr.block_size == block_size_ &&
               hdr.slot_size == slot_size_ &&
               hdr.max_files == kMaxFiles &&
               hdr.table_blocks == table_blocks_;
    }

    void init_fresh() {
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            entries_[i].name[0] = '\0';
            entries_[i].size = 0;
            entries_[i].slot_index = i;
            entries_[i].in_use = 0;
        }
        header_.magic = kMagic;
        header_.version = kVersion;
        header_.block_size = block_size_;
        header_.slot_size = slot_size_;
        header_.max_files = kMaxFiles;
        header_.table_blocks = table_blocks_;
        header_.start_lba = start_lba_;
        header_.data_start_lba = data_start_lba_;
        header_.region_blocks = region_blocks_;
        sync_header();
        sync_entries();
    }

    bool read_header(BlockFSHeader& out) {
        uint8_t* scratch = new uint8_t[block_size_];
        if (!scratch) {
            return false;
        }
        if (rse_block_read(start_lba_, scratch, 1) != 0) {
            delete[] scratch;
            return false;
        }
        std::memcpy(&out, scratch, sizeof(BlockFSHeader));
        delete[] scratch;
        return true;
    }

    void sync_header() {
        uint8_t* scratch = new uint8_t[block_size_];
        if (!scratch) {
            return;
        }
        std::memset(scratch, 0, block_size_);
        std::memcpy(scratch, &header_, sizeof(BlockFSHeader));
        rse_block_write(start_lba_, scratch, 1);
        delete[] scratch;
    }

    bool load_entries() {
        uint32_t bytes = sizeof(BlockFSEntry) * kMaxFiles;
        uint32_t blocks = table_blocks_;
        uint8_t* scratch = new uint8_t[(size_t)blocks * block_size_];
        if (!scratch) {
            return false;
        }
        if (rse_block_read(start_lba_ + 1, scratch, blocks) != 0) {
            delete[] scratch;
            return false;
        }
        std::memcpy(entries_, scratch, bytes);
        delete[] scratch;
        return true;
    }

    void sync_entries() {
        uint32_t bytes = sizeof(BlockFSEntry) * kMaxFiles;
        uint32_t blocks = table_blocks_;
        uint8_t* scratch = new uint8_t[(size_t)blocks * block_size_];
        if (!scratch) {
            return;
        }
        std::memset(scratch, 0, (size_t)blocks * block_size_);
        std::memcpy(scratch, entries_, bytes);
        rse_block_write(start_lba_ + 1, scratch, blocks);
        delete[] scratch;
    }

    BlockFSEntry* find_entry(const char* name) {
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (entries_[i].in_use && name_equal(entries_[i].name, name)) {
                return &entries_[i];
            }
        }
        return nullptr;
    }

    BlockFSEntry* find_free() {
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!entries_[i].in_use) {
                entries_[i].slot_index = i;
                return &entries_[i];
            }
        }
        return nullptr;
    }

    static void write_name(char* dst, const char* src) {
        uint32_t i = 0;
        for (; src[i] && i < kNameMax; ++i) {
            dst[i] = src[i];
        }
        dst[i] = '\0';
    }

    static bool name_equal(const char* a, const char* b) {
        uint32_t i = 0;
        while (a[i] || b[i]) {
            if (a[i] != b[i]) {
                return false;
            }
            ++i;
        }
        return true;
    }

    int64_t block_read_at(uint64_t base_lba, uint64_t offset, uint8_t* buf, uint32_t count) {
        uint64_t lba = base_lba + (offset / block_size_);
        uint32_t block_off = (uint32_t)(offset % block_size_);
        uint32_t remaining = count;
        uint8_t* out = buf;

        uint8_t* scratch = nullptr;
        if (block_off != 0 || (remaining % block_size_) != 0) {
            scratch = new uint8_t[block_size_];
            if (!scratch) {
                return -1;
            }
        }

        if (block_off != 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -1;
            }
            uint32_t take = block_size_ - block_off;
            if (take > remaining) {
                take = remaining;
            }
            std::memcpy(out, scratch + block_off, take);
            out += take;
            remaining -= take;
            lba += 1;
        }

        uint32_t full_bytes = (remaining / block_size_) * block_size_;
        if (full_bytes > 0) {
            uint32_t blocks = full_bytes / block_size_;
            if (rse_block_read(lba, out, blocks) != 0) {
                delete[] scratch;
                return -1;
            }
            out += full_bytes;
            remaining -= full_bytes;
            lba += blocks;
        }

        if (remaining > 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -1;
            }
            std::memcpy(out, scratch, remaining);
            remaining = 0;
        }

        delete[] scratch;
        return count;
    }

    int64_t block_write_at(uint64_t base_lba, uint64_t offset, const uint8_t* buf, uint32_t count) {
        uint64_t lba = base_lba + (offset / block_size_);
        uint32_t block_off = (uint32_t)(offset % block_size_);
        uint32_t remaining = count;
        const uint8_t* in = buf;

        uint8_t* scratch = nullptr;
        if (block_off != 0 || (remaining % block_size_) != 0) {
            scratch = new uint8_t[block_size_];
            if (!scratch) {
                return -1;
            }
        }

        if (block_off != 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -1;
            }
            uint32_t take = block_size_ - block_off;
            if (take > remaining) {
                take = remaining;
            }
            std::memcpy(scratch + block_off, in, take);
            if (rse_block_write(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -1;
            }
            in += take;
            remaining -= take;
            lba += 1;
        }

        uint32_t full_bytes = (remaining / block_size_) * block_size_;
        if (full_bytes > 0) {
            uint32_t blocks = full_bytes / block_size_;
            if (rse_block_write(lba, in, blocks) != 0) {
                delete[] scratch;
                return -1;
            }
            in += full_bytes;
            remaining -= full_bytes;
            lba += blocks;
        }

        if (remaining > 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -1;
            }
            std::memcpy(scratch, in, remaining);
            if (rse_block_write(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -1;
            }
            remaining = 0;
        }

        delete[] scratch;
        return count;
    }
};

} // namespace os
