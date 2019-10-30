//
// Created by fred.nicolson on 17/10/18.
//

#ifndef ELFLOADER_ELFLOADER_H
#define ELFLOADER_ELFLOADER_H


#include "Elf.h"

class ElfLoader
{
public:
    ElfLoader()= default;

    /*!
     * Exec's an ELF file
     *
     * @param elf The parsed ELF file
     * @param argc argc value. Number of elements in argv. But you know that. May be 0.
     * @param argv argv value. This should be the one passed to your main, so the child can be renamed. May be nullptr.
     * @param envp Environmental variables for the child.
     * @return True on success, false on failure
     */
    bool exec(Elf elf, int argc = 0, char *argv[] = nullptr, char *envp[] = nullptr);


private:
    struct Alloc
    {
        enum class Type
        {
            vvar,
            vdso,
            vsyscall,
            stack,
            heap,
            other
        };
        uintptr_t addr;
        size_t len;
        Type type;
    };

    /*!
     * Gets a list of memory allocations belonging to a process
     *
     * @throws An std::exception on failure
     * @param pid Pid of the process to check
     * @return A list of allocations on success
     */
    std::vector<Alloc> get_process_allocations(int pid);

    void write_to_pid(int pid, void *src_addr, size_t src_len, void *dest_addr, size_t dest_len);
};


#endif //ELFLOADER_ELFLOADER_H
