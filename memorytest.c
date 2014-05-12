#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

#ifdef USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#include "zmalloc.h"
#include <Judy.h>

static size_t limit = 100*1024*1024;
static int thread_count = 10;
static int jemalloc_limit;
static size_t (*memory_used_fn) (void);

static void usage(char **argv)
{
	printf("%s\n\n", argv[0]);
	printf("\t-l <bytes> (e.g. 10, 10K, 10M, 10G)\n");
	printf("\t-t <threads>\n");
#ifdef USE_JEMALLOC
	printf("\t-j limit memory by jemalloc stats\n");
#endif
	printf("\n");
}

void *malloc(size_t size)
{
	return zmalloc(size);
}


void *realloc(void *ptr, size_t size)
{
	return zrealloc(ptr, size);
}


void *calloc(size_t nmemb, size_t size)
{
	return zcalloc(nmemb, size);
}


void free(void *ptr)
{
	zfree(ptr);
}


static size_t mem_human_to_cpu(char *s)
{
	char *end;
	size_t limit = strtol(s, &end, 10);

	switch (*end) {
		case 'B':
		case '\0':
			break;
		case 'K':
			limit *= 1024; break;
		case 'M':
			limit *= 1024 * 1024; break;
		case 'G':
			limit *= 1024 * 1024 * 1024; break;
		default:
			goto wrong; break;

	}

	if (limit <= 0) {
wrong:
		fprintf(stderr, "wrong limit\n");
		exit(0);
	}

	return limit;
}

static void mem_cpu_to_human(size_t mem, char *buf, size_t buf_size)
{
	if (mem < 1024) {
		snprintf(buf, buf_size, "%ldB", mem);
	} else if (mem < 1024*1024) {
		snprintf(buf, buf_size, "%.2fKiB", (float)mem/1024.0);
	} else if (mem < 1024*1024*1024) {
		snprintf(buf, buf_size, "%.2fMiB", (float)mem/1024.0/1024.0);
	} else {
		snprintf(buf, buf_size, "%.2fGiB", (float)mem/1024.0/1024.0/1024.0);
	}
}

static void parse_cmd(int argc, char **argv)
{
	int ch;

#ifdef USE_JEMALLOC
	while ((ch = getopt(argc, argv, "l:t:j")) != -1) {
#else
	while ((ch = getopt(argc, argv, "l:t:")) != -1) {
#endif
		switch (ch) {
			case 'l':
				limit = mem_human_to_cpu(optarg); break;
			case 't':
				thread_count = strtol(optarg, 0, 10); break;
#ifdef USE_JEMALLOC
			case 'j':
				jemalloc_limit = 1; break;
#endif
			default:
				usage(argv);
				exit(0);
		}
	}
}

#ifdef USE_JEMALLOC
static size_t jemalloc_size_allocated()
{
	uint64_t epoch = 1;
	size_t allocated;
	size_t len;

	len = sizeof(epoch);
	if (-1 == je_mallctl("epoch", 0, 0, &epoch, len)) {
		fprintf(stderr, "mallctl() error\n");
		exit(1);
	}

	len = sizeof(allocated);
	if (-1 == je_mallctl("stats.allocated", &allocated, &len, NULL, 0)) {
		fprintf(stderr, "mallctl() error\n");
		exit(1);
	}

	return allocated;
}
#endif

#define RAND_RANGE(__n, __min, __max, __tmax) \
	(__n) = (__min) + (long) ((double) ( (double) (__max) - (__min) + 1.0) * ((__n) / ((__tmax) + 1.0)))

#define MAX_RAND_NUM 2000

static void *thread_worker(void *ctx)
{
	Pvoid_t judy_l = NULL;
	struct random_data rnd;
	char rnd_state[8];

	memset(&rnd, 0, sizeof(rnd));
	initstate_r(1, rnd_state, 8, &rnd);

	for (;;) {
		int r;
		Pvoid_t judy_1;
		PPvoid_t ppvalue;
		long i, j, first = 0, array_index, arrays_n, entries_n, entry_value;
		long judy_l_count = JudyLCount(judy_l, 0, -1, NULL);

		while (memory_used_fn() >= limit && (ppvalue = JudyLFirst(judy_l, &first, NULL)) != NULL) {
			judy_1 = *ppvalue;
			Judy1FreeArray(&judy_1, NULL);
			JudyLDel(&judy_l, first, NULL);
		}

		/* random number of arrays */
		random_r(&rnd, &r);
		arrays_n = (long) r;
		RAND_RANGE(arrays_n, 0, MAX_RAND_NUM, 2147483647);

		for (j = 0; j < arrays_n; j++) {
			/* random number of entries */
			random_r(&rnd, &r);
			entries_n = (long) r;
			RAND_RANGE(entries_n, 0, MAX_RAND_NUM, 2147483647);

			judy_1 = NULL;
			for (i = 0; i < entries_n; i++) {
				/* random values in Judy1 array */
				random_r(&rnd, &r);
				entry_value = (long) r;

				Judy1Set(&judy_1, entry_value, NULL);
			}

			for (;;) {
				random_r(&rnd, &r);
				array_index = (long) r;

				ppvalue = JudyLIns(&judy_l, array_index, NULL);
				if (!ppvalue) {
					exit(1);
				}

				if (!*ppvalue) {
					break;
				}
			}

			*ppvalue = judy_1;
		}
	}
}

int main(int argc, char **argv)
{
	parse_cmd(argc, argv);

#ifdef USE_JEMALLOC
	if (jemalloc_limit) {
		memory_used_fn = jemalloc_size_allocated;
	}
#endif

	if (!memory_used_fn) {
		memory_used_fn = zmalloc_used_memory;
	}

	zmalloc_enable_thread_safeness();

	pthread_t threads[thread_count];

	for (int i = 0; i < thread_count; i++) {
		if (0 != pthread_create(&threads[i], 0, thread_worker, 0)) {
			fprintf(stderr, "error while creating thread\n");
			return -1;
		}
	}

	char size_human_buf[1024];
	char rss_human_buf[1024];

	for (;;) {
		sleep(1);
		size_t rss = zmalloc_get_rss();
		size_t used = memory_used_fn();
		mem_cpu_to_human(used, size_human_buf, 1024);
		mem_cpu_to_human(rss, rss_human_buf, 1024);
		printf("memory_used %s, rss %s, fragmentation ratio %f\n", size_human_buf, rss_human_buf, zmalloc_get_fragmentation_ratio(rss));
	}

	return 0;
}

