bits 32

global idt_flush
global irq0
global irq1

; CPU exception stubs
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31

extern irq_handler
extern exception_handler

; -------------------------------------------------------
; idt_flush / lidt
; -------------------------------------------------------
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

; -------------------------------------------------------
; IRQ handlers (hardware interrupts, PIC-remapped to 32+)
; -------------------------------------------------------
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

; -------------------------------------------------------
; Common exception stub
;
; Stack layout when we arrive here (after macro pushes):
;   [esp+0]  int_no
;   [esp+4]  err_code  (real or dummy 0)
;   [esp+8]  EIP       <- pushed by CPU
;   [esp+12] CS
;   [esp+16] EFLAGS
;
; After pusha (8 regs * 4 = 32 bytes):
;   [esp+0..28] saved regs
;   [esp+32]    int_no
;   [esp+36]    err_code
;   [esp+40]    EIP
; -------------------------------------------------------
isr_exc_common:
    pusha
    mov eax, [esp + 32]   ; int_no
    mov ecx, [esp + 36]   ; err_code
    mov edx, [esp + 40]   ; EIP (from CPU frame)
    push edx              ; arg3: eip
    push ecx              ; arg2: err_code
    push eax              ; arg1: int_no
    call exception_handler
    add esp, 12
    popa
    add esp, 8            ; discard int_no + err_code
    iret

; -------------------------------------------------------
; Macros for exception stubs
; -------------------------------------------------------

; Exceptions that do NOT push an error code — push dummy 0
%macro ISR_NOERRCODE 1
isr%1:
    push dword 0
    push dword %1
    jmp isr_exc_common
%endmacro

; Exceptions that DO push an error code (CPU already pushed it)
%macro ISR_ERRCODE 1
isr%1:
    push dword %1
    jmp isr_exc_common
%endmacro

; -------------------------------------------------------
; x86 exception table (Intel SDM vol.3 table 6-1)
; -------------------------------------------------------
ISR_NOERRCODE  0   ; #DE  Divide Error
ISR_NOERRCODE  1   ; #DB  Debug
ISR_NOERRCODE  2   ;      NMI
ISR_NOERRCODE  3   ; #BP  Breakpoint
ISR_NOERRCODE  4   ; #OF  Overflow
ISR_NOERRCODE  5   ; #BR  Bound Range Exceeded
ISR_NOERRCODE  6   ; #UD  Invalid Opcode
ISR_NOERRCODE  7   ; #NM  Device Not Available
ISR_ERRCODE    8   ; #DF  Double Fault        (always err=0)
ISR_NOERRCODE  9   ;      Coprocessor Overrun (legacy)
ISR_ERRCODE   10   ; #TS  Invalid TSS
ISR_ERRCODE   11   ; #NP  Segment Not Present
ISR_ERRCODE   12   ; #SS  Stack-Segment Fault
ISR_ERRCODE   13   ; #GP  General Protection Fault
ISR_ERRCODE   14   ; #PF  Page Fault
ISR_NOERRCODE 15   ;      Reserved
ISR_NOERRCODE 16   ; #MF  x87 Floating-Point
ISR_ERRCODE   17   ; #AC  Alignment Check
ISR_NOERRCODE 18   ; #MC  Machine Check
ISR_NOERRCODE 19   ; #XM  SIMD Floating-Point
ISR_NOERRCODE 20   ; #VE  Virtualization
ISR_NOERRCODE 21   ;      Reserved
ISR_NOERRCODE 22   ;      Reserved
ISR_NOERRCODE 23   ;      Reserved
ISR_NOERRCODE 24   ;      Reserved
ISR_NOERRCODE 25   ;      Reserved
ISR_NOERRCODE 26   ;      Reserved
ISR_NOERRCODE 27   ;      Reserved
ISR_NOERRCODE 28   ;      Reserved
ISR_NOERRCODE 29   ;      Reserved
ISR_NOERRCODE 30   ;      Reserved
ISR_NOERRCODE 31   ;      Reserved
