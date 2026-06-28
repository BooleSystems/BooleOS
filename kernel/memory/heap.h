// nullos/kernel/memory/heap.h
#ifndef HEAP_H
#define HEAP_H
#include <stdint.h>
#include <stddef.h>
void     heap_init(void);
void    *kmalloc(size_t size);
void     kfree(void *ptr);
void     heap_dump(void);
uint32_t heap_free_bytes(void);
#endif
