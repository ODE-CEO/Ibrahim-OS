MBALIGN     equ  1<<0
MEMINFO     equ  1<<1
VIDINFO     equ  1<<2  ; VBE Grafik modunu iste
MBFLAGS     equ  MBALIGN | MEMINFO | VIDINFO
MAGIC       equ  0x1BADB002
CHECKSUM    equ -(MAGIC + MBFLAGS)

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0
    dd 0    ; 0 = Linear Grafik Modu
    dd 640  ; Genişlik
    dd 480  ; Yükseklik
    dd 32   ; 32-bit Renk (True Color)

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push ebx ; Multiboot bilgisinin bellekteki adresini C koduna gönder
    push eax
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
