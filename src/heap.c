#include "heap.h"

static uint8_t heap_memory[HEAP_SIZE];
static uint32_t heap_top = 0;

typedef struct block_header {
    uint32_t size;
    int is_free;
    struct block_header* next;
} block_header_t;

static block_header_t* free_list = 0;

void heap_init(void) {
    heap_top = 0;
    free_list = (block_header_t*)heap_memory;
    free_list->size = HEAP_SIZE - sizeof(block_header_t);
    free_list->is_free = 1;
    free_list->next = 0;
}

void* kmalloc(uint32_t size) {
    // 4-byte hizalama
    size = (size + 3) & ~3;

    block_header_t* curr = free_list;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size >= size + sizeof(block_header_t) + 16) {
                block_header_t* next_block = (block_header_t*)((uint8_t*)curr + sizeof(block_header_t) + size);
                next_block->size = curr->size - size - sizeof(block_header_t);
                next_block->is_free = 1;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            curr->is_free = 0;
            return (void*)((uint8_t*)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }
    return 0; // Bellek yetersiz
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t* header = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    header->is_free = 1;

    // Bitişik boş blokları birleştir (Defragmentation)
    block_header_t* curr = free_list;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}
