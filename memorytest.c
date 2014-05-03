#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

#ifdef HAVE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#include "zmalloc.h"
#include "array.h"

static size_t limit = 100*1024*1024;
static int thread_count = 10;
static int jemalloc_limit;
static size_t (*memory_used_fn) (void);

static void usage(char **argv)
{
	printf("%s\n\n", argv[0]);
	printf("\t-l <bytes> (e.g. 10, 10K, 10M, 10G)\n");
	printf("\t-t <threads>\n");
#ifdef HAVE_JEMALLOC
	printf("\t-j limit memory by jemalloc stats\n");
#endif
	printf("\n");
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

#ifdef HAVE_JEMALLOC
	while ((ch = getopt(argc, argv, "l:t:j")) != -1) {
#else
	while ((ch = getopt(argc, argv, "l:t:")) != -1) {
#endif
		switch (ch) {
			case 'l':
				limit = mem_human_to_cpu(optarg); break;
			case 't':
				thread_count = strtol(optarg, 0, 10); break;
#ifdef HAVE_JEMALLOC
			case 'j':
				jemalloc_limit = 1; break;
#endif
			default:
				usage(argv);
				exit(0);
		}
	}
}

#ifdef HAVE_JEMALLOC
static size_t jemalloc_size_allocated()
{
	uint64_t epoch = 1;
	size_t allocated;
	size_t len;

	len = sizeof(epoch);
	if (-1 == mallctl("epoch", 0, 0, &epoch, len)) {
		fprintf(stderr, "mallctl() error\n");
		exit(1);
	}

	len = sizeof(allocated);
	if (-1 == mallctl("stats.allocated", &allocated, &len, NULL, 0)) {
		fprintf(stderr, "mallctl() error\n");
		exit(1);
	}

	return allocated;
}
#endif

static void *thread_worker(void *ctx)
{
	struct array_s allocations;

	if (0 == array_init(&allocations, sizeof(void *), 0)) {
		fprintf(stderr, "no mem\n");
		exit(1);
	}

	struct random_data rnd;
	memset(&rnd, 0, sizeof(rnd));
	char rnd_state[8];
	initstate_r(1, rnd_state, 8, &rnd);

	for (;;) {
		int32_t r;
		random_r(&rnd, &r);
		long alloc_size = 10 + r % 1024*20;

		while (memory_used_fn() + alloc_size >= limit && allocations.used) {
			void **allocation_p = array_item_last(&allocations);
			if (!allocation_p) {
				break;
			}

			allocations.used--;
			zfree(*allocation_p);
		}

		void *buf = zmalloc(alloc_size);
		if (!buf) {
			fprintf(stderr, "no mem\n");
			exit(1);
		}

		memset(buf, 0x01, sizeof(alloc_size));

		*(void **)array_push(&allocations) = buf;

	}
}

int main(int argc, char **argv)
{
	parse_cmd(argc, argv);

#ifdef HAVE_JEMALLOC
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

