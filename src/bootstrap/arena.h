#ifndef YIS_ARENA_H
#define YIS_ARENA_H

#include <stddef.h>

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t cap;
    size_t used;
    unsigned char data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
} Arena;

void arena_init(Arena *arena);
void arena_free(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
void *arena_alloc_zero(Arena *arena, size_t size);

#endif
