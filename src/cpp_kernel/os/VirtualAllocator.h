#pragma once

#include "PageTable.h"
#include "PhysicalAllocator.h"
#ifdef RSE_KERNEL
#include "KernelStubs.h"
#else
#include <iostream>
#endif

/**
 * Virtual Memory Allocator for Braided OS
 * 
 * Manages virtual address space for a process.
 * Integrates with page table and physical allocator.
 */

namespace os {

class VirtualAllocator {
private:
    PageTable* page_table_;
    PhysicalAllocator* phys_alloc_;
    
    // Virtual memory regions
    uint64_t heap_start_;
    uint64_t heap_end_;
    uint64_t heap_brk_;    // Current break (top of heap)
    
    uint64_t stack_start_;
    uint64_t stack_end_;
    
public:
    VirtualAllocator(PageTable* pt, PhysicalAllocator* pa)
        : page_table_(pt),
          phys_alloc_(pa),
          heap_start_(0x00000000'00400000ULL),   // 4MB
          heap_end_(0x00000000'40000000ULL),     // 1GB
          heap_brk_(0x00000000'00400000ULL),
          stack_start_(0x00007FFF'FFFF0000ULL),  // Near top of user space
          stack_end_(0x00007FFF'FFFFF000ULL) {
    }
    
    /**
     * Allocate virtual memory (like brk/sbrk).
     * 
     * @param size Number of bytes to allocate
     * @return Virtual address, or 0 if failed
     */
    uint64_t allocate(uint64_t size) {
        if (size == 0) return 0;
        
        // Align size to page boundary
        size = align_up(size);
        
        // Check if we have space
        if (heap_brk_ + size > heap_end_) {
            std::cerr << "[VirtualAllocator] Heap overflow!" << std::endl;
            return 0;
        }
        
        uint64_t virt_start = heap_brk_;
        uint64_t virt_end = heap_brk_ + size;
        
        // Allocate and map physical frames
        for (uint64_t virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
            // Allocate physical frame
            uint64_t phys = phys_alloc_->allocateFrame();
            if (phys == 0) {
                // Out of memory - unmap what we've allocated
                for (uint64_t v = virt_start; v < virt; v += PAGE_SIZE) {
                    uint64_t p = page_table_->translate(v);
                    if (p != 0) {
                        phys_alloc_->freeFrame(p);
                        page_table_->unmap(v);
                    }
                }
                return 0;
            }
            
            // Map virtual to physical
            if (!page_table_->map(virt, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
                // Mapping failed - free frame
                phys_alloc_->freeFrame(phys);
                
                // Unmap what we've allocated
                for (uint64_t v = virt_start; v < virt; v += PAGE_SIZE) {
                    uint64_t p = page_table_->translate(v);
                    if (p != 0) {
                        phys_alloc_->freeFrame(p);
                        page_table_->unmap(v);
                    }
                }
                return 0;
            }
        }
        
        // Update break
        heap_brk_ = virt_end;
        
        return virt_start;
    }
    
    /**
     * Free virtual memory.
     * 
     * @param addr Virtual address
     * @param size Number of bytes to free
     */
    void free(uint64_t addr, uint64_t size) {
        if (size == 0) return;
        
        // Align to page boundaries
        uint64_t virt_start = align_down(addr);
        uint64_t virt_end = align_up(addr + size);
        
        // Unmap and free physical frames
        for (uint64_t virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
            uint64_t phys = page_table_->translate(virt);
            if (phys != 0) {
                phys_alloc_->freeFrame(phys);
                page_table_->unmap(virt);
            }
        }
    }
    
    /**
     * Set heap break (like brk syscall).
     * 
     * @param new_brk New break address (0 = query current)
     * @return Current or new break address, or 0 on error
     */
    uint64_t brk(uint64_t new_brk) {
        // Query current break
        if (new_brk == 0) {
            return heap_brk_;
        }
        
        // Check bounds
        if (new_brk < heap_start_ || new_brk > heap_end_) {
            return 0;  // Invalid
        }
        
        // Growing heap
        if (new_brk > heap_brk_) {
            uint64_t size = new_brk - heap_brk_;
            if (allocate(size) == 0) {
                return 0;  // Failed
            }
            return heap_brk_;
        }
        
        // Shrinking heap
        if (new_brk < heap_brk_) {
            uint64_t size = heap_brk_ - new_brk;
            free(new_brk, size);
            heap_brk_ = new_brk;
            return heap_brk_;
        }
        
        // No change
        return heap_brk_;
    }
    
    /**
     * Map memory (like mmap syscall).
     * 
     * @param addr Hint address (0 = let OS choose)
     * @param size Size in bytes
     * @param prot Protection flags
     * @return Virtual address, or 0 on error
     */
    uint64_t mmap(uint64_t addr, uint64_t size, uint64_t prot) {
        if (size == 0) return 0;
        
        // Align size
        size = align_up(size);
        
        // Choose address if not specified
        if (addr == 0) {
            addr = heap_brk_;
        }
        
        // Align address
        addr = align_down(addr);
        
        // Check bounds
        if (addr < heap_start_ || addr + size > heap_end_) {
            std::cerr << "[VirtualAllocator] mmap address out of range!" << std::endl;
            return 0;
        }
        
        // Convert protection flags to PTE flags
        uint64_t pte_flags = PTE_PRESENT | PTE_USER;
        if (prot & 0x02) {  // PROT_WRITE
            pte_flags |= PTE_WRITABLE;
        }
        
        // Allocate and map physical frames
        for (uint64_t virt = addr; virt < addr + size; virt += PAGE_SIZE) {
            // Allocate physical frame
            uint64_t phys = phys_alloc_->allocateFrame();
            if (phys == 0) {
                // Out of memory - unmap what we've allocated
                for (uint64_t v = addr; v < virt; v += PAGE_SIZE) {
                    uint64_t p = page_table_->translate(v);
                    if (p != 0) {
                        phys_alloc_->freeFrame(p);
                        page_table_->unmap(v);
                    }
                }
                return 0;
            }
            
            // Map virtual to physical
            if (!page_table_->map(virt, phys, pte_flags)) {
                phys_alloc_->freeFrame(phys);
                
                // Unmap what we've allocated
                for (uint64_t v = addr; v < virt; v += PAGE_SIZE) {
                    uint64_t p = page_table_->translate(v);
                    if (p != 0) {
                        phys_alloc_->freeFrame(p);
                        page_table_->unmap(v);
                    }
                }
                return 0;
            }
        }
        
        return addr;
    }
    
    /**
     * Unmap memory (like munmap syscall).
     * 
     * @param addr Virtual address
     * @param size Size in bytes
     */
    void munmap(uint64_t addr, uint64_t size) {
        free(addr, size);
    }

    /**
     * Map a fixed address range (for ELF segments, stacks, etc).
     */
    bool mapSegment(const uint8_t* data, uint64_t file_size, uint64_t vaddr, uint64_t mem_size, uint32_t elf_flags) {
        if (mem_size == 0) {
            return true;
        }
        uint64_t pte_flags = PTE_USER;
        if (elf_flags & 0x2) {  // PF_W
            pte_flags |= PTE_WRITABLE;
        }
        return mapRange(vaddr, mem_size, pte_flags, data, file_size);
    }

    /**
     * Allocate and map a user stack.
     * Returns stack pointer (top of stack) or 0 on failure.
     */
    uint64_t allocateStack(uint64_t size) {
        if (size == 0) {
            return 0;
        }
        size = align_up(size);
        if (stack_end_ < stack_start_ + size) {
            return 0;
        }
        uint64_t guard = size > PAGE_SIZE ? PAGE_SIZE : 0;
        uint64_t stack_base = stack_end_ - size;
        uint64_t mapped_base = stack_base + guard;
        uint64_t mapped_size = size - guard;
        if (mapped_size == 0) {
            return 0;
        }
        if (!mapRange(mapped_base, mapped_size, PTE_USER | PTE_WRITABLE, nullptr, 0)) {
            return 0;
        }
        return stack_end_;
    }
    
    /**
     * Change memory protection (like mprotect syscall).
     * 
     * @param addr Virtual address
     * @param size Size in bytes
     * @param prot New protection flags
     * @return true on success
     */
    bool mprotect(uint64_t addr, uint64_t size, uint64_t prot) {
        if (size == 0) return true;
        
        // Align to page boundaries
        uint64_t virt_start = align_down(addr);
        uint64_t virt_end = align_up(addr + size);
        
        // Convert protection flags to PTE flags
        uint64_t pte_flags = PTE_PRESENT | PTE_USER;
        if (prot & 0x02) {  // PROT_WRITE
            pte_flags |= PTE_WRITABLE;
        }
        
        // Update protection for each page
        for (uint64_t virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
            if (!page_table_->protect(virt, pte_flags)) {
                return false;
            }
        }
        
        return true;
    }

    /**
     * Adjust heap start/break after loading an ELF image.
     */
    void setHeapStart(uint64_t start) {
        start = align_up(start);
        heap_start_ = start;
        if (heap_brk_ < heap_start_) {
            heap_brk_ = heap_start_;
        }
    }
    
    /**
     * Get heap bounds.
     */
    uint64_t getHeapStart() const { return heap_start_; }
    uint64_t getHeapEnd() const { return heap_end_; }
    uint64_t getHeapBrk() const { return heap_brk_; }
    uint64_t getStackStart() const { return stack_start_; }
    uint64_t getStackEnd() const { return stack_end_; }
    PageTable* getPageTable() const { return page_table_; }
    PhysicalAllocator* getPhysicalAllocator() const { return phys_alloc_; }

    void setStackBounds(uint64_t start, uint64_t end) {
        start = align_down(start);
        end = align_up(end);
        if (end <= start) {
            return;
        }
        stack_start_ = start;
        stack_end_ = end;
    }

    bool isUserRange(uint64_t addr, uint64_t size) const {
        if (size == 0) {
            return true;
        }
        const uint64_t user_min = 0x1000;
        uint64_t end = addr + size - 1;
        if (end < addr) {
            return false;
        }
        return addr >= user_min && end < stack_end_;
    }

    bool validateUserRange(uint64_t addr, uint64_t size, bool write) const {
        if (!isUserRange(addr, size) || !page_table_) {
            return false;
        }
        uint64_t virt_start = align_down(addr);
        uint64_t virt_end = align_up(addr + size);
        for (uint64_t virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
            const PageTableEntry* pte = page_table_->getPTE(virt);
            if (!pte || !pte->isPresent() || !pte->isUser()) {
                return false;
            }
            if (write && !pte->isWritable()) {
                return false;
            }
        }
        return true;
    }

    bool readUser(void* dst, uint64_t src, uint64_t size) const {
        if (!dst || size == 0) {
            return false;
        }
        if (!isUserRange(src, size) || !page_table_ || !phys_alloc_) {
            return false;
        }
        uint8_t* out = static_cast<uint8_t*>(dst);
        uint64_t addr = src;
        uint64_t remaining = size;
        while (remaining > 0) {
            uint64_t phys = page_table_->translate(addr);
            if (phys == 0) {
                return false;
            }
            void* phys_ptr = phys_alloc_->ptrFromPhys(phys);
            if (!phys_ptr) {
                return false;
            }
            uint64_t page_off = phys & (PAGE_SIZE - 1);
            uint64_t chunk = PAGE_SIZE - page_off;
            if (chunk > remaining) {
                chunk = remaining;
            }
            const uint8_t* src = static_cast<const uint8_t*>(phys_ptr);
            for (uint64_t i = 0; i < chunk; ++i) {
                out[i] = src[i];
            }
            out += chunk;
            addr += chunk;
            remaining -= chunk;
        }
        return true;
    }

    bool writeUser(uint64_t dst, const void* src, uint64_t size) const {
        if (!src || size == 0) {
            return false;
        }
        if (!isUserRange(dst, size) || !page_table_ || !phys_alloc_) {
            return false;
        }
        const uint8_t* in = static_cast<const uint8_t*>(src);
        uint64_t addr = dst;
        uint64_t remaining = size;
        while (remaining > 0) {
            uint64_t phys = page_table_->translate(addr);
            if (phys == 0) {
                return false;
            }
            void* phys_ptr = phys_alloc_->ptrFromPhys(phys);
            if (!phys_ptr) {
                return false;
            }
            uint64_t page_off = phys & (PAGE_SIZE - 1);
            uint64_t chunk = PAGE_SIZE - page_off;
            if (chunk > remaining) {
                chunk = remaining;
            }
            uint8_t* dst = static_cast<uint8_t*>(phys_ptr);
            for (uint64_t i = 0; i < chunk; ++i) {
                dst[i] = in[i];
            }
            in += chunk;
            addr += chunk;
            remaining -= chunk;
        }
        return true;
    }

    VirtualAllocator* clone() const {
        PageTable* new_pt = page_table_ ? page_table_->clone() : nullptr;
        if (!new_pt) {
            return nullptr;
        }
        VirtualAllocator* va = new VirtualAllocator(new_pt, phys_alloc_);
        va->heap_start_ = heap_start_;
        va->heap_end_ = heap_end_;
        va->heap_brk_ = heap_brk_;
        va->stack_start_ = stack_start_;
        va->stack_end_ = stack_end_;
        return va;
    }
    
    /**
     * Print memory statistics.
     */
    void printStats() const {
        uint64_t heap_used = heap_brk_ - heap_start_;
        
        std::cout << "[VirtualAllocator] "
                  << "Heap: " << (heap_used / 1024) << " KB used, "
                  << ((heap_end_ - heap_brk_) / 1024 / 1024) << " MB available"
                  << std::endl;
        
        page_table_->printStats();
    }

private:
    bool mapRange(uint64_t addr, uint64_t size, uint64_t pte_flags,
                  const uint8_t* init_data, uint64_t init_size) {
        if (!page_table_ || !phys_alloc_ || size == 0) {
            return false;
        }

        uint64_t virt_start = align_down(addr);
        uint64_t virt_end = align_up(addr + size);
        uint64_t data_remaining = init_size;
        uint64_t data_offset = addr - virt_start;
        uint64_t mapped_end = virt_start;
        bool ok = true;

        for (uint64_t virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
            uint64_t phys = phys_alloc_->allocateFrame();
            if (phys == 0) {
                ok = false;
                break;
            }
            uint64_t map_flags = pte_flags | PTE_PRESENT;
            if (!page_table_->map(virt, phys, map_flags)) {
                phys_alloc_->freeFrame(phys);
                ok = false;
                break;
            }
            mapped_end = virt + PAGE_SIZE;

            void* page_ptr = phys_alloc_->ptrFromPhys(phys);
            if (page_ptr) {
                uint8_t* dst = static_cast<uint8_t*>(page_ptr);
                for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
                    dst[i] = 0;
                }
            }

            if (init_data && data_remaining > 0) {
                if (!page_ptr) {
                    ok = false;
                    break;
                }
                uint64_t copy_size = PAGE_SIZE - data_offset;
                if (copy_size > data_remaining) {
                    copy_size = data_remaining;
                }
                uint8_t* dst = static_cast<uint8_t*>(page_ptr) + data_offset;
                const uint8_t* src = init_data + (init_size - data_remaining);
                for (uint64_t i = 0; i < copy_size; ++i) {
                    dst[i] = src[i];
                }
                data_remaining -= copy_size;
            }

            data_offset = 0;
        }

        if (!ok || data_remaining > 0) {
            for (uint64_t virt = virt_start; virt < mapped_end; virt += PAGE_SIZE) {
                uint64_t phys = page_table_->translate(virt);
                if (phys != 0) {
                    phys_alloc_->freeFrame(phys);
                    page_table_->unmap(virt);
                }
            }
            return false;
        }

        return true;
    }
};

} // namespace os
