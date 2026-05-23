// nullos/kernel/idt.h
// Interrupt Descriptor Table — interface pública

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

// Registros salvos pelo handler de interrupção (ordem do push em isr.asm)
typedef struct {
    uint32_t ds;                            // Segmento de dados
    uint32_t edi, esi, ebp, esp_dummy;      // pusha
    uint32_t ebx, edx, ecx, eax;           // pusha (continuação)
    uint32_t int_no;                        // Número da interrupção
    uint32_t err_code;                      // Código de erro (ou 0)
    uint32_t eip, cs, eflags, useresp, ss; // Empurrados pela CPU
} __attribute__((packed)) registers_t;

// Tipo de handler de interrupção
typedef void (*isr_handler_t)(registers_t *regs);

// Inicializa e carrega a IDT
void idt_init(void);

// Registra um handler para uma IRQ específica
void idt_register_handler(uint8_t irq, isr_handler_t handler);

#endif // IDT_H
