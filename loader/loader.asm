BITS 64     ; 64bit mode
default rel ; relative mode
SIGSTOP equ 19

; Pushes an auxv pair to the stack.
; Arg1: auxv type, use the AT_n macros above
; Arg2: The value of the auxv entry
%macro PUSH_AUX_ENT 2
    push %1
    push %2
%endmacro

; Calls sys_munmap. 
; Arg1: Address to start unmapping from. Must be page aligned.
; Arg2: Number of bytes to free. Must be a multiple of page size.
; Return value: Stored in rax
%macro sys_munmap 2
    mov rax, 11 ; sys_munmap
    mov rdi, %1
    mov rsi, %2
    syscall
%endmacro

; Calls sys_mmap. 
; Arg1: Address to allocate memory at
; Arg2: Number of bytes to allocate
; Arg3: Allocation protections
; Arg4: Allocation flags
; Arg5: File descriptor
; Arg6: Offset
; Return value: Stored in rax
%macro sys_mmap 6
    mov rax, 9 ; sys_mmap
    mov rdi, %1
    mov rsi, %2
    mov rdx, %3
    mov r10, %4
    mov r8,  %5
    mov r9,  %6
    syscall
%endmacro

; Calls sys_kill (sends a signal)
; Arg1: Pid to send a signal to
; Arg2: Signal to send
; Return value: Stored in rax
%macro sys_kill 2
    mov rax, 62 ; sys_kill
    mov rdi, %1
    mov rsi, %2
    syscall
%endmacro

; Calls sys_getpid (gets current PID)
; Return value: Stored in rax
%macro sys_getpid 0
mov rax, 39 ; sys_getpid
syscall
%endmacro

; Load parameters passed by caller
mov [alloc_list_addr], rdi
mov [entry_point],     rsi
mov [stack_end],       rdx
mov [desired_argc],    rcx

; Now we need to allocate memory for the new process. Pointer to beginning
; of list is in alloc_list_addr, process this structure, which looks like:
; entry_count, alloc_type, address_to_allocate_at, length_of_allocation, ....
mov r12, [alloc_list_addr]        ; Get pointer to alloc list
mov rcx, [r12]                    ; Move list length into rcx so we can loop over it
add r12, 8                        ; Skip to first entry in the list

map_loop:
mov r14, [r12]                    ; Get type of allocation (dealloc/alloc?)
add r12, 8                        ; Skip to next array value
mov rdi, [r12]                    ; Get address to allocate at
add r12, 8                        ; Skip to next array value
mov rsi, [r12]                    ; Get length of allocation
add r12, 8                        ; Skip to next array value

push rcx                          ; rcx will be lost by syscall, save it
mov r13, rdi                      ; Make a copy of the address we're allocating, into a preserved register

cmp r14, 0                        ; If this is an alloc, then jump to alloc branch
je alloc_branch
sys_munmap rdi, rsi               ; We didn't jump, so this is a dealloc entry. Call sys_munmap.
jmp skip_alloc                    ; Skip over the alloc block as we've just done a dealloc
alloc_branch:
sys_mmap rdi, rsi, 6, 50, -1, 0   ; Allocate memory using PROT_EXEC | PROT_WRITE. And mapping MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED
skip_alloc:

pop rcx                           ; restore rcx
loop map_loop                     ; Keep going through each entry

; Suspend ourselves, so that our parent can write in the sections
sys_getpid              ; Get our own pid so we can signal ourselves. Ret stored in RAX.
mov rdi, rax            ; Pid argument should be in RDI, so move from RAX
sys_kill rdi, SIGSTOP   ; Signal ourselves

; We must have been resumed, setup the stack/registers then jump to entry point
mov rsp, [stack_end]    ; Reset the stack pointer to our parent's argv, let's us skip setting up an auxv table
mov rdx, 0              ; Contains a function pointer to be registered with atexit, don't register any!
mov rbp, 0              ; rbp is expected to be 0
mov r9, [desired_argc]  ; Push correct argc value to top of stack, as parent will have popped this off
push r9
jmp [entry_point]       ; Jump to the entry point, yeet

; call sys_exit.
; rdi: Exit code of process
exit:
mov rax, 60
syscall

section	.data
    alloc_list_addr dq 0
    entry_point     dq 0
    stack_end       dq 0
    desired_argc    dq 0
