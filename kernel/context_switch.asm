bits 32

global context_switch

; void context_switch(uint32_t *old_esp, uint32_t new_esp)
; Saves the current callee-saved register frame into *old_esp,
; switches to new_esp, restores that frame, then returns on the new stack.
context_switch:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20] ; old_esp argument after 4 pushes
    mov edx, [esp + 24] ; new_esp argument after 4 pushes
    mov [eax], esp
    mov esp, edx

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
