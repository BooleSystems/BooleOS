bits 32

global idt_flush
global irq0
global irq1

extern irq_handler

idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

irq0:
    push dword 0
    push dword 32
    pusha
    mov eax, 32
    push eax
    call irq_handler
    add esp, 4
    popa
    add esp, 8
    iret

irq1:
    push dword 0
    push dword 33
    pusha
    mov eax, 33
    push eax
    call irq_handler
    add esp, 4
    popa
    add esp, 8
    iret
