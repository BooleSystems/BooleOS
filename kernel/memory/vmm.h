// booleos/kernel/memory/vmm.h
// Virtual Memory Manager — public interface

#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Page flags
#define VMM_PRESENT    0x01  // Page present
#define VMM_WRITABLE   0x02  // Read/write
#define VMM_USER       0x04  // Accessible from userland
#define VMM_KERNEL     (VMM_PRESENT | VMM_WRITABLE)
// Bit 9, one of the three PTE bits the CPU leaves to the OS: a user page
// shared copy-on-write after fork(). Always paired with WRITABLE clear. A
// read-only page WITHOUT this bit is read-only for its own reasons, and a
// write to it is a genuine fault.
#define VMM_COW        0x200

// Error codes returned (negated) by the mapping functions
#define VMM_ERR_RANGE  (-1)  // virt is inside the kernel's first 8MB (user mappings)
#define VMM_ERR_NOMEM  (-2)  // no memory for a new page table

// Initializes paging and enables the CR0.PG bit
void vmm_init(void);

// Maps a virtual address -> physical in the kernel directory.
// Returns 0 on success, VMM_ERR_NOMEM if the page-table pool is exhausted.
int vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

// Unmaps a virtual address
void vmm_unmap_page(uint32_t virt);

// Returns the physical address mapped to a virtual one (or 0 if unmapped)
uint32_t vmm_get_phys(uint32_t virt);

// Returns the kernel's page directory (physical)
uint32_t vmm_get_kernel_directory(void);

// Creates a new page directory cloning the kernel mapping
uint32_t vmm_create_directory(void);

// Frees a process's whole address space: drops its reference to every user
// page (a page shared copy-on-write stays allocated until its last sharer
// lets go), frees each per-process page table (PDE >= 2 with VMM_USER), then
// frees the directory page itself. The shared kernel page tables behind PDE
// 0/1 are never touched. If pd_phys is the directory currently loaded in CR3,
// CR3 is switched to the kernel directory first (it maps the same 0-8MB, so
// the caller's kernel code, stack and data stay reachable). One
// interrupt-off section from start to end. Does nothing for 0 or for the
// kernel directory. The caller must drop every stored copy of pd_phys.
void vmm_destroy_directory(uint32_t pd_phys);

// Switches the current page directory
void vmm_switch_directory(uint32_t cr3);

// Maps virt->phys in page directory pd_phys with user flags (RW + USER)
// pd_phys must be within the first 8MB (identity-mapped).
// Returns 0 on success, or a negative VMM_ERR_* code on failure (nothing
// was mapped). The caller still owns `phys` on failure and must free it.
int vmm_map_user_page(uint32_t pd_phys, uint32_t virt, uint32_t phys);

// Same as vmm_map_user_page(), with the PTE's low flag bits given explicitly
// (VMM_PRESENT and VMM_USER are always added). fork() uses it to map a shared
// page read-only + VMM_COW into the child.
int vmm_map_user_page_flags(uint32_t pd_phys, uint32_t virt, uint32_t phys, uint32_t flags);

// Pointer to the PTE mapping virt in pd_phys, or 0 unless the PDE and the PTE
// are both present and VMM_USER. The pointer is into an identity-mapped page
// table.
uint32_t *vmm_get_user_pte(uint32_t pd_phys, uint32_t virt);

// Makes the page at virt privately writable if it is a copy-on-write page:
// the last owner gets the page back writable in place, any other owner gets
// a fresh copy and drops its reference to the shared one. Runs with
// interrupts off. Used by the page-fault handler and by every kernel write
// into user memory (the kernel writes through physical addresses, so the
// read-only PTE alone would not stop it from writing into the shared page).
// Returns 1 if a copy-on-write page was resolved, 0 if virt is not one (not
// mapped, or not VMM_COW), -1 if out of memory for the copy.
int vmm_cow_break(uint32_t pd_phys, uint32_t virt);

// Resolves virt->phys in an arbitrary page directory (identity-mapped)
uint32_t vmm_get_phys_from_dir(uint32_t pd_phys, uint32_t virt);

// Same as above, but returns 0 unless the mapping also carries VMM_USER
// (on both the PDE and the PTE) — i.e. only resolves addresses actually
// meant to be reachable from ring 3. Use this, not vmm_get_phys_from_dir(),
// to validate a userland-supplied pointer before a syscall reads/writes
// through it: vmm_get_phys_from_dir() would also happily resolve the
// shared kernel identity map (0-8MB) that every process's directory
// clones, since that mapping IS present — just not meant for userland.
uint32_t vmm_get_user_phys_from_dir(uint32_t pd_phys, uint32_t virt);

// Debug
void vmm_dump(void);

#endif // VMM_H
