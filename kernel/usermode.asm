bits 32

global jump_to_usermode

; void jump_to_usermode(uint32_t entry, uint32_t user_esp)
;
; Salta de ring 0 para ring 3 via iret.
; Monta na stack o frame de 5 palavras que iret espera:
;   [ss, esp, eflags, cs, eip]  (ss no topo da stack antes do iret)
;
; Segmentos de destino:
;   cs  = 0x1B  (SEG_USER_CODE | RPL=3)
;   ss/ds/es/fs/gs = 0x23  (SEG_USER_DATA | RPL=3)
;
; eflags: mantém flags atuais e força IF=1 (interrupções habilitadas em ring 3).

jump_to_usermode:
    mov eax, [esp + 4]      ; entry  (eip de destino)
    mov ecx, [esp + 8]      ; user_esp

    ; Atualiza segmentos de dados para ring 3 antes de iret
    mov dx, 0x23
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ; Monta frame para iret (push de baixo para cima = ss primeiro)
    push dword 0x23         ; ss  (user data, RPL=3)
    push ecx                ; esp (user stack pointer)
    pushf
    or dword [esp], 0x200   ; IF = 1  (habilita interrupções em ring 3)
    push dword 0x1B         ; cs  (user code, RPL=3)
    push eax                ; eip (entry point do processo)

    iret                    ; não retorna
