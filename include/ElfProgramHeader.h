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
        interpreted = 4,
    };

    enum Flags : uint32_t
    {
        executable = 1,
        writeable = 2,
        readable = 4
    };

    Type type;
    uint32_t flags;
    uint64_t file_offset;
    uint64_t mem_offset;
    uint64_t file_size;
    uint64_t mem_size;
    uint64_t alignment;
};


#endif //ELFLOADER_ELFPROGRAMHEADER_H
