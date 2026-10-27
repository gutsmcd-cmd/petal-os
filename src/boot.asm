; Petal OS - multiboot 1 entry (from scratch)
MB_MAGIC equ 0x1BADB002
MB_FLAGS equ 0x00000007          ; page-align, mem info, video mode
section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd -(MB_MAGIC + MB_FLAGS)
    dd 0,0,0,0,0                 ; address fields (unused)
    dd 0                         ; linear graphics mode
    dd 1024, 768, 32             ; width, height, depth
section .bss
align 16
stack_bottom: resb 65536
stack_top:
section .text
global _start
extern kmain
_start:
    cli
    mov esp, stack_top
    fninit
    push ebx
    push eax
    call kmain
.hang: hlt
    jmp .hang
