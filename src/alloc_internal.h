#include "config.h"
#include <stddef.h>
#include <stdbool.h>

struct Chunk {
    struct Chunk* prev;
    size_t size;
    struct Chunk* next;
    bool is_free;
};

// align CHUNK_SIZE to ALLOC_ALIGNEMENT to preserve alignement of the allocation (for ex if ALLOC_ALIGNEMENT = 16, and the ptr p is 16 bytes aligned, but sizeof(struct Chunk) = 24 bytes, p will only be 8 bytes aligned !!)
#define CHUNK_SIZE ((sizeof(struct Chunk) + ALLOC_ALIGNEMENT - 1) & ~(ALLOC_ALIGNEMENT - 1))

#define GET_PTR_FROM_CHUNK(chunk_ptr) (void*)(((unsigned char*)chunk_ptr)+CHUNK_SIZE)  

#define GET_CHUNK_FROM_PTR(ptr) ((struct Chunk*)(((unsigned char*)ptr)-CHUNK_SIZE))

// TODO : do I really need to store the whole linked list or just the last (need to unmap it ?)
struct MmapZone {
    size_t size;
    struct MmapZone* next;
};

#define GET_PTR_FROM_MMAP_ZONE(mmap_ptr) (void*)(((unsigned char*)mmap_ptr)+sizeof(struct MmapZone))  
#define GET_MMAP_ZONE_FROM_PTR(ptr) (struct MmapZone*)(((unsigned char*)ptr)-sizeof(struct MmapZone))

