//
// Created by fred.nicolson on 24/10/18.
//

#ifndef ELFLOADER_ELFSECTIONHEADER_H
#define ELFLOADER_ELFSECTIONHEADER_H
#include <cstdint>
#include <string>

class ElfSectionHeader
{
public:

    enum class Type : uint32_t
    {
        null = 0,
        progbits = 1,
        symtab = 2,
        strtab = 3,
        rela = 4,
        hash = 5,
        dynamic = 6,
        note = 7,
        nobits = 8,
        rel = 9,
        shlib = 0x0A,
        dynsym = 0x0B,
        init_array = 0x0E,
        fini_array = 0x0F,
        preinit_array  = 0x10,
        group = 0x11,
        symtab_shndx = 0x12,
        num = 0x13
    };

    enum Flags : uint64_t
    {
        write = 1,
        alloc = 2,
        execinstr = 3,
        merge = 4,
        strings = 5,
        info_link = 6,
        link_order = 0,
        os_nonconforming = 0,
        group = 0,
        tls = 0,
        maskos = 0,
        maskproc = 0,
        ordered = 0,
        exclude = 0
    };

    uint32_t name_strtab_offset;
    std::string name;
    Type type;
    uint64_t flags;
    uint64_t mem_offset;
    uint64_t file_offset;
    uint64_t file_size;
    uint32_t link;
    uint32_t info;
    uint64_t alignment;
    uint64_t entry_size;
};


#endif //ELFLOADER_ELFSECTIONHEADER_H
