#include <stddef.h>
#include <stdbool.h>

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

