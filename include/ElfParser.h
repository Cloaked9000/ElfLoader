//
// Created by fred.nicolson on 17/10/18.
//

#ifndef ELFLOADER_ELFPARSER_H
#define ELFLOADER_ELFPARSER_H
#include <fstream>
#include "ElfHeader.h"
#include "Elf.h"
#include "ElfSectionHeader.h"

class ElfParser
{
public:
    /*!
     * Parses an ELF file into usable structures
     *
     * @throws An std::exception on failure
     * @param elf_stream Data stream to load, containing the ELF data
     * @param name The name of the ELF file
     * @return The parsed ELF header
     */
    Elf parse(std::ifstream &elf_stream, std::string name);

private:

    ElfProgramHeader parse_program_header64(std::ifstream &elf_stream);
    ElfSectionHeader parse_section_header64(std::ifstream &elf_stream);
};


#endif //ELFLOADER_ELFPARSER_H
