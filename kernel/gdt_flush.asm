; nullos/kernel/gdt_flush.asm
; Loads the GDT and reloads all segment registers
; Called by gdt_init() in gdt.c

bits 32
global gdt_flush

gdt_flush:
    ; Argument: address of gdt_ptr (via stack, cdecl convention)
    mov eax, [esp + 4]
    lgdt [eax]

    ; Reload data segment registers with the kernel data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with the kernel code selector (0x08)
    jmp 0x08:.flush

.flush:
    ret
