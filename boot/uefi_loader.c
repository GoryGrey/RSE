#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

#include "rse_boot.h"

static const EFI_GUID EFI_LOADED_IMAGE_PROTOCOL_GUID_LOCAL = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static const EFI_GUID EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID_LOCAL = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static const EFI_GUID EFI_FILE_INFO_ID_LOCAL = EFI_FILE_INFO_ID;

struct RelocBlock {
    UINT32 page_rva;
    UINT32 block_size;
    UINT16 entry;
    UINT16 pad;
};

__attribute__((used, section(".reloc")))
static const struct RelocBlock reloc_stub = {
    .page_rva = 0,
    .block_size = 12,
    .entry = 0,
    .pad = 0
};

static void put_line(EFI_SYSTEM_TABLE *st, const CHAR16 *msg) {
    if (st && st->ConOut) {
        st->ConOut->OutputString(st->ConOut, (CHAR16 *)msg);
    }
}

static inline void outb(UINT16 port, UINT8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static int serial_can_send(void) {
    return (inb(0x3F8 + 5) & 0x20) != 0;
}

static void serial_write_char(char c) {
    if (c == '\n') {
        serial_write_char('\r');
    }
    while (!serial_can_send()) {
    }
    outb(0x3F8, (UINT8)c);
}

static void serial_write(const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        serial_write_char(*s++);
    }
}

static void serial_write_hex64(UINT64 value) {
    static const char hex[] = "0123456789abcdef";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        buf[2 + i] = hex[(value >> ((15 - i) * 4)) & 0xF];
    }
    buf[18] = '\0';
    serial_write(buf);
}

#define ELF_MAGIC_0 0x7f
#define ELF_MAGIC_1 'E'
#define ELF_MAGIC_2 'L'
#define ELF_MAGIC_3 'F'

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EM_X86_64 0x3E

#define PT_LOAD 1

typedef struct {
    unsigned char e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} Elf64_Phdr;

static EFI_STATUS load_file(EFI_HANDLE image_handle,
                            EFI_SYSTEM_TABLE *st,
                            const CHAR16 *path,
                            void **out_buffer,
                            UINTN *out_size) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE *loaded_image = NULL;

    serial_write("[RSE] load_file start\n");
    if (!st || !st->BootServices) {
        serial_write("[RSE] no BootServices\n");
        return EFI_LOAD_ERROR;
    }
    serial_write("[RSE] BootServices=");
    serial_write_hex64((UINT64)st->BootServices);
    serial_write("\n");
    serial_write("[RSE] HandleProtocol=");
    serial_write_hex64((UINT64)st->BootServices->HandleProtocol);
    serial_write("\n");
    status = st->BootServices->HandleProtocol(image_handle,
                                             (EFI_GUID *)&EFI_LOADED_IMAGE_PROTOCOL_GUID_LOCAL,
                                             (void **)&loaded_image);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] HandleProtocol LoadedImage failed\r\n");
        return status;
    }
    serial_write("[RSE] load_file loaded_image ok\n");

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    status = st->BootServices->HandleProtocol(loaded_image->DeviceHandle,
                                             (EFI_GUID *)&EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID_LOCAL,
                                             (void **)&fs);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] HandleProtocol FS failed\r\n");
        return status;
    }
    serial_write("[RSE] load_file fs ok\n");

    EFI_FILE_PROTOCOL *root = NULL;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] OpenVolume failed\r\n");
        return status;
    }
    serial_write("[RSE] load_file open volume ok\n");

    EFI_FILE_PROTOCOL *file = NULL;
    status = root->Open(root, &file, (CHAR16 *)path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] Open file failed\r\n");
        return status;
    }
    serial_write("[RSE] load_file open file ok\n");

    UINTN info_size = SIZE_OF_EFI_FILE_INFO + 200;
    EFI_FILE_INFO *info = NULL;
    status = st->BootServices->AllocatePool(EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] AllocatePool info failed\r\n");
        return status;
    }

    status = file->GetInfo(file, (EFI_GUID *)&EFI_FILE_INFO_ID_LOCAL, &info_size, info);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] GetInfo failed\r\n");
        return status;
    }
    serial_write("[RSE] load_file getinfo ok\n");

    UINTN file_size = info->FileSize;
    void *buffer = NULL;
    status = st->BootServices->AllocatePool(EfiLoaderData, file_size, &buffer);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] AllocatePool file failed\r\n");
        return status;
    }
    serial_write("[RSE] load_file alloc ok\n");

    status = file->Read(file, &file_size, buffer);
    if (EFI_ERROR(status)) {
        put_line(st, L"[RSE] Read failed\r\n");
        return status;
    }
    serial_write("[RSE] load_file read ok\n");

    *out_buffer = buffer;
    *out_size = file_size;
    return EFI_SUCCESS;
}

static EFI_STATUS load_elf_kernel(EFI_SYSTEM_TABLE *st,
                                  void *buffer,
                                  UINTN size,
                                  void **out_entry) {
    if (size < sizeof(Elf64_Ehdr)) {
        put_line(st, L"[RSE] ELF header too small\r\n");
        return EFI_LOAD_ERROR;
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buffer;
    if (ehdr->e_ident[0] != ELF_MAGIC_0 ||
        ehdr->e_ident[1] != ELF_MAGIC_1 ||
        ehdr->e_ident[2] != ELF_MAGIC_2 ||
        ehdr->e_ident[3] != ELF_MAGIC_3 ||
        ehdr->e_ident[4] != ELFCLASS64 ||
        ehdr->e_ident[5] != ELFDATA2LSB ||
        ehdr->e_machine != EM_X86_64) {
        put_line(st, L"[RSE] Invalid ELF header\r\n");
        return EFI_UNSUPPORTED;
    }

    if (ehdr->e_phoff + (UINT64)ehdr->e_phnum * sizeof(Elf64_Phdr) > size) {
        put_line(st, L"[RSE] Invalid program headers\r\n");
        return EFI_LOAD_ERROR;
    }

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((UINT8 *)buffer + ehdr->e_phoff);
    for (UINT16 i = 0; i < ehdr->e_phnum; ++i) {
        Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS dest = (EFI_PHYSICAL_ADDRESS)ph->p_paddr;
        UINTN pages = (UINTN)((ph->p_memsz + 0xFFF) / 0x1000);
        serial_write("[RSE] seg paddr=");
        serial_write_hex64((UINT64)dest);
        serial_write(" memsz=");
        serial_write_hex64((UINT64)ph->p_memsz);
        serial_write(" pages=");
        serial_write_hex64((UINT64)pages);
        serial_write("\n");
        EFI_STATUS status = st->BootServices->AllocatePages(
            AllocateAddress, EfiLoaderData, pages, &dest);
        if (EFI_ERROR(status)) {
            serial_write("[RSE] AllocatePages status=");
            serial_write_hex64((UINT64)status);
            serial_write("\n");
            put_line(st, L"[RSE] AllocatePages failed\r\n");
            return status;
        }

        st->BootServices->SetMem((void *)(UINTN)dest, ph->p_memsz, 0);
        if (ph->p_filesz > 0) {
            if (ph->p_offset + ph->p_filesz > size) {
                put_line(st, L"[RSE] Segment out of range\r\n");
                return EFI_LOAD_ERROR;
            }
            st->BootServices->CopyMem(
                (void *)(UINTN)dest,
                (UINT8 *)buffer + ph->p_offset,
                ph->p_filesz);
        }
    }

    *out_entry = (void *)(UINTN)ehdr->e_entry;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI _start(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    serial_init();
    serial_write("[RSE] UEFI loader start\n");
    put_line(system_table, L"[RSE] UEFI loader start\r\n");

    void *buffer = NULL;
    UINTN size = 0;
    EFI_STATUS status = load_file(image_handle, system_table, L"\\kernel.elf", &buffer, &size);
    if (EFI_ERROR(status)) {
        serial_write("[RSE] Kernel read failed\n");
        put_line(system_table, L"[RSE] Kernel read failed\r\n");
        return status;
    }
    serial_write("[RSE] Kernel read ok\n");

    void *entry = NULL;
    status = load_elf_kernel(system_table, buffer, size, &entry);
    if (EFI_ERROR(status)) {
        serial_write("[RSE] Kernel load failed\n");
        put_line(system_table, L"[RSE] Kernel load failed\r\n");
        return status;
    }
    serial_write("[RSE] Kernel load ok\n");

    struct rse_boot_info *boot_info = NULL;
    EFI_STATUS alloc_status = system_table->BootServices->AllocatePool(
        EfiLoaderData, sizeof(*boot_info), (void **)&boot_info);
    if (!EFI_ERROR(alloc_status) && boot_info) {
        boot_info->magic = RSE_BOOT_MAGIC;
        boot_info->fb_addr = 0;
        boot_info->fb_width = 0;
        boot_info->fb_height = 0;
        boot_info->fb_pitch = 0;
        boot_info->fb_bpp = 0;
        boot_info->system_table = (UINT64)(UINTN)system_table;
    }
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS gop_status = system_table->BootServices->LocateProtocol(
        &gop_guid,
        NULL,
        (void **)&gop);
    if (!EFI_ERROR(gop_status) && gop && gop->Mode && gop->Mode->Info && boot_info) {
        boot_info->fb_addr = (UINT64)gop->Mode->FrameBufferBase;
        boot_info->fb_width = gop->Mode->Info->HorizontalResolution;
        boot_info->fb_height = gop->Mode->Info->VerticalResolution;
        boot_info->fb_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
        boot_info->fb_bpp = 32;
        serial_write("[RSE] GOP framebuffer ok\n");
    } else if (!boot_info) {
        serial_write("[RSE] Boot info alloc failed\n");
    } else {
        serial_write("[RSE] GOP not available\n");
    }

    serial_write("[RSE] Jumping to kernel\n");
    put_line(system_table, L"[RSE] Jumping to kernel\r\n");
    void (*kernel_entry)(void *) = (void (*)(void *))entry;
    kernel_entry(boot_info);

    return EFI_SUCCESS;
}
