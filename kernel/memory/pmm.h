// nullos/kernel/memory/pmm.h
// Physical Memory Manager — interface pública

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096  // 4KB por página

// Inicializa o PMM com o mapa de memória do Multiboot2
// mem_upper = memória alta em KB (vem do multiboot)
void pmm_init(uint32_t mem_upper);

// Aloca uma página física (retorna endereço físico ou 0 se sem memória)
uint32_t pmm_alloc_page(void);

// Libera uma página física
void pmm_free_page(uint32_t addr);

// Retorna quantidade de páginas livres
uint32_t pmm_free_pages(void);

// Retorna quantidade total de páginas
uint32_t pmm_total_pages(void);

// Marca região de memória como usada (ex: kernel, IDT)
void pmm_mark_used(uint32_t addr, uint32_t size);

// Marca região de memória como livre
void pmm_mark_free(uint32_t addr, uint32_t size);

// Debug: imprime estado do PMM via VGA
void pmm_dump(void);

#endif // PMM_H
