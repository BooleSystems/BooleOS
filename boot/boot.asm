; nullos/boot/boot.asm
; Entry point do NullOS — header Multiboot2 + setup inicial antes de chamar kmain()

bits 32

; ============================================================
; Constantes Multiboot2
; ============================================================
MB2_MAGIC       equ 0xE85250D6
MB2_ARCH        equ 0           ; i386 protected mode
MB2_HEADER_LEN  equ (mb2_header_end - mb2_header)
MB2_CHECKSUM    equ -(MB2_MAGIC + MB2_ARCH + MB2_HEADER_LEN)

STACK_SIZE      equ 0x4000      ; 16 KB de stack

; ============================================================
; Seção de texto (código)
; ============================================================
section .multiboot2
align 8
mb2_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_HEADER_LEN
    dd MB2_CHECKSUM

    ; Tag de encerramento (obrigatório)
    align 8
    dw 0        ; type = 0 (end tag)
    dw 0        ; flags
    dd 8        ; size
mb2_header_end:

section .text
global _start
extern kmain

_start:
    ; Desabilita interrupções — ainda não temos IDT
    cli

    ; Configura stack
    mov esp, stack_top

    ; Salva o magic e o ponteiro da estrutura Multiboot2
    ; eax = magic (0x36d76289), ebx = endereço da struct multiboot_info
    push ebx        ; multiboot_info ptr
    push eax        ; magic

    ; Chama o kernel C
    call kmain

    ; Se kmain retornar por algum motivo, trava aqui
.hang:
    cli
    hlt
    jmp .hang

; ============================================================
; Seção BSS — stack do kernel
; ============================================================
section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:
