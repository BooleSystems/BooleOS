// nullos/kernel/memory/vmm.h
// Virtual Memory Manager — public interface

#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Page flags
#define VMM_PRESENT    0x01  // Page present
#define VMM_WRITABLE   0x02  // Read/write
#define VMM_USER       0x04  // Accessible from userland
#define VMM_KERNEL     (VMM_PRESENT | VMM_WRITABLE)

// Initializes paging and enables the CR0.PG bit
void vmm_init(void);

// Maps a virtual address -> physical
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

// Unmaps a virtual address
void vmm_unmap_page(uint32_t virt);

// Returns the physical address mapped to a virtual one (or 0 if unmapped)
uint32_t vmm_get_phys(uint32_t virt);

// Returns the kernel's page directory (physical)
uint32_t vmm_get_kernel_directory(void);

// Creates a new page directory cloning the kernel mapping
uint32_t vmm_create_directory(void);

// Switches the current page directory
void vmm_switch_directory(uint32_t cr3);

// Maps virt->phys in page directory pd_phys with user flags (RW + USER)
// pd_phys must be within the first 8MB (identity-mapped)
void vmm_map_user_page(uint32_t pd_phys, uint32_t virt, uint32_t phys);

// Resolves virt->phys in an arbitrary page directory (identity-mapped)
uint32_t vmm_get_phys_from_dir(uint32_t pd_phys, uint32_t virt);

// Debug
void vmm_dump(void);

#endif // VMM_H
