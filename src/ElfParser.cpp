//
// Created by fred.nicolson on 17/10/18.
//

#include <ElfParser.h>
#include <netinet/in.h>
#include <iostream>
#include <algorithm>

Elf ElfParser::parse(std::ifstream &elf_stream, std::string elf_name)
{
    //Throw exception on fail, don't continue
    elf_stream.exceptions(std::ifstream::badbit);

    //Read header, verifying magic
    ElfHeader header;
    {
        std::string magic(4, '\0');
        elf_stream.read(magic.data(), magic.size());
        if(magic != std::string{'\x7F', 'E', 'L', 'F'})
            throw std::logic_error("Bad ELF header");
    }

    //Read in the rest
    elf_stream.read((char*)&header.word, sizeof(header.word));
    size_t pointer_length = (header.word == ElfHeader::WordSize::b32) ? 4 : 8;
    elf_stream.read((char*)&header.endian, sizeof(header.word));
    elf_stream.read((char*)&header.version, sizeof(header.version));
    elf_stream.read((char*)&header.abi, sizeof(header.abi));
    elf_stream.seekg(8, std::ifstream::cur); //Skip reserved 8 bytes
    elf_stream.read((char*)&header.type, sizeof(header.type));
    elf_stream.read((char*)&header.arch, sizeof(header.arch));
    elf_stream.seekg(4, std::ifstream::cur); //Skip second version parameter
    elf_stream.read((char*)&header.program_entry_pos, pointer_length);
    elf_stream.read((char*)&header.program_header_table_pos, pointer_length);
    elf_stream.read((char*)&header.program_section_table_pos, pointer_length);
    elf_stream.read((char*)&header.flags, sizeof(header.flags));
    elf_stream.read((char*)&header.header_size, sizeof(header.header_size));
    elf_stream.read((char*)&header.program_header_table_entry_size, sizeof(header.program_header_table_entry_size));
    elf_stream.read((char*)&header.program_header_table_entry_count, sizeof(header.program_header_table_entry_count));
    elf_stream.read((char*)&header.section_header_table_entry_size, sizeof(header.section_header_table_entry_size));
    elf_stream.read((char*)&header.section_header_table_entry_count, sizeof(header.section_header_table_entry_count));
    elf_stream.read((char*)&header.section_header_name_index, sizeof(header.section_header_name_index));

    //Ensure that endianness is the same as ours
    if(htonl(47) == 47)
    {
        //Big endian
        if(header.endian != ElfHeader::Endian::big)
            throw std::logic_error("Can't load little-endian ELF file on big-endian system");
    }
    else
    {
        //Little endian
        if(header.endian != ElfHeader::Endian::little)
            throw std::logic_error("Can't load big-endian ELF file on little-endian system");
    }

    //Ensure that it's 64bit
    if(header.word != ElfHeader::WordSize::b64)
    {
        throw std::logic_error("Cannot load unsupported 32bit ELF file");
    }

    //Read program headers
    std::vector<ElfProgramHeader> program_headers;
    elf_stream.seekg(header.program_header_table_pos, std::ifstream::beg);
    for(size_t a = 0; a < header.program_header_table_entry_count; ++a)
    {
        ElfProgramHeader program_header = parse_program_header64(elf_stream);
        program_headers.emplace_back(program_header);
    }

    //Read section headers
    std::vector<ElfSectionHeader> section_headers;
    elf_stream.seekg(header.program_section_table_pos, std::ifstream::beg);
    for(size_t a = 0; a < header.section_header_table_entry_count; ++a)
    {
        ElfSectionHeader section_header = parse_section_header64(elf_stream);
        section_headers.emplace_back(section_header);
    }

    //Store parsed details into Elf object
    Elf elf;
    elf.header = header;
    elf.program_headers = std::move(program_headers);
    elf.section_headers = std::move(section_headers);
    elf.name = std::move(elf_name);
    elf_stream.seekg(0, std::ifstream::end);
    elf.binary_data.resize(static_cast<unsigned long>(elf_stream.tellg()));
    elf_stream.seekg(0, std::ifstream::beg);
    elf_stream.read(elf.binary_data.data(), elf.binary_data.size());

    //Read section header names from strtab section if it exists
    //We have a strtab section, so fill in names
    ElfSectionHeader *shstrtab = nullptr;
    if(elf.header.section_header_name_index < elf.section_headers.size())
    {
        for(auto &section : elf.section_headers)
        {
            if(section.type == ElfSectionHeader::Type::null)
                continue;
            auto &strtab = elf.section_headers[elf.header.section_header_name_index];
            section.name.append(&elf.binary_data.at(strtab.file_offset + section.name_strtab_offset));
        }
    }
    else
    {
        std::cout << "Invalid section name index found in header. Not filling in section names." << std::endl;
    }

    return elf;
}

ElfProgramHeader ElfParser::parse_program_header64(std::ifstream &elf_stream)
{
    ElfProgramHeader header{};
    elf_stream.read((char*)&header.type, sizeof(header.type));
    elf_stream.read((char*)&header.flags, sizeof(header.flags));
    elf_stream.read((char*)&header.file_offset, sizeof(header.file_offset));
    elf_stream.read((char*)&header.mem_offset, sizeof(header.file_offset));
    elf_stream.seekg(8, std::ifstream::cur); //Reserved
    elf_stream.read((char*)&header.file_size, sizeof(header.file_size));
    elf_stream.read((char*)&header.mem_size, sizeof(header.mem_size));
    elf_stream.read((char*)&header.alignment, sizeof(header.alignment));
    return header;
}

ElfSectionHeader ElfParser::parse_section_header64(std::ifstream &elf_stream)
{
    ElfSectionHeader header{};
    elf_stream.read((char*)&header.name_strtab_offset, sizeof(header.name_strtab_offset));
    elf_stream.read((char*)&header.type, sizeof(header.type));
    elf_stream.read((char*)&header.flags, sizeof(header.flags));
    elf_stream.read((char*)&header.mem_offset, sizeof(header.mem_offset));
    elf_stream.read((char*)&header.file_offset, sizeof(header.file_offset));
    elf_stream.read((char*)&header.file_size, sizeof(header.file_size));
    elf_stream.read((char*)&header.link, sizeof(header.link));
    elf_stream.read((char*)&header.info, sizeof(header.info));
    elf_stream.read((char*)&header.alignment, sizeof(header.alignment));
    elf_stream.read((char*)&header.entry_size, sizeof(header.entry_size));
    return header;
}
