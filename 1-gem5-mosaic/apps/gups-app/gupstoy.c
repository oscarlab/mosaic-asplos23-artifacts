#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <sys/mman.h>

#include <string.h>
#include <numa.h>
#include <time.h>
#include <pthread.h>

#ifdef _OPENMP
#include <omp.h>
#endif


extern FILE *opt_file_out;

/*
 * ============================================================================
 * HPCC RandomAccess
 * ============================================================================
 */


#ifdef _OPENMP
#define NUPDATE (1UL << 29)
#else
#define NUPDATE (1UL << 29)
#endif

///< parameters for ther andom table
#define POLY 0x0000000000000007UL
#define PERIOD 1317624576693539401L


static uint64_t
HPCC_starts(int64_t n)
{
    int i, j;
    uint64_t m2[64];
    uint64_t temp, ran;

    while (n < 0) n += PERIOD;
    while (n > PERIOD) n -= PERIOD;
    if (n == 0) return 0x1;

    temp = 0x1;
    for (i=0; i<64; i++) {
        m2[i] = temp;
        temp = (temp << 1) ^ ((int64_t) temp < 0 ? POLY : 0);
        temp = (temp << 1) ^ ((int64_t) temp < 0 ? POLY : 0);
    }

    for (i=62; i>=0; i--)
        if ((n >> i) & 1)
            break;

    ran = 0x2;
    while (i > 0) {
    temp = 0;
    for (j=0; j<64; j++)
        if ((ran >> j) & 1)
            temp ^= m2[j];
        ran = temp;
        i -= 1;
        if ((n >> i) & 1)
            ran = (ran << 1) ^ ((int64_t) ran < 0 ? POLY : 0);
    }

    return ran;
}

///< the name of the shared memory file created
#define CONFIG_SHM_FILE_NAME "/tmp/alloctest-bench"


int real_main(int argc, char *argv[]);
int real_main(int argc, char *argv[])
{
    size_t mem = (1UL << 30);

    fprintf(opt_file_out, "<gups tablesize=\"%zu\"></gups>\n", mem);

    struct timespec time1, time2;

    #define MAP_FLAGS  (MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED)
    #define MAP_SIZE (sizeof(STREAM_TYPE) * (STREAM_ARRAY_SIZE+OFFSET))
    //
    uint64_t *Table = malloc(mem);  
    if (Table == NULL) {
        printf("MAP FAILED for Table %p expected %p\n", Table);
        return -1;  
    }

    //uint64_t *Table = malloc(mem);
    if (!Table) {
        fprintf(opt_file_out, "ERROR: Could not allocate table!\n");
        return -1;
    }

    size_t TableSize = mem / sizeof(uint64_t);

    fprintf(opt_file_out, "<gups table=\"%p\" tablesize=\"%zu\"></gups>\n", Table, TableSize);


    /* Initialize main table */
    #ifdef _OPENMP
    #pragma omp parallel for
    #endif
    for (size_t i=0; i<TableSize; i++) {
        Table[i] = i;
    }

    FILE *fd2 = fopen(CONFIG_SHM_FILE_NAME ".ready", "w");

    if (fd2 == NULL) {
        fprintf (stderr, "ERROR: could not create the shared memory file descriptor\n");
        exit(-1);
    }

    usleep(250);


    #ifdef _OPENMP
    #pragma omp parallel
    {
    #endif
        /* Current random numbers */
        uint64_t *ran = calloc(128, sizeof(uint64_t));
        printf("====  %p\n", ran);
        for (size_t j=0; j<128; j++) {
            ran[j] = HPCC_starts ((NUPDATE/128) * j);
        }

        printf("====\n");

        #ifdef _OPENMP
        size_t num_threads = omp_get_num_threads();
        #pragma omp for
        for (size_t i=0; i<(NUPDATE/128) * num_threads; i++) {
        #else
        for (size_t i=0; i<NUPDATE/128; i++) {
        #endif
            /* #pragma ivdep */
            for (size_t j=0; j<128; j++) {
                ran[j] = (ran[j] << 1) ^ ((int64_t) ran[j] < 0 ? POLY : 0);
                size_t elm = ran[j] % TableSize;
                Table[elm] ^= ran[j];
                Table[TableSize - elm] ^= ran[j];
            }
        }

        free(ran);
    #ifdef _OPENMP
    } // opened in prepare_parallel_for
    #endif

//  Don't free the table in the end, so we can check the page table
//    free(Table);

    return 0;
}
