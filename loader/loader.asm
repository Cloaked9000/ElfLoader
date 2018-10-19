BITS 64     ; 64bit mode
default rel ; relative mode
%define SIGSTOP 19

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
mov [free_begin_p1],   rdi
mov [free_len_p1],     rsi
mov [free_begin_p2],   rdx
mov [free_len_p2],     rcx
mov [alloc_list_addr], r8
mov [entry_point],     r9

; First thing we need to do is unmap all memory around us. Do the stuff before us first.
sys_munmap [free_begin_p1], [free_len_p1]
cmp rax, 0
jne exit

; Now unmap everything after us
sys_munmap [free_begin_p2], [free_len_p2]
cmp rax, 0
jne exit

; Now we need to allocate memory for the new process. Pointer to beginning
; of list is in alloc_list_addr, process this structure, which looks like:
; entry_count, address_to_allocate_at, length_of_allocation, ....
mov r12, [alloc_list_addr]        ; Get pointer to alloc list
mov rcx, [r12]                    ; Move list length into rcx so we can loop over it
add r12, 8                        ; Skip to first entry in the list

map_loop:
mov rdi, [r12]                    ; Get address to allocate at
add r12, 8                        ; Skip to next array value
mov rsi, [r12]                    ; Get length of allocation
add r12, 8                        ; Skip to next array value

push rcx                          ; rcx will be lost by syscall, save it
mov r13, rdi                      ; Make a copy of the address we're allocating, into a preserved register
sys_mmap rdi, rsi, 6, 50, -1, 0   ; Allocate memory using PROT_EXEC | PROT_WRITE. And mapping MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED
pop rcx                           ; restore rcx
cmp rax, r13                      ; Verify that the memory address was allocated where we want it
jne exit
loop map_loop                     ; Keep going through each entry

; Suspend ourselves, so that our parent can write in the sections
sys_getpid              ; Get our own pid so we can signal ourselves. Ret stored in RAX.
mov rdi, rax            ; Pid argument should be in RDI, so move from RAX
sys_kill rdi, SIGSTOP   ; Signal ourselves

; We must have been resumed, jump to the entry point
jmp [entry_point]

; call sys_exit.
; rdi: Exit code of process
exit:
mov rax, 60
syscall

section	.data
    free_begin_p1   dq 0
    free_len_p1     dq 0
    free_begin_p2   dq 0
    free_len_p2     dq 0
    alloc_list_addr dq 0
    entry_point     dq 0
    
