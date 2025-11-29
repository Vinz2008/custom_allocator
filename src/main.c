#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "alloc.h"

struct test_struct {
    int a, b;
    double d1, d2;
    void* p;
};

int main(){
    struct test_struct* ts = custom_malloc(sizeof(struct test_struct));
    printf("ptr allocated : %p\n", ts);
    assert(ts != NULL);
    ts->a = 2;
    ts->p = (void*)0xBEEF;
    printf("%d\n", ts->a);
    printf("%p\n", ts->p);
    custom_free(ts);

    for (int i = 0; i < 100; i++){
        //dump_meta();
        void* p = custom_malloc(sizeof(char)*10);
        printf("ptr allocated in loop nb %d : %p\n", i, p);
    }

    char* p_realloc = custom_malloc(sizeof(char)*10);
    p_realloc[8] = 'a';
    p_realloc = custom_realloc(p_realloc, 20);
    printf("p_realloc[8] : %c\n", p_realloc[8]);

    printf("page size : %d\n", getpagesize());
    unsigned char* a = custom_malloc(4096);
    a[4096-1] = 18;
    //dump_meta();
    custom_free(a);
    return 0;
}
