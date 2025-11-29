#include "config.h"
#include "alloc_internal.h"
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#if LOGGING

#include <fcntl.h>

extern struct MmapZone* mmap_zone_start;
extern struct Chunk* start_chunk;

// to prevent malloc
static size_t my_strlen(const char* s){
    size_t i = 0;
    while (*s++) i++;
    return i;
}

static ssize_t write_wrapper(int fd, const char* buf){
    return write(fd, buf, my_strlen(buf));
}


void safe_log(const char* message){
    write_wrapper(2, message);
}



static const char* ptr_to_string(const void* p){
    static char buf_ptr[50] = {0};
    memset(buf_ptr, 0, sizeof(buf_ptr));
    
    uintptr_t ptr = (uintptr_t)p;
    if (ptr == 0){
        buf_ptr[0] = '0';
        buf_ptr[1] = 'x';
        buf_ptr[2] = '0';
        return buf_ptr;
    } 
        
    char* buf_ptr_temp = buf_ptr + sizeof(buf_ptr) - 1; 
    while (ptr != 0){
        buf_ptr_temp--;
        uint8_t digit = ptr % 16;
        *buf_ptr_temp = (digit >= 10) ? 'A' + digit - 10 : '0' + digit;
        ptr /= 16;
    }
    buf_ptr_temp--;
    *buf_ptr_temp = 'x';
    buf_ptr_temp--;
    *buf_ptr_temp = '0';

    return buf_ptr_temp;
}

void dump_meta(){
    int fd = open("dump_meta.txt", O_CREAT | O_WRONLY, 0644);
    // TODO : add size of mmap zone and chunks
    struct MmapZone* zone = mmap_zone_start;
    while (zone){
        write_wrapper(fd, "MMAP ZONE at "); 
        write_wrapper(fd, ptr_to_string(zone));
        write_wrapper(fd, "\n");
        zone = zone->next;
    }

    struct Chunk* chunk = start_chunk;
    while (chunk){
        write_wrapper(fd, "CHUNK at ");
        write_wrapper(fd, ptr_to_string(chunk));
        write_wrapper(fd, "\n");
        chunk = chunk->next;
    } 
   
    close(fd);
}

#else 
void safe_log(const char* message){
    (void)message;
}
#endif