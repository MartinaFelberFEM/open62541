

#include <stdio.h>

#include <assert.h>

#include "bufmalloc.h"

// int main(void) {

//     char *pNormalAlloc = (char*) UA_malloc(10 * sizeof(char));
//     assert(pNormalAlloc != NULL);

//     useMembufAlloc();

//     char *pMemBufAlloc = (char*) UA_malloc(10 * sizeof(char));
//     assert(pMemBufAlloc != NULL);
//     UA_free(pMemBufAlloc);
//     pMemBufAlloc = NULL;

//     useNormalAlloc();

//     UA_free(pNormalAlloc);
//     pNormalAlloc = NULL;


//     return 0;
// }

int main(void) {

    #ifdef UA_ENABLE_MALLOC_SINGLETON
        printf("Malloc singleton should be defined\n");
    #endif

    // UA_mallocSingleTon points to <__GI___libc_malloc>

    useMembufAlloc();

    char *pMemBufAlloc = (char*) UA_malloc(10 * sizeof(char));
    assert(pMemBufAlloc != NULL);
    UA_free(pMemBufAlloc);
    pMemBufAlloc = NULL;

    useNormalAlloc();

    char *pNormalAlloc = (char*) UA_malloc(10 * sizeof(char));
    assert(pNormalAlloc != NULL);

    UA_free(pNormalAlloc);
    pNormalAlloc = NULL;


    return 0;
}