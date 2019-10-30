#include <iostream>
#include <fstream>
#include <ElfParser.h>
#include <ElfLoader.h>

int main(int argc, char *argv[], char *envp[])
{
    //Load ELF file
    std::string filepath = "hello";
    std::ifstream stream(filepath, std::ifstream::in | std::ifstream::binary);
    if(!stream.is_open())
        throw std::runtime_error("Couldn't open '" + filepath + "'");

    //Parse it
    ElfParser parser;
    Elf binary = parser.parse(stream, filepath);

    //Execute it
    ElfLoader loader;
    loader.exec(binary, argc, argv, envp);
    return 0;
}