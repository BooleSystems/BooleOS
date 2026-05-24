// nullos/kernel/memory/vmm.h
// Virtual Memory Manager — interface pública

#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Flags de página
#define VMM_PRESENT    0x01  // Página presente
#define VMM_WRITABLE   0x02  // Leitura/escrita
#define VMM_USER       0x04  // Acessível pelo userland
#define VMM_KERNEL     (VMM_PRESENT | VMM_WRITABLE)

// Inicializa paginação e ativa o bit CR0.PG
void vmm_init(void);

// Mapeia endereço virtual -> físico
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

// Desmapeia endereço virtual
void vmm_unmap_page(uint32_t virt);

// Retorna endereço físico mapeado a um virtual (ou 0 se não mapeado)
uint32_t vmm_get_phys(uint32_t virt);

// Debug
void vmm_dump(void);

#endif // VMM_H
