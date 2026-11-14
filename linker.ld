ENTRY(_start)

SECTIONS
{
    /* Kernel 1MB noktasında başlar (Korumalı mod standardı) */
    . = 1M;

    /* Önce Multiboot başlığı, ardından kodlar (.text) */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Salt okunur veriler (Stringler vb.) */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Başlangıç değeri atanmış veriler */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Başlangıç değeri atanmamış veriler ve Stack (.bss) */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
