bits 32

global context_switch

; void context_switch(uint32_t *old_esp, uint32_t new_esp, uint32_t new_cr3)
; Saves the current callee-saved register frame into *old_esp,
; switches to new_esp and new_cr3, restores that frame, then returns.
context_switch:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20] ; old_esp
    mov edx, [esp + 24] ; new_esp
    mov ecx, [esp + 28] ; new_cr3

    mov [eax], esp
    
    ; Switch CR3 if new_cr3 is not 0
    test ecx, ecx
    jz .skip_cr3
    mov cr3, ecx
.skip_cr3:

    mov esp, edx

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
