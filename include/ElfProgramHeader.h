//
// Created by fred.nicolson on 17/10/18.
//

#ifndef ELFLOADER_ELFPROGRAMHEADER_H
#define ELFLOADER_ELFPROGRAMHEADER_H
#include <cstdint>

class ElfProgramHeader
{
public:
    enum class Type : uint32_t
    {
        null = 0,
        load = 1,
        dynamic = 2,
        interpreted = 3,
        note = 4,
        shlib = 5,
        phdr = 6,
        tls = 7,
        loos = 0x60000000,
        hios = 0x6fffffff,
        loproc = 0x70000000,
        hiproc = 0x7fffffff,
    };

    enum Flags : uint32_t
    {
        executable = 1,
        writeable = 2,
        readable = 4
    };

    ElfProgramHeader()= default;
    ElfProgramHeader(Type type, uint32_t flags, uint64_t fileOffset, uint64_t memOffset, uint64_t fileSize, uint64_t memSize, uint64_t alignment)
    : type(type), flags(flags), file_offset(fileOffset), mem_offset(memOffset), file_size(fileSize), mem_size(memSize), alignment(alignment)
    {}

    Type type;
    uint32_t flags;
    uint64_t file_offset;
    uint64_t mem_offset;
    uint64_t file_size;
    uint64_t mem_size;
    uint64_t alignment;
};


#endif //ELFLOADER_ELFPROGRAMHEADER_H
