#include "mem.h"

#include "err.h"

#include <stdlib.h>
 
void* checked_malloc(const size_t nbytes) {
    void* ptr = malloc(nbytes);
    if (ptr == NULL)
        fatal("malloc");
    return ptr;  // don't forget to free!
}

void* checked_realloc(void* ptr, const size_t nbytes) {
    void* new_ptr = realloc(ptr, nbytes);
    if (ptr == NULL)
        fatal("realloc");
    return new_ptr;  // don't forget to free!
}
