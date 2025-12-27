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

    os::BlockFSEntry* entry = fs.open("alpha.txt", true);
    assert(entry != nullptr);

    const char payload[] = "blockfs payload";
    int64_t wrote = fs.write(entry, 0, reinterpret_cast<const uint8_t*>(payload),
                             static_cast<uint32_t>(sizeof(payload) - 1));
    assert(wrote == static_cast<int64_t>(sizeof(payload) - 1));

    std::array<uint8_t, 64> out{};
    int64_t read = fs.read(entry, 0, out.data(), sizeof(out));
    assert(read == static_cast<int64_t>(sizeof(payload) - 1));
    assert(std::memcmp(out.data(), payload, sizeof(payload) - 1) == 0);

    uint64_t base_lba = fs.getDataStartLba() + (uint64_t)entry->slot_index * fs.getSlotBlocks();
    std::array<uint8_t, 512> raw{};
    int rc = os::rse_block_read(base_lba, raw.data(), 1);
    assert(rc == 0);
    raw[0] ^= 0xFF;
    rc = os::rse_block_write(base_lba, raw.data(), 1);
    assert(rc == 0);

    int64_t corrupt_read = fs.read(entry, 0, out.data(), sizeof(out));
    assert(corrupt_read == -os::EIO);

    bool removed = fs.remove("alpha.txt");
    assert(removed);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
