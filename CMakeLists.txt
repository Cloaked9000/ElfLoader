cmake_minimum_required(VERSION 3.12)
project(ElfLoader)
include_directories(include)

set(CMAKE_CXX_STANDARD 17)

add_executable(ElfLoader main.cpp src/ElfParser.cpp include/ElfParser.h include/ElfHeader.h include/Elf.h include/ElfProgramHeader.h src/ElfLoader.cpp include/ElfLoader.h loader/loader.h)