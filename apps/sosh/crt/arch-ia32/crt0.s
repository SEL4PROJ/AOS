# @LICENSE(NICTA)

    .global _start
    .extern seL4_InitBootInfo
    .extern exit

    .text

_start:
    leal    _stack_top, %esp
    pushl   %ebx
    call    seL4_InitBootInfo
    addl    $4, %esp
    call    main
    pushl   %eax
    call    exit
1:  jmp     1b

    .bss
    .align  4

_stack_bottom:
    .space  1024
_stack_top:
