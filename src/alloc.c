#define _DEFAULT_SOURCE
#include "alloc.h"
#include "config.h"
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

// pointer to new memory
void* get_more_memory(size_t bytes_to_add){
    // mmap ?
    // sbrk

    void* current_heap = sbrk(0);

    sbrk(bytes_to_add);
    return current_heap;
}

struct Chunk {
    size_t size;
    struct Chunk* next;
    bool is_free;
};

#define GET_PTR_FROM_CHUNK(chunk_ptr) (void*)(((unsigned char*)chunk_ptr)+sizeof(struct Chunk))  

#define GET_CHUNK_FROM_PTR(ptr) (struct Chunk*)(((unsigned char*)ptr)-sizeof(struct Chunk))  


struct Chunk* start_chunk = NULL;


size_t align_allocation(size_t size){
    size_t rem = size % ALLOC_ALIGNEMENT;
    if (rem == 0){
        return size;
    }
    return size + (ALLOC_ALIGNEMENT - rem);

    // could do that instead
    // return (size + ALLOC_ALIGNEMENT - 1) & ~(ALLOC_ALIGNEMENT - 1);   
}

struct Chunk* find_free_block(size_t size, struct Chunk** last_chunk){
    struct Chunk* current_chunk = start_chunk;
    while (current_chunk){
        if (current_chunk->is_free && current_chunk->size <= size){
            break;
        }

        *last_chunk = current_chunk;
        current_chunk = current_chunk->next;
    }

    return current_chunk;
}

struct Chunk* new_chunk(size_t size){
    void* new_mem = get_more_memory(sizeof(struct Chunk) + size);
    struct Chunk* chunk = (struct Chunk*)new_mem;
    chunk->size = size;
    chunk->next = NULL;
    chunk->is_free = false;
    return chunk;
}

void* custom_malloc(size_t size){
    size = align_allocation(size);
    struct Chunk* chunk = NULL;
    if (start_chunk == NULL){
        chunk = new_chunk(size);
        start_chunk = chunk;
    } else {
        struct Chunk* last_chunk = NULL;
        struct Chunk* current_chunk = find_free_block(size, &last_chunk);

        if (!current_chunk){
            // couldn't find free chunk
            chunk = new_chunk(size);
            last_chunk->next = chunk;
        } else {
            current_chunk->is_free = false;
            chunk = current_chunk;
        }
    }


    return GET_PTR_FROM_CHUNK(chunk);
}

// TODO : real realloc
void* custom_realloc(void* ptr, size_t new_size){
    void* new_ptr = custom_malloc(new_size);
    struct Chunk* old_alloc_chunk = GET_CHUNK_FROM_PTR(ptr);
    memcpy(new_ptr, ptr, old_alloc_chunk->size);
    custom_free(ptr);
    return new_ptr;
}

void custom_free(void* ptr){
    struct Chunk* chunk = GET_CHUNK_FROM_PTR(ptr);
    chunk->is_free = true;
}


#if REPLACE_LIBC_FUNCTIONS
void* malloc(size_t size){
    return custom_malloc(size);
}

void* realloc(void* ptr, size_t new_size){
    return custom_realloc(ptr, new_size);
}

void free(void* ptr){
    custom_free(ptr);
}

#endif