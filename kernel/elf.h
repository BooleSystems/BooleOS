#ifndef ELF_H
#define ELF_H

#include <stdint.h>

/* ELF32 header */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

/* ELF32 program header */
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

#define ELF_MAGIC0    0x7F
#define ELF_MAGIC1    'E'
#define ELF_MAGIC2    'L'
#define ELF_MAGIC3    'F'
#define ELF_CLASS32   1
#define ELF_DATA2LSB  1   /* little-endian */
#define ET_EXEC       2
#define EM_386        3
#define PT_LOAD       1

/*
 * elf_load - load an ELF32 executable into a process address space.
 *
 * cr3         : physical address of the process page directory
 * elf_data    : pointer to the ELF image in kernel-accessible memory
 * entry_point : out — virtual entry point (e_entry)
 *
 * Returns 0 on success, -1 on error.
 */
int elf_load(uint32_t cr3, const void *elf_data, uint32_t *entry_point);

#endif
