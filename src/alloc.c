#define _DEFAULT_SOURCE
#include "alloc.h"
#include "config.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/syscall.h> 
#include <unistd.h>
#include <sys/mman.h>

// TODO : add allocator mutex to make it thread-safe ?

struct Chunk {
    size_t size;
    struct Chunk* next;
    bool is_free;
};

#define GET_PTR_FROM_CHUNK(chunk_ptr) (void*)(((unsigned char*)chunk_ptr)+sizeof(struct Chunk))  

#define GET_CHUNK_FROM_PTR(ptr) (struct Chunk*)(((unsigned char*)ptr)-sizeof(struct Chunk))

// TODO : do I really need to store the whole linked list or just the last (need to unmap it ?)
struct MmapZone {
    size_t size;
    struct MmapZone* next;
};

#define GET_PTR_FROM_MMAP_ZONE(mmap_ptr) (void*)(((unsigned char*)mmap_ptr)+sizeof(struct MmapZone))  
#define GET_MMAP_ZONE_FROM_PTR(ptr) (struct MmapZone*)(((unsigned char*)ptr)-sizeof(struct MmapZone))

// TODO : make it thread safe
struct MmapZone* mmap_zone_start = NULL;
struct MmapZone* current_mmap_zone = NULL;
size_t current_mmap_offset = 0;

size_t current_mmap_size = 32 * 1024;

__thread bool is_in_malloc = false;


#if LOGGING

// to prevent malloc
static size_t my_strlen(const char* s){
    size_t i = 0;
    while (*s++) i++;
    return i;
}

static void safe_log(const char* message){
    write(2, message, my_strlen(message));
}

#else 
static void safe_log(const char* message){
    (void)message;
}
#endif

static void* my_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off){
    return (void*)syscall(SYS_mmap, (unsigned long)addr, (unsigned long)len, (unsigned long)prot, (unsigned long)flags, (unsigned long)fildes, (unsigned long)off);
}

// pointer to new memory
void* get_more_memory(size_t bytes_to_add){
    // mmap ?
    // sbrk

    /*void* current_heap = sbrk(0);

    if (sbrk(bytes_to_add) == (void*)-1){
        return NULL;
    }

    return current_heap;*/

    // TODO : when bigger than page, just mmap

    if (current_mmap_zone && current_mmap_offset + bytes_to_add <= current_mmap_zone->size){
        void* ptr = (char*)GET_PTR_FROM_MMAP_ZONE(current_mmap_zone) + current_mmap_offset;
        current_mmap_offset += bytes_to_add;
        return ptr;
    } 

    struct MmapZone* mmap_zone = mmap_zone_start;
    while (mmap_zone && mmap_zone->next){
        mmap_zone = mmap_zone->next;
    }

    
    safe_log("NEW MMAP\n");
    void* mmap_ptr = my_mmap(NULL, current_mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // TODO : use the syscall directly (for reentrancy ? or check if the libc one does anything that could malloc ?)
    if (mmap_ptr == MAP_FAILED){
        return NULL;
    }
    
    struct MmapZone* new_mmap_zone = (struct MmapZone*)mmap_ptr;
    new_mmap_zone->size = current_mmap_size - sizeof(struct MmapZone);
    new_mmap_zone->next = NULL;
    if (mmap_zone_start){
        struct MmapZone* z = mmap_zone_start;
        while (z && z->next){
            z = z->next;
        }
        z->next = new_mmap_zone;
    } else {
        mmap_zone_start = new_mmap_zone;
    }
    current_mmap_zone = new_mmap_zone;

    current_mmap_offset = sizeof(struct MmapZone);
    current_mmap_size *= 2;

    
    return GET_PTR_FROM_MMAP_ZONE(new_mmap_zone);
}

// TODO : small bins : 16, 32, 64 bytes, medium bin ? (128, 256, 512 ?)

#define min(x, y) ({ \
    __typeof__(x) _x = (x);    \
    __typeof__(y) _y = (y); \
    (_x < _y) ? _x : _y; \
    })


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
        if (current_chunk->is_free && current_chunk->size >= size){
            break;
        }

        *last_chunk = current_chunk;
        current_chunk = current_chunk->next;
    }

    return current_chunk;
}

struct Chunk* new_chunk(size_t size){
    void* new_mem = get_more_memory(sizeof(struct Chunk) + size);
    if (!new_mem){
        return NULL;
    }
    struct Chunk* chunk = (struct Chunk*)new_mem;
    chunk->size = size;
    chunk->next = NULL;
    chunk->is_free = false;
    return chunk;
}

void* custom_malloc(size_t size){
    /*if (is_in_malloc){
        return NULL;
    }*/
    //is_in_malloc = true;
    size = align_allocation(size);
    struct Chunk* chunk = NULL;
    if (start_chunk == NULL){
        chunk = new_chunk(size);
        if (!chunk){
            //is_in_malloc = false;
            return NULL;
        }
        start_chunk = chunk;
    } else {
        struct Chunk* last_chunk = NULL;
        struct Chunk* current_chunk = find_free_block(size, &last_chunk);

        if (!current_chunk){
            // couldn't find free chunk
            chunk = new_chunk(size);
            if (!chunk){
                return NULL;
            }
            last_chunk->next = chunk;
        } else {
            current_chunk->is_free = false;
            chunk = current_chunk;
        }
    }

    //is_in_malloc = false;

    return GET_PTR_FROM_CHUNK(chunk);
}

// TODO : real realloc
void* custom_realloc(void* ptr, size_t new_size){
    void* new_ptr = custom_malloc(new_size);
    if (ptr){
        struct Chunk* old_alloc_chunk = GET_CHUNK_FROM_PTR(ptr);
        size_t copy_size = min(old_alloc_chunk->size, new_size); // for the case where new_size < old_alloc_chunk->size
        memcpy(new_ptr, ptr, copy_size);
        custom_free(ptr);
    }
    return new_ptr;
}

static void merge_block(struct Chunk* chunk){
    if (chunk->next && chunk->next->is_free && chunk->next == (struct Chunk*)((char*)GET_PTR_FROM_CHUNK(chunk) + chunk->size)){
        safe_log("MERGE BLOCK");
        chunk->size += sizeof(struct Chunk) + chunk->next->size;
        chunk->next = chunk->next->next;
    }
}

void custom_free(void* ptr){
    if (!ptr){
        return;
    }
    struct Chunk* chunk = GET_CHUNK_FROM_PTR(ptr);
    chunk->is_free = true;
    merge_block(chunk);
}

void* custom_calloc(size_t nmemb, size_t size){
    size_t total_size = size * nmemb; // TODO : overflow checks ?
    void* ptr = custom_malloc(total_size);
    if (!ptr){
        return NULL;
    }
    memset(ptr, 0, total_size);
    return ptr;
}


#if REPLACE_LIBC_FUNCTIONS
void* malloc(size_t size){
    return custom_malloc(size);
}

void* calloc(size_t nmemb, size_t size){
    return custom_calloc(nmemb, size);
}

void* realloc(void* ptr, size_t new_size){
    return custom_realloc(ptr, new_size);
}

void free(void* ptr){
    custom_free(ptr);
}

#endif