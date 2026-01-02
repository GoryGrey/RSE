#include "../os/BlockFS.h"
#include "../os/Syscall.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

int main() {
    std::cout << "[BlockFS Tests]" << std::endl;

    os::rse_block_configure(512, 20000);
    os::BlockFS fs;
    bool mounted = fs.mount(512, os::rse_block_total_blocks());
    assert(mounted);

    const char nested_name[] = "bad/name";
    os::BlockFSEntry* bad = fs.open(nested_name, true, 0644);
    assert(bad == nullptr);
    bool removed_bad = fs.remove(nested_name);
    assert(!removed_bad);
    const char dot_name[] = "bad/./name";
    os::BlockFSEntry* dot_entry = fs.open(dot_name, true, 0644);
    assert(dot_entry == nullptr);
    const char dotdot_name[] = "bad/../name";
    os::BlockFSEntry* dotdot_entry = fs.open(dotdot_name, true, 0644);
    assert(dotdot_entry == nullptr);

    bool made_dir = fs.mkdir("bad", 0755);
    assert(made_dir);
    os::BlockFSEntry* nested = fs.open(nested_name, true, 0644);
    assert(nested != nullptr);

    std::array<char, os::BlockFS::kNameMax + 2> long_name{};
    for (size_t i = 0; i < long_name.size() - 1; ++i) {
        long_name[i] = 'a';
    }
    long_name[long_name.size() - 1] = '\0';
    os::BlockFSEntry* too_long = fs.open(long_name.data(), true, 0644);
    assert(too_long == nullptr);

    os::BlockFSEntry* entry = fs.open("alpha.txt", true, 0644);
    assert(entry != nullptr);

    const char payload[] = "blockfs payload";
    int64_t wrote = fs.write(entry, 0, reinterpret_cast<const uint8_t*>(payload),
                             static_cast<uint32_t>(sizeof(payload) - 1));
    assert(wrote == static_cast<int64_t>(sizeof(payload) - 1));

    std::array<uint8_t, 64> out{};
    int64_t read = fs.read(entry, 0, out.data(), sizeof(out));
    assert(read == static_cast<int64_t>(sizeof(payload) - 1));
    assert(std::memcmp(out.data(), payload, sizeof(payload) - 1) == 0);

    os::BlockFSEntry* wipe = fs.open("wipe.txt", true, 0644);
    assert(wipe != nullptr);
    const char wipe_payload[] = "wipe";
    int64_t wipe_wrote = fs.write(wipe, 0, reinterpret_cast<const uint8_t*>(wipe_payload),
                                  static_cast<uint32_t>(sizeof(wipe_payload) - 1));
    assert(wipe_wrote == static_cast<int64_t>(sizeof(wipe_payload) - 1));
    uint64_t wipe_lba = fs.getDataStartLba() + (uint64_t)fs.slotIndex(wipe) * fs.getSlotBlocks();
    std::array<uint8_t, 512> wipe_raw{};
    int rc = os::rse_block_read(wipe_lba, wipe_raw.data(), 1);
    assert(rc == 0);
    assert(std::memcmp(wipe_raw.data(), wipe_payload, sizeof(wipe_payload) - 1) == 0);
    bool removed_wipe = fs.remove("wipe.txt");
    assert(removed_wipe);
    std::array<uint8_t, 512> wiped{};
    rc = os::rse_block_read(wipe_lba, wiped.data(), 1);
    assert(rc == 0);
    for (size_t i = 0; i < wiped.size(); ++i) {
        assert(wiped[i] == 0);
    }
    os::BlockFSEntry* wipe2 = fs.open("wipe2.txt", true, 0644);
    assert(wipe2 != nullptr);
    uint64_t wipe2_lba = fs.getDataStartLba() + (uint64_t)fs.slotIndex(wipe2) * fs.getSlotBlocks();
    std::array<uint8_t, 512> wipe2_raw{};
    rc = os::rse_block_read(wipe2_lba, wipe2_raw.data(), 1);
    assert(rc == 0);
    for (size_t i = 0; i < wipe2_raw.size(); ++i) {
        assert(wipe2_raw[i] == 0);
    }

    uint64_t base_lba = fs.getDataStartLba() + (uint64_t)fs.slotIndex(entry) * fs.getSlotBlocks();
    std::array<uint8_t, 512> raw{};
    rc = os::rse_block_read(base_lba, raw.data(), 1);
    assert(rc == 0);
    raw[0] ^= 0xFF;
    rc = os::rse_block_write(base_lba, raw.data(), 1);
    assert(rc == 0);

    int64_t corrupt_read = fs.read(entry, 0, out.data(), sizeof(out));
    assert(corrupt_read == -os::EIO);

    os::BlockFSEntry* dup = fs.open("dup.txt", true, 0644);
    assert(dup != nullptr);
    os::BlockFSEntry dup_copy = *dup;
    dup_copy.size = 0;
    dup_copy.checksum = 0;
    dup_copy.in_use = 1;
    bool wrote_dup = fs.debugWriteEntry(200, dup_copy);
    assert(wrote_dup);

    os::BlockFSEntry* oversize = fs.open("oversize.txt", true, 0644);
    assert(oversize != nullptr);
    os::BlockFSEntry oversize_copy = *oversize;
    oversize_copy.size = fs.getSlotBlocks() * fs.getBlockSize() + 1;
    oversize_copy.checksum = 0;
    bool wrote_oversize = fs.debugWriteEntry(fs.slotIndex(oversize), oversize_copy);
    assert(wrote_oversize);

    os::BlockFSEntry* badtype = fs.open("badtype.txt", true, 0644);
    assert(badtype != nullptr);
    os::BlockFSEntry badtype_copy = *badtype;
    badtype_copy.reserved[0] = 0xFF;
    bool wrote_badtype = fs.debugWriteEntry(fs.slotIndex(badtype), badtype_copy);
    assert(wrote_badtype);

    os::BlockFS fs2;
    bool mounted2 = fs2.mount(512, os::rse_block_total_blocks());
    assert(mounted2);

    os::BlockFSEntry* alpha_after = fs2.open("alpha.txt", false, 0);
    assert(alpha_after != nullptr);
    assert(alpha_after->size == 0);
    bool removed_dup = fs2.remove("dup.txt");
    assert(removed_dup);
    os::BlockFSEntry* dup_after = fs2.open("dup.txt", false, 0);
    assert(dup_after == nullptr);
    os::BlockFSEntry* oversize_after = fs2.open("oversize.txt", false, 0);
    assert(oversize_after == nullptr);
    os::BlockFSEntry* badtype_after = fs2.open("badtype.txt", false, 0);
    assert(badtype_after == nullptr);

    bool removed_dir_fail = fs2.remove("bad");
    assert(!removed_dir_fail);
    bool removed_nested = fs2.remove(nested_name);
    assert(removed_nested);
    bool removed_dir = fs2.remove("bad");
    assert(removed_dir);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
