#pragma once

#include "Syscall.h"
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
    uint32_t checksum;
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
    uint32_t journal_active;
    uint32_t journal_index;
    uint32_t journal_crc;
    uint32_t reserved0;
    BlockFSEntry journal_entry;
    uint32_t reserved[2];
};

class BlockFS {
public:
    static constexpr uint32_t kMagic = 0x52534501u;
    static constexpr uint32_t kVersion = 3u;
    static constexpr uint32_t kMinVersion = 2u;
    static constexpr uint32_t kMaxFiles = 256u;
    static constexpr uint32_t kSlotBytes = 16384u;
    static constexpr uint32_t kNameMax = 31u;
    static constexpr uint32_t kGptGuardBlocks = 34u;
    enum EntryType : uint8_t {
        kEntryFile = 0,
        kEntryDir = 1
    };

    BlockFS() : mounted_(false), block_size_(0), slot_size_(0),
                slot_blocks_(0), table_blocks_(0), start_lba_(0),
                data_start_lba_(0), region_blocks_(0) {
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            entries_[i].name[0] = '\0';
            entries_[i].size = 0;
            entries_[i].slot_index = i;
            entries_[i].checksum = 0;
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
            apply_journal();
            sanitize_entries();
            if (header_.version < kVersion) {
                header_.version = kVersion;
                clear_journal();
                (void)sync_header();
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

    uint32_t getBlockSize() const {
        return block_size_;
    }

    uint32_t getSlotBlocks() const {
        return slot_blocks_;
    }

    uint64_t getDataStartLba() const {
        return data_start_lba_;
    }

    bool isDirectory(const BlockFSEntry* entry) const {
        return entry && entry->in_use && entry_type(*entry) == kEntryDir;
    }

    uint16_t effectiveMode(const BlockFSEntry* entry) const {
        if (!entry) {
            return 0;
        }
        uint16_t mode = entry_mode(*entry);
        if (mode != 0) {
            return mode;
        }
        return default_mode(entry_type(*entry));
    }

    bool mkdir(const char* name, uint16_t mode) {
        if (!mounted_ || !name || name[0] == '\0') {
            return false;
        }
        if (!is_valid_path(name)) {
            return false;
        }
        if (!parent_dir_exists(name)) {
            return false;
        }
        BlockFSEntry* entry = find_entry(name);
        if (entry) {
            return false;
        }
        BlockFSEntry* free_slot = find_free();
        if (!free_slot) {
            return false;
        }
        write_name(free_slot->name, name);
        free_slot->size = 0;
        free_slot->checksum = 0;
        free_slot->in_use = 1;
        set_entry_type(*free_slot, kEntryDir);
        set_entry_mode(*free_slot, mode != 0 ? mode : default_mode(kEntryDir));
        uint32_t index = (uint32_t)(free_slot - entries_);
        if (!commit_entry(index)) {
            return false;
        }
        return true;
    }

    bool stat(const char* name, uint32_t* size, uint8_t* type, uint16_t* mode) const {
        if (!mounted_ || !name || !size || !type || !mode) {
            return false;
        }
        if (!is_valid_path(name)) {
            return false;
        }
        const BlockFSEntry* entry = find_entry(name);
        if (!entry || !entry->in_use) {
            return false;
        }
        *size = entry->size;
        *type = entry_type(*entry);
        *mode = effectiveMode(entry);
        return true;
    }

    int32_t listDirectory(const char* path, char* out, uint32_t max) const {
        if (!mounted_ || !out || max == 0) {
            return -EINVAL;
        }
        const char* prefix = (path && path[0] != '\0') ? path : nullptr;
        uint32_t prefix_len = 0;
        if (prefix) {
            if (!is_valid_path(prefix)) {
                return -EINVAL;
            }
            const BlockFSEntry* entry = find_entry(prefix);
            if (!entry || entry_type(*entry) != kEntryDir) {
                return -ENOENT;
            }
            prefix_len = cstr_len(prefix);
        }
        struct SeenEntry {
            char name[kNameMax + 1];
            bool is_dir;
        };
        SeenEntry seen[kMaxFiles];
        uint32_t seen_count = 0;
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!entries_[i].in_use) {
                continue;
            }
            const char* name = entries_[i].name;
            if (prefix) {
                if (std::memcmp(name, prefix, prefix_len) != 0 || name[prefix_len] != '/') {
                    continue;
                }
                name += prefix_len + 1;
            }
            if (name[0] == '\0') {
                continue;
            }
            const char* slash = std::strchr(name, '/');
            uint32_t seg_len = slash ? static_cast<uint32_t>(slash - name)
                                     : static_cast<uint32_t>(cstr_len(name));
            if (seg_len == 0) {
                continue;
            }
            bool is_dir = slash != nullptr || entry_type(entries_[i]) == kEntryDir;
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

    BlockFSEntry* open(const char* name, bool create, uint16_t mode = 0) {
        if (!mounted_ || !name || name[0] == '\0') {
            return nullptr;
        }
        if (!is_valid_path(name)) {
            return nullptr;
        }
        BlockFSEntry* entry = find_entry(name);
        if (entry) {
            if (entry_type(*entry) == kEntryDir) {
                return nullptr;
            }
            return entry;
        }
        if (!create) {
            return nullptr;
        }
        if (!parent_dir_exists(name)) {
            return nullptr;
        }
        BlockFSEntry* free_slot = find_free();
        if (!free_slot) {
            return nullptr;
        }
        write_name(free_slot->name, name);
        free_slot->size = 0;
        free_slot->checksum = 0;
        free_slot->in_use = 1;
        set_entry_type(*free_slot, kEntryFile);
        uint16_t set_mode = mode != 0 ? mode : default_mode(kEntryFile);
        set_entry_mode(*free_slot, set_mode);
        uint32_t index = (uint32_t)(free_slot - entries_);
        if (!commit_entry(index)) {
            return nullptr;
        }
        return free_slot;
    }

    int64_t read(BlockFSEntry* entry, uint64_t offset, uint8_t* buf, uint32_t count) {
        if (!mounted_ || !entry || !buf) {
            return -EINVAL;
        }
        if (!entry->in_use) {
            return -EINVAL;
        }
        if (offset >= entry->size) {
            return 0;
        }
        if (!verify_checksum(entry)) {
            return -EIO;
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
            return -EINVAL;
        }
        if (!entry->in_use) {
            return -EINVAL;
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
            }
            if (!update_checksum(entry)) {
                return -EIO;
            }
            uint32_t index = (uint32_t)(entry - entries_);
            if (!commit_entry(index)) {
                return -EIO;
            }
        }
        return wrote;
    }

    int truncate(BlockFSEntry* entry) {
        if (!mounted_ || !entry) {
            return -EINVAL;
        }
        if (!entry->in_use) {
            return -EINVAL;
        }
        entry->size = 0;
        entry->checksum = 0;
        uint32_t index = (uint32_t)(entry - entries_);
        if (!commit_entry(index)) {
            return -EIO;
        }
        return 0;
    }

    bool remove(const char* name) {
        if (!mounted_ || !name) {
            return false;
        }
        if (!is_valid_path(name)) {
            return false;
        }
        BlockFSEntry* entry = find_entry(name);
        if (!entry) {
            return false;
        }
        if (entry_type(*entry) == kEntryDir && has_children(name)) {
            return false;
        }
        uint32_t index = (uint32_t)(entry - entries_);
        clear_entry(index);
        if (!commit_entry(index)) {
            return false;
        }
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
        int32_t rc = listDirectory(nullptr, out, max);
        if (rc < 0) {
            return 0;
        }
        return (uint32_t)rc;
    }

#ifndef RSE_KERNEL
    bool debugWriteEntry(uint32_t index, const BlockFSEntry& entry) {
        if (!mounted_ || index >= kMaxFiles) {
            return false;
        }
        entries_[index] = entry;
        entries_[index].slot_index = index;
        sync_entries();
        return true;
    }
#endif

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

    static size_t cstr_len(const char* s) {
        size_t len = 0;
        if (!s) {
            return 0;
        }
        while (s[len] != '\0') {
            ++len;
        }
        return len;
    }

    static uint8_t entry_type(const BlockFSEntry& entry) {
        return entry.reserved[0];
    }

    static uint16_t entry_mode(const BlockFSEntry& entry) {
        return (uint16_t)entry.reserved[1] | ((uint16_t)entry.reserved[2] << 8);
    }

    static void set_entry_type(BlockFSEntry& entry, uint8_t type) {
        entry.reserved[0] = type;
    }

    static void set_entry_mode(BlockFSEntry& entry, uint16_t mode) {
        entry.reserved[1] = (uint8_t)(mode & 0xFFu);
        entry.reserved[2] = (uint8_t)((mode >> 8) & 0xFFu);
    }

    static uint16_t default_mode(uint8_t type) {
        return type == kEntryDir ? 0755 : 0644;
    }

    static bool valid_type(uint8_t type) {
        return type == kEntryFile || type == kEntryDir;
    }

    bool parent_dir_exists(const char* path) const {
        const char* slash = std::strrchr(path, '/');
        if (!slash) {
            return true;
        }
        if (slash == path) {
            return false;
        }
        char parent[kNameMax + 1];
        size_t len = (size_t)(slash - path);
        if (len == 0 || len > kNameMax) {
            return false;
        }
        std::memset(parent, 0, sizeof(parent));
        std::memcpy(parent, path, len);
        parent[len] = '\0';
        const BlockFSEntry* entry = find_entry(parent);
        return entry && entry->in_use && entry_type(*entry) == kEntryDir;
    }

    bool has_children(const char* path) const {
        size_t len = cstr_len(path);
        if (len == 0) {
            return false;
        }
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!entries_[i].in_use) {
                continue;
            }
            const char* name = entries_[i].name;
            if (std::memcmp(name, path, len) == 0 && name[len] == '/') {
                return true;
            }
        }
        return false;
    }

    bool is_valid_header(const BlockFSHeader& hdr) const {
        return hdr.magic == kMagic &&
               hdr.version >= kMinVersion &&
               hdr.version <= kVersion &&
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
            entries_[i].checksum = 0;
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
        clear_journal();
        (void)sync_header();
        (void)sync_entries();
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

    bool sync_header() {
        uint8_t* scratch = new uint8_t[block_size_];
        if (!scratch) {
            return false;
        }
        std::memset(scratch, 0, block_size_);
        std::memcpy(scratch, &header_, sizeof(BlockFSHeader));
        bool ok = rse_block_write(start_lba_, scratch, 1) == 0;
        delete[] scratch;
        return ok;
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

    bool sync_entries() {
        uint32_t bytes = sizeof(BlockFSEntry) * kMaxFiles;
        uint32_t blocks = table_blocks_;
        uint8_t* scratch = new uint8_t[(size_t)blocks * block_size_];
        if (!scratch) {
            return false;
        }
        std::memset(scratch, 0, (size_t)blocks * block_size_);
        std::memcpy(scratch, entries_, bytes);
        bool ok = rse_block_write(start_lba_ + 1, scratch, blocks) == 0;
        delete[] scratch;
        return ok;
    }

    BlockFSEntry* find_entry(const char* name) {
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (entries_[i].in_use && name_equal(entries_[i].name, name)) {
                return &entries_[i];
            }
        }
        return nullptr;
    }

    const BlockFSEntry* find_entry(const char* name) const {
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
                entries_[i].checksum = 0;
                return &entries_[i];
            }
        }
        return nullptr;
    }

    void clear_entry(uint32_t index) {
        if (index >= kMaxFiles) {
            return;
        }
        BlockFSEntry& entry = entries_[index];
        entry.name[0] = '\0';
        entry.size = 0;
        entry.checksum = 0;
        entry.in_use = 0;
        entry.slot_index = index;
        set_entry_type(entry, kEntryFile);
        set_entry_mode(entry, 0);
    }

    void sanitize_entries() {
        bool changed = false;
        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!entries_[i].in_use) {
                continue;
            }
            const char* name = entries_[i].name;
            if (!is_valid_path(name)) {
                clear_entry(i);
                changed = true;
                continue;
            }
            uint8_t type = entry_type(entries_[i]);
            if (!valid_type(type)) {
                clear_entry(i);
                changed = true;
                continue;
            }
            if (entry_mode(entries_[i]) == 0) {
                set_entry_mode(entries_[i], default_mode(type));
                changed = true;
            }
            if (type == kEntryDir) {
                if (entries_[i].size != 0 || entries_[i].checksum != 0) {
                    entries_[i].size = 0;
                    entries_[i].checksum = 0;
                    changed = true;
                }
            } else {
                if (entries_[i].size > slot_size_) {
                    clear_entry(i);
                    changed = true;
                    continue;
                }
            }
        }

        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!entries_[i].in_use) {
                continue;
            }
            for (uint32_t j = i + 1; j < kMaxFiles; ++j) {
                if (!entries_[j].in_use) {
                    continue;
                }
                if (name_equal(entries_[i].name, entries_[j].name)) {
                    clear_entry(j);
                    changed = true;
                }
            }
        }

        bool removed = false;
        do {
            removed = false;
            for (uint32_t i = 0; i < kMaxFiles; ++i) {
                if (!entries_[i].in_use) {
                    continue;
                }
                const char* name = entries_[i].name;
                if (!parent_dir_exists(name)) {
                    clear_entry(i);
                    removed = true;
                    changed = true;
                }
            }
        } while (removed);

        for (uint32_t i = 0; i < kMaxFiles; ++i) {
            if (!entries_[i].in_use) {
                continue;
            }
            if (entry_type(entries_[i]) == kEntryFile &&
                !verify_checksum(&entries_[i])) {
                clear_entry(i);
                changed = true;
            }
        }
        if (changed) {
            (void)sync_entries();
        }
    }

    static void write_name(char* dst, const char* src) {
        uint32_t i = 0;
        for (; src[i] && i < kNameMax; ++i) {
            dst[i] = src[i];
        }
        dst[i] = '\0';
    }

    static bool is_valid_path(const char* name) {
        if (!name || name[0] == '\0') {
            return false;
        }
        if (name[0] == '/') {
            return false;
        }
        uint32_t len = 0;
        bool in_segment = false;
        for (const char* p = name; *p; ++p) {
            if (*p == '\\') {
                return false;
            }
            if (*p == '/') {
                if (!in_segment) {
                    return false;
                }
                in_segment = false;
            } else {
                in_segment = true;
            }
            if (++len > kNameMax) {
                return false;
            }
        }
        return in_segment;
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

    void clear_journal() {
        header_.journal_active = 0;
        header_.journal_index = 0;
        header_.journal_crc = 0;
        std::memset(&header_.journal_entry, 0, sizeof(header_.journal_entry));
    }

    uint32_t entry_crc(const BlockFSEntry& entry) const {
        static constexpr uint32_t kOffset = 2166136261u;
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&entry);
        return fnv1a_update(kOffset, bytes, (uint32_t)sizeof(BlockFSEntry));
    }

    void apply_journal() {
        if (header_.journal_active == 0) {
            return;
        }
        if (header_.journal_index >= kMaxFiles) {
            clear_journal();
            (void)sync_header();
            return;
        }
        if (entry_crc(header_.journal_entry) != header_.journal_crc) {
            clear_journal();
            (void)sync_header();
            return;
        }
        header_.journal_entry.slot_index = header_.journal_index;
        entries_[header_.journal_index] = header_.journal_entry;
        if (!sync_entries()) {
            return;
        }
        clear_journal();
        (void)sync_header();
    }

    bool commit_entry(uint32_t index) {
        if (index >= kMaxFiles) {
            return false;
        }
        header_.journal_active = 1;
        header_.journal_index = index;
        header_.journal_entry = entries_[index];
        header_.journal_crc = entry_crc(header_.journal_entry);
        if (!sync_header()) {
            return false;
        }
        if (!sync_entries()) {
            return false;
        }
        clear_journal();
        return sync_header();
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
                return -ENOMEM;
            }
        }

        if (block_off != 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -EIO;
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
                return -EIO;
            }
            out += full_bytes;
            remaining -= full_bytes;
            lba += blocks;
        }

        if (remaining > 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -EIO;
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
                return -ENOMEM;
            }
        }

        if (block_off != 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -EIO;
            }
            uint32_t take = block_size_ - block_off;
            if (take > remaining) {
                take = remaining;
            }
            std::memcpy(scratch + block_off, in, take);
            if (rse_block_write(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -EIO;
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
                return -EIO;
            }
            in += full_bytes;
            remaining -= full_bytes;
            lba += blocks;
        }

        if (remaining > 0) {
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -EIO;
            }
            std::memcpy(scratch, in, remaining);
            if (rse_block_write(lba, scratch, 1) != 0) {
                delete[] scratch;
                return -EIO;
            }
            remaining = 0;
        }

        delete[] scratch;
        return count;
    }

    static uint32_t fnv1a_update(uint32_t hash, const uint8_t* data, uint32_t len) {
        static constexpr uint32_t kPrime = 16777619u;
        for (uint32_t i = 0; i < len; ++i) {
            hash ^= data[i];
            hash *= kPrime;
        }
        return hash;
    }

    bool compute_checksum(const BlockFSEntry* entry, uint32_t* out) const {
        if (!entry || !out) {
            return false;
        }
        if (entry->size == 0) {
            *out = 0;
            return true;
        }
        uint8_t* scratch = new uint8_t[block_size_];
        if (!scratch) {
            return false;
        }
        static constexpr uint32_t kOffset = 2166136261u;
        uint32_t hash = kOffset;
        uint64_t remaining = entry->size;
        uint64_t offset = 0;
        uint64_t lba_base = data_start_lba_ + (uint64_t)entry->slot_index * slot_blocks_;
        while (remaining > 0) {
            uint64_t lba = lba_base + (offset / block_size_);
            if (rse_block_read(lba, scratch, 1) != 0) {
                delete[] scratch;
                return false;
            }
            uint32_t block_off = (uint32_t)(offset % block_size_);
            uint32_t take = block_size_ - block_off;
            if (take > remaining) {
                take = (uint32_t)remaining;
            }
            hash = fnv1a_update(hash, scratch + block_off, take);
            remaining -= take;
            offset += take;
        }
        delete[] scratch;
        *out = hash;
        return true;
    }

    bool update_checksum(BlockFSEntry* entry) {
        uint32_t hash = 0;
        if (!compute_checksum(entry, &hash)) {
            return false;
        }
        entry->checksum = hash;
        return true;
    }

    bool verify_checksum(const BlockFSEntry* entry) const {
        uint32_t hash = 0;
        if (!compute_checksum(entry, &hash)) {
            return false;
        }
        return hash == entry->checksum;
    }
};

} // namespace os
