.global _start
_start:
    li a0, 1
    la a1, msg
    li a2, 17
    li a7, 16
    ecall
1:
    j 1b

msg:
    .ascii "Hello from user!\n"