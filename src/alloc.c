#define _DEFAULT_SOURCE
#include "alloc.h"
#include "config.h"
#include "alloc_internal.h"
#include "logging.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/syscall.h> 
#include <unistd.h>
#include <sys/mman.h>

// TODO : add allocator mutex to make it thread-safe ?



// TODO : make it thread safe
struct MmapZone* mmap_zone_start = NULL;
struct MmapZone* current_mmap_zone = NULL;
size_t current_mmap_offset = 0;

size_t current_mmap_size = 32 * 1024;

// TODO
/*const size_t mmap_sizes[] = {
    32 * 1024, 64 * 1024, 128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024, // 32 KiB to 1Mib
    2 * 1024 * 1024, 4 * 1024 * 1024, 8 * 1024 * 1024, 16 * 1024 * 1024, 32 * 1024 * 1024, // 2Mib to 32MiB
    64 * 1024 * 1024, 128 * 1024 * 1024, 256 * 1024 * 1024, 512 * 1024 * 1024, 1024 * 1024 * 1024, // 64Mib to 1GiB
    2 * 1024 * 1024 * 1024,
};*/

__thread bool is_in_malloc = false;


static bool is_bigger_than_page_size(size_t size){
    return size >= (size_t)getpagesize();
}


static void* my_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off){
    long ret = syscall(SYS_mmap, (unsigned long)addr, (unsigned long)len, (unsigned long)prot, (unsigned long)flags, (unsigned long)fildes, (unsigned long)off);
    if (ret < 0){
        safe_log("error in mmap\n");
        return MAP_FAILED;
    }
    return (void*)ret;
}

static void my_unmmap(void *addr, size_t len){
    long ret = syscall(SYS_munmap, (unsigned long)addr, (unsigned long)len);
    if (ret < 0){
        safe_log("error in unmmap\n");
    }
}

struct MmapZone* get_mem_mmap(size_t size){
    safe_log("NEW MMAP\n");
    void* mmap_ptr = my_mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmap_ptr == MAP_FAILED){
        return NULL;
    }
    return (struct MmapZone*)mmap_ptr;
}

// pointer to new memory
static void* get_more_memory(size_t bytes_to_add){
    // mmap ?
    // sbrk

    /*void* current_heap = sbrk(0);

    if (sbrk(bytes_to_add) == (void*)-1){
        return NULL;
    }

    return current_heap;*/

    // TODO : when bigger than page, just mmap

    if (is_bigger_than_page_size(bytes_to_add)){
        return get_mem_mmap(bytes_to_add); // TODO : align to page size ?
    }

    if (current_mmap_zone && current_mmap_offset + bytes_to_add <= current_mmap_zone->size){
        void* ptr = (char*)GET_PTR_FROM_MMAP_ZONE(current_mmap_zone) + current_mmap_offset;
        current_mmap_offset += bytes_to_add;
        return ptr;
    } 

    struct MmapZone* mmap_zone = mmap_zone_start;
    while (mmap_zone && mmap_zone->next){
        mmap_zone = mmap_zone->next;
    }

    
    
    
    struct MmapZone* new_mmap_zone = get_mem_mmap(current_mmap_size);
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

    current_mmap_offset = bytes_to_add;
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


static size_t align_allocation(size_t size){
    size_t rem = size % ALLOC_ALIGNEMENT;
    if (rem == 0){
        return size;
    }
    return size + (ALLOC_ALIGNEMENT - rem);

    // could do that instead
    // return (size + ALLOC_ALIGNEMENT - 1) & ~(ALLOC_ALIGNEMENT - 1);   
}

static struct Chunk* find_free_block(size_t size, struct Chunk** last_chunk){
    //dump_meta();
    *last_chunk = NULL;
    struct Chunk* current_chunk = start_chunk;
    while (current_chunk){
        if (current_chunk->is_free && is_bigger_than_page_size(size)){
            // remove from chunk lists big mmap allocations (pages) that were freed
            if (*last_chunk){
                (*last_chunk)->next = current_chunk->next;
            } else {
                start_chunk = current_chunk->next;
            }
        }
        if (current_chunk->is_free && current_chunk->size >= size){
            break;
        }

        *last_chunk = current_chunk;
        current_chunk = current_chunk->next;
    }

    return current_chunk;
}

static struct Chunk* new_chunk(size_t size){
    void* new_mem = get_more_memory(CHUNK_SIZE + size);
    if (!new_mem){
        return NULL;
    }
    struct Chunk* chunk = (struct Chunk*)new_mem;
    chunk->size = size;
    chunk->next = NULL;
    chunk->is_free = false;
    chunk->prev = NULL;
    return chunk;
}

void* custom_malloc(size_t size){
    if (is_in_malloc){
        return get_mem_mmap(size);
    }
    if (/*is_in_malloc ||*/ size == 0){
        return NULL;
    }
    //is_in_malloc = true;
    size = align_allocation(size);
    struct Chunk* chunk = NULL;
    if (start_chunk == NULL){
        chunk = new_chunk(size);
        if (!chunk){
            is_in_malloc = false;
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
                is_in_malloc = false;
                return NULL;
            }
            last_chunk->next = chunk;
            chunk->prev = last_chunk;
        } else {
            current_chunk->is_free = false;
            chunk = current_chunk;
        }
    }

    is_in_malloc = false;

    return GET_PTR_FROM_CHUNK(chunk);
}

// TODO : real realloc
void* custom_realloc(void* ptr, size_t new_size){
    void* new_ptr = custom_malloc(new_size);
    if (!new_ptr){
        return NULL;
    }
    if (ptr){
        struct Chunk* old_alloc_chunk = GET_CHUNK_FROM_PTR(ptr);
        size_t copy_size = min(old_alloc_chunk->size, new_size); // for the case where new_size < old_alloc_chunk->size
        memcpy(new_ptr, ptr, copy_size);
        custom_free(ptr);
    }
    return new_ptr;
}

static void merge_block(struct Chunk* chunk){
    // TODO : add previous check for merging ? or entire list iteration ?
    //dump_meta();
    while (chunk->next && chunk->next->is_free && chunk->next == (struct Chunk*)((char*)GET_PTR_FROM_CHUNK(chunk) + chunk->size)){
        safe_log("MERGE BLOCK\n");
        chunk->size += CHUNK_SIZE + chunk->next->size;
        chunk->next = chunk->next->next;
        if (chunk->next){
            chunk->next->prev = chunk;
        }
    }

    while (chunk->prev && chunk->prev->is_free && chunk->prev == (struct Chunk*)GET_CHUNK_FROM_PTR(((char*)chunk) - chunk->prev->size)){
        chunk->prev->size += CHUNK_SIZE + chunk->size;
        chunk->prev->next = chunk->next;
        chunk = chunk->prev;
        if (chunk->next){
            chunk->next->prev = chunk->prev;
        }
    }
}

static void free_mmap(struct Chunk* chunk){
    if (chunk->prev){
        chunk->prev->next = chunk->next;
    }
    if (chunk->next){
        chunk->next->prev = chunk->prev;
    }
    my_unmmap(chunk, chunk->size + CHUNK_SIZE);
}

void custom_free(void* ptr){
    if (!ptr){
        return;
    }
    struct Chunk* chunk = GET_CHUNK_FROM_PTR(ptr);
    if (is_bigger_than_page_size(CHUNK_SIZE + chunk->size)){
        free_mmap(chunk);
        return;
    }
    chunk->is_free = true;
    merge_block(chunk);
}

void* custom_calloc(size_t nelem, size_t size){
    if (nelem == 0 || size == 0){
        return NULL;
    }
    if (nelem > __INT64_MAX__/size){
        return NULL;
    }

    size_t total_size = size * nelem; // TODO : overflow checks ?
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

// TODO : implement posix_memalign (need to just malloc with a little more adjustement and then add to the pointer to be aligned : https://github.com/kraj/musl/blob/ff441c9ddfefbb94e5881ddd5112b24a944dc36c/src/malloc/mallocng/aligned_alloc.c)

#endif