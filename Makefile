all: memorytest

memorytest: memorytest.c zmalloc.c
	gcc --std=gnu99 -g3 -O3 -o memorytest memorytest.c zmalloc.c -pthread -ljemalloc

clean:
	rm -f memorytest
