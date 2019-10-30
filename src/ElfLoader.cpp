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
#include <elf.h>
#include "../loader/loader.h"

uint64_t round_up(uint64_t number, uint64_t multiple)
{
    return number + (multiple - (number % multiple));
}

uint64_t round_down(uint64_t number, uint64_t multiple)
{
    return round_up(number, multiple) - multiple;
}

template<typename T>
inline bool contains(T range_begin, T range_end, T point)
{
    return point >= range_begin && point < range_end;
}

class AllocationBuilder
{
public:
    enum class Type : uint64_t
    {
        Alloc = 0,
        Dealloc = 1,
    };

    struct Alloc
    {
        Alloc(Type type, uintptr_t addr, uintptr_t len) : type(type), addr(addr), len(len)
        {}

        [[nodiscard]] static constexpr auto size()
        {
            return sizeof(type) + sizeof(addr) + sizeof(len);
        }

        Type type;
        uintptr_t addr;
        uintptr_t len;
    };

    template<typename ...Args>
    void add(Args &&...args)
    {
        allocations.emplace_back(std::forward<Args>(args)...);
    }

    std::string build()
    {
        const uint64_t len = allocations.size();
        std::string str((char*)&len, sizeof(len));
        for(const auto &alloc : allocations)
        {
            str.append((char*)&alloc.type, sizeof(alloc.type));
            str.append((char*)&alloc.addr, sizeof(alloc.addr));
            str.append((char*)&alloc.len, sizeof(alloc.len));
        }
        return str;
    }

    std::vector<Alloc> allocations;
};

typedef uint64_t (*LoaderFunc)(void *alloc_list_addr, uint64_t entry_point, uint64_t stack_end, uint64_t argc);
bool ElfLoader::exec(Elf elf, int argc, char *argv[], char *envp[])
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

        //Figure out which sections of memory need to be allocated/de-allocated.
        //To do this, first enumerate our own address space to figure out what needs to be free'd in the new process.
        AllocationBuilder alloc_builder;

        std::vector<Alloc> addr_space = get_process_allocations(getpid());
        for(const auto &alloc : addr_space)
        {
            if(alloc.type == Alloc::Type::other)
            {
                std::cout << "Dealloc: " << std::hex << "0x" << alloc.addr << ", " << std::dec << alloc.len << std::endl;
                alloc_builder.add(AllocationBuilder::Type::Dealloc, alloc.addr, alloc.len);
            }
        }

        //Now we need to figure out what needs to be allocated in the new process
        for(const auto &alloc : elf.program_headers)
        {
            if(alloc.type == ElfProgramHeader::Type::load || alloc.type == ElfProgramHeader::Type::tls)
            {
                std::cout << "Alloc: " << std::hex << "0x" << round_down(alloc.mem_offset, (size_t)getpagesize()) << ", " << std::dec << alloc.mem_size << std::endl;
                alloc_builder.add(AllocationBuilder::Type::Alloc, round_down(alloc.mem_offset, (size_t)getpagesize()), alloc.mem_size);
            }
        }

//        std::vector<Elf64_auxv_t> auxv;
//        char **ptr = envp;
//        while(ptr) ++ptr; ++ptr;
//        auto *current = (Elf64_auxv_t*)ptr;
//        while(current)
//        {
//            auxv.emplace_back(*current);
//        }

        //Allocate memory for the loader + the allocation info
        std::string alloc_info = alloc_builder.build();
        const auto payload_length = loader_len + alloc_info.size();
        auto *loader_addr = (uint8_t*)mmap((uintptr_t*)0x500000000, payload_length,  PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(loader_addr == MAP_FAILED)
        {
            std::cout << "Failed to mmap loader: " << errno << std::endl;
            return EXIT_FAILURE;
        }

        //Check that loader address is in unused space
        std::cout << "mmapping loader to: " << std::hex << (uintptr_t*)loader_addr << ". Length: " << std::dec << loader_len + alloc_info.size() << std::endl;
        for(const auto &alloc : alloc_builder.allocations)
        {
            if(((uintptr_t)loader_addr >= alloc.addr && (uintptr_t)loader_addr < alloc.addr + alloc.len) ||
               ((uintptr_t)loader_addr + payload_length >= alloc.addr && (uintptr_t)loader_addr + payload_length < alloc.addr + alloc.len) ||
               (alloc.addr >= (uintptr_t)loader_addr && alloc.addr + alloc.len < (uintptr_t)loader_addr + payload_length) ||
               (alloc.addr + alloc.len >= (uintptr_t)loader_addr && alloc.addr + alloc.len < (uintptr_t)loader_addr + payload_length))
            {
                std::cout << "Loader got mmapped into needed memory. " << (int)alloc.type << ", 0x" << std::hex << alloc.addr << ", " << std::dec << alloc.len << std::endl;
                return EXIT_FAILURE;
            }
        }

        //Write the loader binary
        memcpy(loader_addr, loader, loader_len);

        //Write alloc info
        memcpy(loader_addr + loader_len, alloc_info.data(), alloc_info.size());

        //Jump into the loader, we should not return from here
        ((LoaderFunc)loader_addr)(loader_addr + loader_len, elf.header.program_entry_pos, (uint64_t)argv, argc);
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
        if(section.type == ElfProgramHeader::Type::dynamic)
            std::cout << "Warn: Image contains dynamic sections!" << std::endl;
        if(section.type != ElfProgramHeader::Type::load && section.type != ElfProgramHeader::Type::tls)
            continue;

        write_to_pid(pid, &elf.binary_data[section.file_offset], section.file_size, (void*)section.mem_offset, section.file_size);
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
        else if(type == "heap") alloc.type = Alloc::Type::heap;
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
    ssize_t ret = process_vm_writev(pid, &local_vec, 1, &remote_vec, 1, 0);
    if(ret != local_vec.iov_len)
    {
        throw std::runtime_error("Failed to make remote write. Bytes written: " + std::to_string(ret) + "/" + std::to_string(local_vec.iov_len) + ". Errno: " + std::to_string(errno));
    }
}
