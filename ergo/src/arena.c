#include "arena.h"

#include <stdlib.h>
#include <string.h>

static ArenaBlock *arena_new_block(size_t cap) {
    ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + cap);
    if (!block) {
        return NULL;
    }
    block->next = NULL;
    block->cap = cap;
    block->used = 0;
    return block;
}

void arena_init(Arena *arena) {
    arena->head = NULL;
}

void arena_free(Arena *arena) {
    ArenaBlock *block = arena->head;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
}

void *arena_alloc(Arena *arena, size_t size) {
    const size_t align = sizeof(void *);
    size = (size + align - 1) & ~(align - 1);
    if (!arena->head || arena->head->used + size > arena->head->cap) {
        size_t cap = size > 4096 ? size : 4096;
        ArenaBlock *block = arena_new_block(cap);
        if (!block) {
            return NULL;
        }
        block->next = arena->head;
        arena->head = block;
    }
    void *ptr = arena->head->data + arena->head->used;
    arena->head->used += size;
    return ptr;
}

void *arena_alloc_zero(Arena *arena, size_t size) {
    void *ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
