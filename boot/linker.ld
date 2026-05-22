/* nullos/boot/linker.ld
   Linker script do NullOS — x86_64-elf
   O kernel é linkado a partir de 1MB (0x100000), que é onde o GRUB carrega. */

ENTRY(_start)

SECTIONS
{
    /* Começa em 1MB */
    . = 0x100000;

    /* Header Multiboot2 — precisa estar nos primeiros 32KB do arquivo */
    .multiboot2 :
    {
        *(.multiboot2)
    }

    /* Código */
    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }

    /* Dados somente leitura (strings, constantes) */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Dados inicializados */
    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    /* Dados não inicializados (BSS) — stack fica aqui */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Descarta seções desnecessárias */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
