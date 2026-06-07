.section .text
.global _start
_start:
    xor %rbp, %rbp
    mov %rsp, %rdi
    call _init_c
    hlt
