//
// Created by fred.nicolson on 17/10/18.
//

#ifndef ELFLOADER_ELF_H
#define ELFLOADER_ELF_H

#include <vector>
#include "ElfHeader.h"
#include "ElfProgramHeader.h"
#include "ElfSectionHeader.h"

class Elf
{
public:
    ElfHeader header;
    std::vector<ElfProgramHeader> program_headers;
    std::vector<ElfSectionHeader> section_headers;
    std::string binary_data;
    std::string name;
};


#endif //ELFLOADER_ELF_H
