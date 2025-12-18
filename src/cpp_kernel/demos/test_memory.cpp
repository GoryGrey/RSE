#include "../os/PageTable.h"
#include "../os/PhysicalAllocator.h"
#include "../os/VirtualAllocator.h"
#include <iostream>
#include <cassert>

using namespace os;

void test_page_table() {
    std::cout << "\n=== Test 1: Page Table ===" << std::endl;
    
    PageTable pt;
    
    // Map some pages
    assert(pt.map(0x1000, 0x10000));
    assert(pt.map(0x2000, 0x20000));
    assert(pt.map(0x3000, 0x30000));
    
    // Translate
    assert(pt.translate(0x1000) == 0x10000);
    assert(pt.translate(0x1234) == 0x10234);
    assert(pt.translate(0x2000) == 0x20000);
    assert(pt.translate(0x3000) == 0x30000);
    
    // Check mapping
    assert(pt.isMapped(0x1000));
    assert(pt.isMapped(0x2000));
    assert(!pt.isMapped(0x4000));
    
    // Unmap
    pt.unmap(0x2000);
    assert(!pt.isMapped(0x2000));
    assert(pt.translate(0x2000) == 0);
    
    pt.printStats();
    
    std::cout << "✅ Test 1 passed!" << std::endl;
}

void test_physical_allocator() {
    std::cout << "\n=== Test 2: Physical Allocator ===" << std::endl;
    
    // 16MB of physical memory
    PhysicalAllocator pa(0x100000, 16 * 1024 * 1024);
    
    uint64_t total = pa.total();
    assert(total == (16 * 1024 * 1024) / PAGE_SIZE);
    assert(pa.available() == total);
    
    // Allocate some frames
    uint64_t frame1 = pa.allocateFrame();
    uint64_t frame2 = pa.allocateFrame();
    uint64_t frame3 = pa.allocateFrame();
    
    assert(frame1 != 0);
    assert(frame2 != 0);
    assert(frame3 != 0);
    assert(frame1 != frame2);
    assert(frame2 != frame3);
    
    assert(pa.available() == total - 3);
    
    // Free frames
    pa.freeFrame(frame2);
    assert(pa.available() == total - 2);
    
    pa.freeFrame(frame1);
    pa.freeFrame(frame3);
    assert(pa.available() == total);
    
    pa.printStats();
    
    std::cout << "✅ Test 2 passed!" << std::endl;
}

void test_virtual_allocator() {
    std::cout << "\n=== Test 3: Virtual Allocator ===" << std::endl;
    
    PageTable pt;
    PhysicalAllocator pa(0x100000, 16 * 1024 * 1024);
    VirtualAllocator va(&pt, &pa);
    
    // Allocate some memory
    uint64_t addr1 = va.allocate(4096);
    uint64_t addr2 = va.allocate(8192);
    uint64_t addr3 = va.allocate(4096);
    
    assert(addr1 != 0);
    assert(addr2 != 0);
    assert(addr3 != 0);
    
    // Check that they're mapped
    assert(pt.isMapped(addr1));
    assert(pt.isMapped(addr2));
    assert(pt.isMapped(addr3));
    
    // Free memory
    va.free(addr2, 8192);
    assert(!pt.isMapped(addr2));
    
    va.printStats();
    
    std::cout << "✅ Test 3 passed!" << std::endl;
}

void test_brk() {
    std::cout << "\n=== Test 4: brk() ===" << std::endl;
    
    PageTable pt;
    PhysicalAllocator pa(0x100000, 16 * 1024 * 1024);
    VirtualAllocator va(&pt, &pa);
    
    // Get current break
    uint64_t old_brk = va.brk(0);
    std::cout << "Initial break: 0x" << std::hex << old_brk << std::dec << std::endl;
    
    // Grow heap
    uint64_t new_brk = va.brk(old_brk + 16384);
    assert(new_brk == old_brk + 16384);
    
    // Check that pages are mapped
    assert(pt.isMapped(old_brk));
    assert(pt.isMapped(old_brk + 4096));
    assert(pt.isMapped(old_brk + 8192));
    assert(pt.isMapped(old_brk + 12288));
    
    // Shrink heap
    new_brk = va.brk(old_brk + 4096);
    assert(new_brk == old_brk + 4096);
    
    // Check that pages are unmapped
    assert(pt.isMapped(old_brk));
    assert(!pt.isMapped(old_brk + 8192));
    
    va.printStats();
    
    std::cout << "✅ Test 4 passed!" << std::endl;
}

void test_mmap() {
    std::cout << "\n=== Test 5: mmap() ===" << std::endl;
    
    PageTable pt;
    PhysicalAllocator pa(0x100000, 16 * 1024 * 1024);
    VirtualAllocator va(&pt, &pa);
    
    // Map some memory
    uint64_t addr = va.mmap(0, 16384, 0x03);  // PROT_READ | PROT_WRITE
    assert(addr != 0);
    
    std::cout << "Mapped at: 0x" << std::hex << addr << std::dec << std::endl;
    
    // Check that pages are mapped
    assert(pt.isMapped(addr));
    assert(pt.isMapped(addr + 4096));
    assert(pt.isMapped(addr + 8192));
    assert(pt.isMapped(addr + 12288));
    
    // Check writable
    PageTableEntry* pte = pt.getPTE(addr);
    assert(pte != nullptr);
    assert(pte->isWritable());
    
    // Unmap
    va.munmap(addr, 16384);
    assert(!pt.isMapped(addr));
    
    va.printStats();
    
    std::cout << "✅ Test 5 passed!" << std::endl;
}

void test_mprotect() {
    std::cout << "\n=== Test 6: mprotect() ===" << std::endl;
    
    PageTable pt;
    PhysicalAllocator pa(0x100000, 16 * 1024 * 1024);
    VirtualAllocator va(&pt, &pa);
    
    // Map some memory (read/write)
    uint64_t addr = va.mmap(0, 4096, 0x03);  // PROT_READ | PROT_WRITE
    assert(addr != 0);
    
    PageTableEntry* pte = pt.getPTE(addr);
    assert(pte->isWritable());
    
    // Change to read-only
    assert(va.mprotect(addr, 4096, 0x01));  // PROT_READ
    assert(!pte->isWritable());
    
    // Change back to read/write
    assert(va.mprotect(addr, 4096, 0x03));  // PROT_READ | PROT_WRITE
    assert(pte->isWritable());
    
    va.printStats();
    
    std::cout << "✅ Test 6 passed!" << std::endl;
}

void test_clone() {
    std::cout << "\n=== Test 7: Page Table Clone ===" << std::endl;
    
    PageTable pt;
    
    // Map some pages
    pt.map(0x1000, 0x10000);
    pt.map(0x2000, 0x20000);
    pt.map(0x3000, 0x30000);
    
    // Clone
    PageTable* pt2 = pt.clone();
    assert(pt2 != nullptr);
    
    // Check that mappings are copied
    assert(pt2->translate(0x1000) == 0x10000);
    assert(pt2->translate(0x2000) == 0x20000);
    assert(pt2->translate(0x3000) == 0x30000);
    
    // Modify original
    pt.unmap(0x2000);
    assert(!pt.isMapped(0x2000));
    assert(pt2->isMapped(0x2000));  // Clone still has it
    
    delete pt2;
    
    std::cout << "✅ Test 7 passed!" << std::endl;
}

void test_stress() {
    std::cout << "\n=== Test 8: Stress Test ===" << std::endl;
    
    PageTable pt;
    PhysicalAllocator pa(0x100000, 64 * 1024 * 1024);  // 64MB
    VirtualAllocator va(&pt, &pa);
    
    // Allocate lots of memory
    const int num_allocs = 100;
    uint64_t addrs[num_allocs];
    
    for (int i = 0; i < num_allocs; i++) {
        addrs[i] = va.allocate(4096);
        assert(addrs[i] != 0);
    }
    
    std::cout << "Allocated " << num_allocs << " pages" << std::endl;
    
    // Free half
    for (int i = 0; i < num_allocs / 2; i++) {
        va.free(addrs[i], 4096);
    }
    
    std::cout << "Freed " << (num_allocs / 2) << " pages" << std::endl;
    
    // Allocate more
    for (int i = 0; i < num_allocs / 2; i++) {
        uint64_t addr = va.allocate(4096);
        assert(addr != 0);
    }
    
    std::cout << "Allocated " << (num_allocs / 2) << " more pages" << std::endl;
    
    va.printStats();
    pa.printStats();
    
    std::cout << "✅ Test 8 passed!" << std::endl;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         MEMORY MANAGEMENT TEST SUITE                     ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    test_page_table();
    test_physical_allocator();
    test_virtual_allocator();
    test_brk();
    test_mmap();
    test_mprotect();
    test_clone();
    test_stress();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         ALL TESTS PASSED ✅                               ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    return 0;
}
