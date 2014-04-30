#ifndef ARRAY_H
#define ARRAY_H

#include <stdlib.h>

#include "zmalloc.h"

struct array_s {
	void *data;
	size_t sz;
	unsigned long used;
	unsigned long allocated;
};

static inline struct array_s *array_init(struct array_s *a, size_t sz, unsigned long initial_num)
{
	void *allocated = 0;

	if (!a) {
		a = zmalloc(sizeof(struct array_s));

		if (!a) {
			return 0;
		}

		allocated = a;
	}

	a->sz = sz;

	if (initial_num) {
		a->data = zcalloc(sz*initial_num);

		if (!a->data) {
			zfree(allocated);
			return 0;
		}
	}
	else {
		a->data = 0;
	}

	a->allocated = initial_num;
	a->used = 0;

	return a;
}

static inline void *array_item(struct array_s *a, unsigned long n)
{
	void *ret;

	ret = (char *) a->data + a->sz * n;

	return ret;
}

static inline void *array_item_last(struct array_s *a)
{
	return array_item(a, a->used - 1);
}

#define array_v(a, type) ((type *) (a)->data)
#define array_mem_used(a) ( (a)->sz * (a)->allocated )

static inline void *array_push(struct array_s *a)
{
	void *ret;

	if (a->used == a->allocated) {
		unsigned long new_allocated = a->allocated ? a->allocated * 2 : 16;
		void *new_ptr = zrealloc(a->data, a->sz * new_allocated);

		if (!new_ptr) {
			return 0;
		}

		a->data = new_ptr;
		a->allocated = new_allocated;
	}

	ret = array_item(a, a->used);

	++a->used;

	return ret;
}

static inline void array_free(struct array_s *a)
{
	zfree(a->data);
	a->data = 0;
	a->sz = 0;
	a->used = a->allocated = 0;
}

#endif
