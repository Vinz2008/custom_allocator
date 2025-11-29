
// align pointers to 128 bits or 16 bytes
// TODO : replace this with 8 : see https://www.man7.org/linux/man-pages/man3/posix_memalign.3.html the malloc part
#define ALLOC_ALIGNEMENT 16

#define REPLACE_LIBC_FUNCTIONS 1

#define LOGGING 0