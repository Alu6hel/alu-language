/* Slice 16: Custom Linker Script Sorcery */
ENTRY(_start)

SECTIONS
{
    /* Force kernel modules into exact physical RAM addresses */
    . = 0x100000;
    
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
