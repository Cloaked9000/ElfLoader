## What is it
Basically a userland exec. It forks, and then loads an elf binary into the forked process. More of a proof of concept than anything.

Example of a very basic ELF file, with the text beneath "Resuming child..." being the loaded ELF's output.

![Screenshot](https://frednicolson.co.uk/u/elf_load_img.png)

##  How it works
There are multiple steps, and it's a little complex, but to sum it up:

1. The loader forks into parent and child.
2. The parent waits on the child to enter a suspended state.
3. The child mmap's a chunk of memory large enough for a flat-binary loader and page allocation information needed for the new ELF.
4. The child jumps to the newly allocated loader, letting the loader deallocate all pages but itself and some kernel mapped memory.
5. The loader mmap's loadable sections exactly as specified by the new ELF file.
6. The loader suspends its own process, indicating that the parent should resume.
7. The parent resumes, before writing the loadable ELF sections directly into the child process.
8. The parent resumes the child. 
9. The child sets up the stack and then jumps to the program entry point, beginning execution of the loaded ELF.

## Building
The Loader must first be built using NASM, and the loader header file generated, this can be done using the following command whilst in the loader directory:
```sh
nasm -fbin loader.asm && xxd -i loader > loader.h
```
CMake can then be used to build  the rest of the loader.

## Limitations
1. No support for 32bit binaries.
2. No support for dynamic linking (statically link!).
3. Section flag permissions aren't obeyed. Everything is allocated using ```PROT_WRITE | PROT_EXEC``` which is not secure.
4. I have no clue how portable this is, or how well it'll work for complex programs.
