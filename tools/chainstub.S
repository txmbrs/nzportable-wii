.section .text.entry,"ax"
.global _start
_start:
    lis 1,0x9321
    addi 1,1,-32
    li 0,0
    stw 0,0(1)
    b relocate
