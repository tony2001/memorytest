#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "zmalloc.h"
#include "array.h"

static size_t limit = 100*1024*1024;
static int thread_count = 10;

static void usage(char **argv)
{
	printf("%s\n\n", argv[0]);
	printf("\t-l <bytes> (e.g. 10, 10K, 10M, 10G)\n");
	printf("\t-t <threads>\n");
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
			limit *= limit * 1024; break;
		case 'M':
			limit *= limit * 1024 * 1024; break;
		case 'G':
			limit *= limit * 1024 * 1024 * 1024; break;
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

	while ((ch = getopt(argc, argv, "l:t:")) != -1) {
		switch (ch) {
			case 'l':
				limit = mem_human_to_cpu(optarg); break;
			case 't':
				thread_count = strtol(optarg, 0, 10); break;
			default:
				usage(argv);
				exit(0);
		}
	}
}

static void *thread_worker(void *ctx)
{
	struct array_s allocations;

	if (0 == array_init(&allocations, sizeof(void *), 0)) {
		fprintf(stderr, "no mem\n");
		exit(1);
	}

	for (;;) {
		long alloc_size = 10 + random() % 1024*20;

		while (zmalloc_used_memory() + alloc_size >= limit && allocations.used) {
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
		size_t used = zmalloc_used_memory();
		mem_cpu_to_human(used, size_human_buf, 1024);
		mem_cpu_to_human(rss, rss_human_buf, 1024);
		printf("memory_used %s, rss %s, fragmentation ratio %f\n", size_human_buf, rss_human_buf, zmalloc_get_fragmentation_ratio(rss));
	}

	return 0;
}

