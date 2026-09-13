bits 32

global jump_to_usermode

; void jump_to_usermode(uint32_t entry, uint32_t user_esp)
;
; Jumps from ring 0 to ring 3 via iret.
; Builds the 5-word frame on the stack that iret expects:
;   [ss, esp, eflags, cs, eip]  (ss at the top of the stack before iret)
;
; Destination segments:
;   cs  = 0x1B  (SEG_USER_CODE | RPL=3)
;   ss/ds/es/fs/gs = 0x23  (SEG_USER_DATA | RPL=3)
;
; eflags: keeps the current flags and forces IF=1 (interrupts enabled in ring 3).

jump_to_usermode:
    mov eax, [esp + 4]      ; entry  (destination eip)
    mov ecx, [esp + 8]      ; user_esp

    ; Update data segments to ring 3 before iret
    mov dx, 0x23
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ; Build the iret frame (push bottom-up = ss first)
    push dword 0x23         ; ss  (user data, RPL=3)
    push ecx                ; esp (user stack pointer)
    pushf
    or dword [esp], 0x200   ; IF = 1  (enables interrupts in ring 3)
    push dword 0x1B         ; cs  (user code, RPL=3)
    push eax                ; eip (process entry point)

    iret                    ; does not return
