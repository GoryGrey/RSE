#include "../os/BlockFS.h"

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

    bool removed = fs.remove("alpha.txt");
    assert(removed);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
