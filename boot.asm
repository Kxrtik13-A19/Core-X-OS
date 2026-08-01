MAGIC    equ 0x1BADB002
FLAGS    equ (1<<0) | (1<<1) | (1<<2)   ; Request memory mapping and video mode
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0
    dd 0            ; Linear graphics mode
    dd 800          ; Width
    dd 600          ; Height
    dd 32           ; 32-bit color depth

section .bss
align 16
stack_bottom:
    resb 16384      ; 16KB Stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push ebx        ; Pass Multiboot Info structure pointer
    push eax        ; Pass Magic Number
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang