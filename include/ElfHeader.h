//
// Created by fred.nicolson on 17/10/18.
//

#ifndef ELFLOADER_ELFHEADER_H
#define ELFLOADER_ELFHEADER_H


#include <sstream>

class ElfHeader
{
public:
    enum class WordSize : uint8_t
    {
        b32 = 1,
        b64 = 2,
        WordCount = 3,
    };

    enum class Endian : uint8_t
    {
        little = 1,
        big = 2,
    };

    enum class Architecture : uint16_t
    {
        Undefined = 0,
        Sparc = 2,
        x86 = 3,
        MIPS = 8,
        PowerPC = 0x14,
        ARM = 0x28,
        IA64 = 0x32,
        x86_64 = 0x3E,
    };

    enum class Type : uint16_t
    {
        relocatable = 1,
        executable = 2,
        shared = 3,
        core = 4
    };

    WordSize word;
    Endian endian;
    uint8_t version;
    uint8_t abi;
    Type type;
    Architecture arch;
    uintptr_t program_entry_pos;
    uintptr_t program_header_table_pos;
    uintptr_t program_section_table_pos;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_table_entry_count;
    uint16_t program_header_table_entry_size;
    uint16_t section_header_table_entry_count;
    uint16_t section_header_table_entry_size;
    uint16_t section_header_name_index;
};


#endif //ELFLOADER_ELFHEADER_H
