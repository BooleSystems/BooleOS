; nullos/boot/boot.asm
; NullOS entry point — Multiboot2 header + initial setup before calling kmain()

bits 32

; ============================================================
; Multiboot2 constants
; ============================================================
MB2_MAGIC       equ 0xE85250D6
MB2_ARCH        equ 0           ; i386 protected mode
MB2_HEADER_LEN  equ (mb2_header_end - mb2_header)
MB2_CHECKSUM    equ 0x100000000 - (MB2_MAGIC + MB2_ARCH + MB2_HEADER_LEN)

STACK_SIZE      equ 0x4000      ; 16 KB stack

; ============================================================
; Text section (code)
; ============================================================
section .multiboot2
align 8
mb2_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_HEADER_LEN
    dd MB2_CHECKSUM

    ; End tag (mandatory)
    align 8
    dw 0        ; type = 0 (end tag)
    dw 0        ; flags
    dd 8        ; size
mb2_header_end:

section .text
global _start
extern kmain

_start:
    ; Disable interrupts — we don't have an IDT yet
    cli

    ; Set up the stack
    mov esp, stack_top

    ; Save the magic value and the Multiboot2 structure pointer
    ; eax = magic (0x36d76289), ebx = address of the multiboot_info struct
    push ebx        ; multiboot_info ptr
    push eax        ; magic

    ; Call the C kernel
    call kmain

    ; If kmain returns for any reason, hang here
.hang:
    cli
    hlt
    jmp .hang

; ============================================================
; BSS section — kernel stack
; ============================================================
section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:
