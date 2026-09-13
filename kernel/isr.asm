bits 32

global idt_flush
global irq0
global irq1
global irq14
global irq15
global isr128_resume

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

global isr128

extern irq_handler
extern exception_handler
extern syscall_handler
extern g_syscall_frame

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

; IRQ14/15 (ATA primary/secondary channels, PIC-remapped to 46/47)
irq14:
    push dword 0
    push dword 46
    pusha
    mov eax, 46
    push eax
    call irq_handler
    add esp, 4
    popa
    add esp, 8
    iret

irq15:
    push dword 0
    push dword 47
    pusha
    mov eax, 47
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

; -------------------------------------------------------
; int 0x80 — syscall gate (DPL=3, called from ring 3)
;
; Convention: eax=num, ebx=arg1, ecx=arg2, edx=arg3
; Return:     eax = value returned by syscall_handler
;
; When coming from ring 3, the CPU pushes the full frame
; (eip, cs, eflags, user_esp, user_ss) via the TSS.
; pusha/popa preserves all of the user's registers;
; the eax slot in the frame is overwritten with the return value.
;
; g_syscall_frame / isr128_resume (SYS_FORK support):
; right after 'pusha', ESP points at a 13-word block: the 8 pusha
; registers followed directly by the CPU's ring3->ring0 trap frame
; (eip, cs, eflags, user_esp, user_ss) — i.e. everything needed to
; resume this exact ring-3 execution point via 'popa; iret'. We save
; that ESP into g_syscall_frame so process_fork() (see process.c) can
; copy the whole block into a new child process's own kernel stack —
; with the eax slot forced to 0 — so the child's first scheduling
; lands on isr128_resume below and comes back out to ring 3 exactly
; where the parent's fork() call was, just with a different eax.
; g_syscall_frame is only ever valid synchronously for the syscall
; being dispatched right now — see the comment on it in syscall.c.
; -------------------------------------------------------
isr128:
    pusha                   ; saves eax,ecx,edx,ebx,esp,ebp,esi,edi
    mov [g_syscall_frame], esp
    ; after pusha: [esp+28]=eax  [esp+16]=ebx  [esp+24]=ecx  [esp+20]=edx
    mov eax, [esp + 28]     ; num   (original eax)
    mov ebx, [esp + 16]     ; arg1  (original ebx)
    mov ecx, [esp + 24]     ; arg2  (original ecx)
    mov edx, [esp + 20]     ; arg3  (original edx)
    push edx                ; cdecl: arg3 last
    push ecx                ; arg2
    push ebx                ; arg1
    push eax                ; num   (first)
    call syscall_handler
    add esp, 16
    mov [esp + 28], eax     ; write the return value into the pusha frame's EAX slot
isr128_resume:
    popa                    ; restore regs; eax = syscall return value (0 for a fresh fork() child)
    iret
