#include "config.h"
#include <sys/types.h>

void* custom_malloc(size_t size);
void custom_free(void* ptr);
void* custom_realloc(void* ptr, size_t new_size);

#if LOGGING
void dump_meta();
#endif