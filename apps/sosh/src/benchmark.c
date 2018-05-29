/*
 * Copyright 2016, Data61 & CSIRO
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 */
#include <autoconf.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <utils/util.h>

#include <sos.h>

/* number of times to run the benchmark before recording results
 * this primes the caches etc so we don't use cold cache results */
#define WARMUPS     1

/* number of times to run the benchmarks and collect data -
 * for testing purposes you can set this to 1, but for reported
 * results it should be 10 */
#define ITERATIONS 10

/* total number of iterations for the benchmark loop */
#define N_RESULTS (WARMUPS + ITERATIONS)

/* size constants */
#define KB 1024
#define MB (KB*KB)

/* buffer size exponents - where buffer size is 2^{CONSTANT} */
#define MIN_BUF_SIZE 9u /* 2^9 = 512b */
#define MAX_BUF_SIZE 18u /* 2^18 = 512K */

/* total file size to benchmark for each iteration of the benchmark.
 * You can make this smaller for testing, but performance graphs need
 * to be for 4 MB */
#define TOTAL_FILE_SIZE (4 * MB)

_Static_assert (BIT(MAX_BUF_SIZE) <= TOTAL_FILE_SIZE, "total file size must be a multiple of MAX_BUF_SIZE");
_Static_assert (MIN_BUF_SIZE > 0, "min buf size bigger than 0");
_Static_assert(MAX_BUF_SIZE >= MIN_BUF_SIZE, "min buf size smaller than or eq to max buf size");

/* name of the benchmark file to write/read to */
#define BENCHMARK_FILE "benchmark.dat"
/* name of file to write results to */
#define BENCHMARK_RESULTS_FILE "results.tsv"

/* cycle counter constants */
#define CCNT_64     BIT(3u)
#define CCNT_RESET  BIT(2u)
#define CCNT_ENABLE BIT(0u)
#define PMCNTENSET BIT(31u)

/* max size for output lines in results.tsv */
#define LINE_SIZE 200

#define READ_CCNT(var) do { \
    asm volatile("mrc p15, 0, %0, c9, c13, 0\n" \
        : "=r"(var) \
    ); \
} while(0)

/* amount of loops to do for each benchmark */
#define LOOPS (TOTAL_FILE_SIZE/BIT(MAX_BUF_SIZE))

static char buf[TOTAL_FILE_SIZE];

typedef int (*benchmark_fn_t)(int fd, char *buf, size_t nbyte);

static inline uint32_t read_pmcr(void)
{
    uint32_t pmcr;
    /* read the pmcr */
    asm volatile (
        "mrc p15, 0, %0, c9, c12, 0\n"
        : "=r" (pmcr)
    );
    return pmcr;
}

static void init_ccnt(void *arg) {

    uint32_t pmcr = read_pmcr();
    asm volatile (
        // allow ccnt to be read from user level
        "mcr p15, 0,  %0, c9, c14, 0\n"
        //turn off overflow interrupts
        "mcr p15, 0, %1, c9, c14, 2\n"
        // set the cycle count to tick every 64 bits so it doesn't overflow
        // also enable it and reset to 0
        "mcr p15, 0, %2, c9, c12, 0\n"
        //enable the cycle counter
        "mcr p15, 0, %3, c9, c12, 1\n"
        : /* no outputs */
     : "r" (1),
       "r" (-1),
       "r" (pmcr | CCNT_RESET | CCNT_ENABLE | CCNT_64),
       "r" (PMCNTENSET)
     : /* no clobber */
    );
}

static void reset_ccnt(uint32_t pmcr) {
    asm volatile (
        "mcr p15, 0, %0, c9, c12, 0\n"
        : /* no outputs */
        : "r" (pmcr | CCNT_RESET)
        : /* no clobber */
    );
}

void sos_fprintf(int fd, const char *format, ...)
{
    /* go via a string as we have an fd not a FILE*
     * and we don't want muslc to get in the way of debugging */
    va_list args;
    va_start(args, format);
    char buf[LINE_SIZE];
    vsnprintf(buf, LINE_SIZE, format, args);
    va_end(args);
    sos_sys_write(fd, buf, strnlen(buf, LINE_SIZE));
}

static int open_helper(char *name, fmode_t mode)
{
    int fd = sos_sys_open(name, mode);
    if (fd == -1) {
        printf("Failed to open file %s\n", name);
    }
    return fd;
}

static int run_benchmark(char *name, benchmark_fn_t fn, uint32_t overhead,
                  int results_fd, int debug_mode)
{
    uint32_t results[N_RESULTS];
    uint32_t pmcr = read_pmcr();

    /* open the file */
    int fd = open_helper(BENCHMARK_FILE, O_RDWR);
    if (fd == -1) {
        return -1;
    }

    /* iterate through buffer sizes, going up by 3 powers of 2 each time,
     * for the provided constants, this will iterate through
     * 512b, 4k, 32k, 256k */
    for (size_t sz = BIT(MIN_BUF_SIZE); sz <= BIT(MAX_BUF_SIZE); sz = sz << 3u) {
        uint32_t start, end;

        for (int i = 0; i < N_RESULTS; i++) {
            reset_ccnt(pmcr);
            READ_CCNT(start);
            /* for each buf size, send the file over the network */
            for (int j = 0; j < LOOPS; j++) {
                if (debug_mode) {
                    if (fn == (benchmark_fn_t) sos_sys_write) {
                        /* if we're writing, write a magic val to the buffer */
                        assert(CLZ(sz) < sz);
                        buf[j * sz + CLZ(sz)] = (char) CLZ(sz);
                    }
                }

                assert(j * sz < sz * LOOPS);
                int res = fn(fd, &buf[j * sz], sz);

                if(debug_mode){
                    /* check we read/wrote the amount we asked to */
                    if (res != sz) {
                        printf("benchmark_fn did not %s full buf size, only %u/%u\n",
                                name, res, sz);
                        sos_sys_close(fd);
                        return -1;
                    }
                    /* if we're reading, check the magic val is correct */
                    if (fn == sos_sys_read) {
                        assert(buf[j * sz + CLZ(sz)] == (char) CLZ(sz));
                    }
                }
            }

            READ_CCNT(end);
            results[i] = end - start;
        }

        /* output to results file, calculate results offline */
        sos_fprintf(results_fd, "{\"name\": \"%s\",", name);
        sos_fprintf(results_fd, "\"buf_size\": %u,", sz);
        sos_fprintf(results_fd, "\"file_size\": %u,", LOOPS * sz);
        sos_fprintf(results_fd, "\"samples\": [");

        uint64_t check_sum = 0;
        for (int i = WARMUPS; i < N_RESULTS; i++) {
            /* remove overhead */
            uint32_t result = results[i] - overhead;
            sos_fprintf(results_fd, "%u", result);
            if (i < N_RESULTS - 1) {
                sos_fprintf(results_fd, ",");
            } else {
                sos_fprintf(results_fd, "]");
            }
            /* store a checksum, so we can validate the results */
            check_sum += result;
        }
        sos_fprintf(results_fd, ",\"check_sum\":%llu}\n", check_sum);
        /* skip ',' for the last value */
        if (sz != BIT(MAX_BUF_SIZE)) {
            sos_fprintf(results_fd, ",");
        }

    }

    sos_sys_close(fd);
    return 0;
}

static uint32_t find_overhead(void)
{
    /* measure overhead of reading the cycle counter */
    uint32_t results[N_RESULTS] = {0};
    uint32_t start, end;
    for (int i = 0; i < N_RESULTS; i++) {
        READ_CCNT(start);
        READ_CCNT(end);
        results[i] = end - start;
    }

    /* find lowest value (excluding WARMUPS) */
    uint32_t overhead = UINT32_MAX;
    for (int i = WARMUPS; i < N_RESULTS; i++) {
        overhead = MIN(results[i], overhead);
    }
    return overhead;
}

int sos_benchmark(int debug_mode)
{
    /* allow the cycle counter to be read from user level */
#ifndef CONFIG_DANGEROUS_CODE_INJECTION
    /* if this fails, you need to make menuconfig and select
     * 'Build Options' -> 'Build kernel with support for executing arbitrary code in protected mode'
     * This allows us to configurethe cycle counter to be read from user level */
#error "Cannot read cycle count without DANGEROUS_CODE_INJECTION enabled!"
#endif
    seL4_DebugRun(init_ccnt, NULL);

    /* find overhead of measuring cycle counter */
    uint32_t overhead = find_overhead();


    /* create benchmark results file */
    int results_fd = open_helper(BENCHMARK_RESULTS_FILE, O_WRONLY);
    if (results_fd == -1) {
        return -1;
    }

    sos_fprintf(results_fd, "[");
     /* benchmark write */
    int res = run_benchmark("sos_sys_write", (benchmark_fn_t) sos_sys_write,
                        overhead, results_fd, debug_mode);

    if (res == -1) {
        sos_sys_close(results_fd);
        return -1;
    }
    sos_fprintf(results_fd, ",");

    /* benchmark read */
    res = run_benchmark("sos_sys_read", sos_sys_read, overhead, results_fd,
                        debug_mode);
    sos_fprintf(results_fd, "]");
    sos_sys_close(results_fd);
    return res;
}
