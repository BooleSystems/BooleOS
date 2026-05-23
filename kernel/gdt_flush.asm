; nullos/kernel/gdt_flush.asm
; Carrega a GDT e recarrega todos os registradores de segmento
; Chamado por gdt_init() em gdt.c

bits 32
global gdt_flush

gdt_flush:
    ; Argumento: endereço do gdt_ptr (via stack, convenção cdecl)
    mov eax, [esp + 4]
    lgdt [eax]

    ; Recarrega registradores de segmento de dados com seletor kernel data (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump para recarregar CS com seletor kernel code (0x08)
    jmp 0x08:.flush

.flush:
    ret
