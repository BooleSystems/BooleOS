// nullos/kernel/gdt.h
// Global Descriptor Table — interface pública

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Índices dos segmentos na GDT
#define GDT_NULL_SEG    0
#define GDT_KERNEL_CODE 1
#define GDT_KERNEL_DATA 2
#define GDT_USER_CODE   3
#define GDT_USER_DATA   4
#define GDT_ENTRIES     5

// Seletores de segmento (índice * 8 + RPL)
#define SEG_KERNEL_CODE 0x08  // Ring 0
#define SEG_KERNEL_DATA 0x10  // Ring 0
#define SEG_USER_CODE   0x1B  // Ring 3
#define SEG_USER_DATA   0x23  // Ring 3

// Inicializa e carrega a GDT
void gdt_init(void);

#endif // GDT_H
