//
// Created by fred.nicolson on 17/10/18.
//

#include <ElfLoader.h>
#include <sys/mman.h>
#include <zconf.h>
#include <cstring>
#include <iostream>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <algorithm>
#include <sys/uio.h>
#include "../loader/loader.h"

uint64_t round_up(uint64_t number, uint64_t multiple)
{
    return number + (multiple - (number % multiple));
}

uint64_t round_down(uint64_t number, uint64_t multiple)
{
    return round_up(number, multiple) - multiple;
}

struct ElfMetadata
{
    ElfMetadata(Elf &elf)
    {
        struct Alloc
        {
            Alloc(uint64_t mem_offset_, uint64_t mem_size_)
                    : mem_offset(mem_offset_),
                      mem_size(mem_size_){}
            uint64_t mem_offset;
            uint64_t mem_size;
        };

        //Store allocation information for loadable program headers
        std::vector<Alloc> alloc;
        for(auto &header : elf.program_headers)
        {
            if(header.type != ElfProgramHeader::Type::load)
                continue;
            alloc.emplace_back(round_down(header.mem_offset, (size_t)getpagesize()), header.mem_size);
        }

        //Prepare info into a format that the Loader expects
        uint64_t alloc_count = alloc.size();
        alloc_info.append((char*)&alloc_count, sizeof(alloc_count));
        alloc_info.append((char*)alloc.data(), alloc.size() * sizeof(Alloc));
    }

    inline const char *data() const
    {
        return alloc_info.data();
    }

    inline size_t size() const
    {
        return alloc_info.size();
    }

    std::string alloc_info;
};

typedef uint64_t (*LoaderFunc)(uint64_t free_begin_p1, uint64_t free_len_p1, uint64_t free_begin_p2, uint64_t free_len_p2, void *alloc_list_addr, uint64_t entry_point);
bool ElfLoader::exec(Elf elf, int argc, char *argv[])
{
    //Fork, creating a new process
    int pid = fork();

    //If we're the child, write the loader into memory, then execute it
    if(pid == 0)
    {
        //Set new process name if we can
        if(argc > 0 && argv != nullptr)
        {
            size_t name_len = strlen(argv[0]);
            strncpy(argv[0], elf.name.data(), name_len);
        }

        //Prepare elf metadata to pass to loader
        ElfMetadata alloc_info(elf);

        //Allocate memory for the loader
        auto page_size = static_cast<uint64_t>(getpagesize());
        auto *loader_addr = (uint8_t*)mmap(nullptr, loader_len + alloc_info.size(),  PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(loader_addr == MAP_FAILED)
            return EXIT_FAILURE;

        //Write the loader binary
        memcpy(loader_addr, loader, loader_len);

        //Write alloc metadata
        memcpy(loader_addr + loader_len, alloc_info.data(), alloc_info.size());

        //Get process memory allocations so we know where we can free up to (only free up until the stack, no more)
        std::vector<Alloc> allocations = get_process_allocations(getpid());
        uintptr_t stack_addr = std::find_if(allocations.begin(), allocations.end(), [](const Alloc &alloc) {
            return alloc.type == Alloc::Type::stack;
        })->addr;

        //Figure out the address range to munmap. I cba doing this in asm, so do it here and pass it as arguments.
        uint64_t free_begin_p1 = 0;
        uint64_t free_len_p1 = round_up((uint64_t)loader_addr, page_size) - page_size;
        uint64_t free_begin_p2 = round_up((uint64_t)loader_addr + loader_len, page_size);
        uint64_t free_len_p2 = stack_addr - free_begin_p2;

        //Jump into the loader, we should not return from here
        ((LoaderFunc)loader_addr)(free_begin_p1, free_len_p1, free_begin_p2, free_len_p2, loader_addr + loader_len, elf.header.program_entry_pos);
        abort();
    }

    //Wait for child to initialise. It should suspend itself on success.
    std::cout << "Child with PID " << pid << " spawned. Waiting for it to initialise and suspend." << std::endl;
    int status;
    waitpid(pid, &status, WUNTRACED);
    if(WIFEXITED(status) || WIFSIGNALED(status))
    {
        std::cout << "Child failed to initialise. Failed." << std::endl;
        return false;
    }

    //Child is now ready to have new code written into it, write the program headers
    std::cout << "Child suspended. Writing new sections..." << std::endl;
    for(auto &section : elf.program_headers)
    {
        //Only write sections marked as loadable
        if(section.type != ElfProgramHeader::Type::load)
            continue;

        write_to_pid(pid, &elf.binary_data[section.file_offset], section.file_size, (void*)section.mem_offset, section.file_size);
        std::cout << "Write program header " << section.mem_offset << ": " << section.file_size << std::endl;
    }

    //Sections are now written, resume the child
    std::cout << "Write complete. Resuming child..." << std::endl;
    kill(pid, SIGCONT);

    //Wait for child to finish
    waitpid(pid, &status, 0);
    std::cout << "Child exited with: " << status << std::endl;
    return true;
}

std::vector<ElfLoader::Alloc> ElfLoader::get_process_allocations(int pid)
{
    //Open /proc/pid/maps
    const std::string filepath = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream stream(filepath, std::ifstream::in | std::ifstream::binary);
    if(!stream.is_open())
        throw std::runtime_error("Couldn't open '" + filepath + "': " + std::to_string(errno));

    //Read it into map_data
    std::vector<std::string> map_data;
    {
        std::string buff;
        while(std::getline(stream, buff))
        {
            map_data.emplace_back(std::move(buff));
            buff.clear();
        }
    }

    //Parse it into an allocations list
    std::vector<ElfLoader::Alloc> allocations;
    for(auto &line : map_data)
    {
        //Find start/end in string of addresses. Should look something like: 562510a17000-562510a20000 r-xp
        auto start_end = line.find('-');
        auto alloc_end = line.find(' ', start_end);

        //Find type, should look like [stack], or /a/filepath/to/a/library
        auto type_start = line.find('[');
        auto type_end = line.find(']');
        std::string type = line.substr(type_start + 1, type_end - type_start - 1);

        //Extract addresses
        std::string start_address = line.substr(0, start_end);
        std::string end_address = line.substr(start_end + 1, alloc_end - start_end - 1);

        //Pack
        Alloc alloc{};
        alloc.addr = std::stoull(start_address, nullptr, 16);
        alloc.len = std::stoull(end_address, nullptr, 16) - alloc.addr;
        alloc.type = Alloc::Type::other;
        if(type == "stack") alloc.type = Alloc::Type::stack;
        else if(type == "vvar") alloc.type = Alloc::Type::vvar;
        else if(type == "vdso") alloc.type = Alloc::Type::vdso;
        else if(type == "vsyscall") alloc.type = Alloc::Type::vsyscall;
        allocations.emplace_back(alloc);
    }

    return allocations;
}

void ElfLoader::write_to_pid(int pid, void *src_addr, size_t src_len, void *dest_addr, size_t dest_len)
{
    iovec local_vec{};
    iovec remote_vec{};
    local_vec.iov_base = src_addr;
    local_vec.iov_len = src_len;
    remote_vec.iov_base = dest_addr;
    remote_vec.iov_len = dest_len;
    if(process_vm_writev(pid, &local_vec, 1, &remote_vec, 1, 0) != local_vec.iov_len)
    {
        throw std::runtime_error("Failed to make remote write. Errno: " + std::to_string(errno));
    }
}
