#ifndef HEAP_H
#define HEAP_H

#include "os.h"

#define HEAP_SIZE (1024 * 1024) // 1 MB Dinamik Bellek Alanı

void heap_init(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);

#endif
