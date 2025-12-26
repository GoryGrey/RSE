#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "../FixedStructures.h"

namespace os {

constexpr uint8_t EI_MAG0 = 0;
constexpr uint8_t EI_MAG1 = 1;
constexpr uint8_t EI_MAG2 = 2;
constexpr uint8_t EI_MAG3 = 3;
constexpr uint8_t EI_CLASS = 4;
constexpr uint8_t EI_DATA = 5;
constexpr uint8_t EI_VERSION = 6;

constexpr uint8_t ELF_MAGIC_0 = 0x7f;
constexpr uint8_t ELF_MAGIC_1 = 'E';
constexpr uint8_t ELF_MAGIC_2 = 'L';
constexpr uint8_t ELF_MAGIC_3 = 'F';

constexpr uint8_t ELFCLASS64 = 2;
constexpr uint8_t ELFDATA2LSB = 1;

constexpr uint16_t EM_X86_64 = 0x3e;
constexpr uint32_t PT_LOAD = 1;
constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_R = 0x4;

struct __attribute__((packed)) Elf64_Ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct __attribute__((packed)) Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

static_assert(sizeof(Elf64_Ehdr) == 64, "Elf64_Ehdr size mismatch");
static_assert(sizeof(Elf64_Phdr) == 56, "Elf64_Phdr size mismatch");

struct ElfSegment {
    uint64_t vaddr;
    uint64_t memsz;
    uint64_t filesz;
    uint64_t offset;
    uint64_t align;
    uint32_t flags;
};

struct ElfImage {
    uint64_t entry = 0;
    FixedVector<ElfSegment, 8> segments;
};

enum class ElfLoadError : uint8_t {
    kOk = 0,
    kTooSmall,
    kBadMagic,
    kUnsupportedClass,
    kUnsupportedEndian,
    kUnsupportedMachine,
    kInvalidProgramHeaders,
    kSegmentOutOfRange,
    kTooManySegments
};

inline bool parseElf64(const uint8_t* data, size_t size, ElfImage& out, ElfLoadError* error) {
    out.segments.clear();
    out.entry = 0;

    if (!data || size < sizeof(Elf64_Ehdr)) {
        if (error) {
            *error = ElfLoadError::kTooSmall;
        }
        return false;
    }

    const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);
    if (ehdr->e_ident[EI_MAG0] != ELF_MAGIC_0 ||
        ehdr->e_ident[EI_MAG1] != ELF_MAGIC_1 ||
        ehdr->e_ident[EI_MAG2] != ELF_MAGIC_2 ||
        ehdr->e_ident[EI_MAG3] != ELF_MAGIC_3) {
        if (error) {
            *error = ElfLoadError::kBadMagic;
        }
        return false;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        if (error) {
            *error = ElfLoadError::kUnsupportedClass;
        }
        return false;
    }

    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        if (error) {
            *error = ElfLoadError::kUnsupportedEndian;
        }
        return false;
    }

    if (ehdr->e_machine != EM_X86_64) {
        if (error) {
            *error = ElfLoadError::kUnsupportedMachine;
        }
        return false;
    }

    if (ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        if (error) {
            *error = ElfLoadError::kInvalidProgramHeaders;
        }
        return false;
    }

    uint64_t ph_end = ehdr->e_phoff + (uint64_t)ehdr->e_phnum * sizeof(Elf64_Phdr);
    if (ph_end > size) {
        if (error) {
            *error = ElfLoadError::kInvalidProgramHeaders;
        }
        return false;
    }

    const Elf64_Phdr* phdrs = reinterpret_cast<const Elf64_Phdr*>(data + ehdr->e_phoff);
    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        const Elf64_Phdr* ph = &phdrs[i];
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0) {
            continue;
        }
        if (ph->p_offset + ph->p_filesz > size) {
            if (error) {
                *error = ElfLoadError::kSegmentOutOfRange;
            }
            return false;
        }
        ElfSegment seg = {};
        seg.vaddr = ph->p_vaddr;
        seg.memsz = ph->p_memsz;
        seg.filesz = ph->p_filesz;
        seg.offset = ph->p_offset;
        seg.align = ph->p_align;
        seg.flags = ph->p_flags;
        if (!out.segments.push_back(seg)) {
            if (error) {
                *error = ElfLoadError::kTooManySegments;
            }
            return false;
        }
    }

    out.entry = ehdr->e_entry;
    if (error) {
        *error = ElfLoadError::kOk;
    }
    return true;
}

} // namespace os
