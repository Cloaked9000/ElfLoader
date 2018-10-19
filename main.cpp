#include <iostream>
#include <fstream>
#include <ElfParser.h>
#include <sys/mman.h>
#include <cstring>
#include <zconf.h>
#include <sys/syscall.h>
#include <signal.h>
#include <chrono>
#include <thread>
#include <sys/types.h>
#include <ElfLoader.h>

int main(int argc, char *argv[])
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
    loader.exec(binary, argc, argv);

    return 0;
}