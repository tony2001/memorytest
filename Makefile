all: jemalloc

libc: memorytest.c zmalloc.c config.h
	gcc --std=gnu99 -g3 -O3 -o memorytest memorytest.c zmalloc.c -pthread

jemalloc: memorytest.c zmalloc.c config.h
	gcc -DHAVE_JEMALLOC --std=gnu99 -g3 -O3 -o memorytest memorytest.c zmalloc.c -pthread -ljemalloc

tcmalloc: memorytest.c zmalloc.c config.h
	gcc -DHAVE_TCMALLOC --std=gnu99 -g3 -O3 -o memorytest memorytest.c zmalloc.c -pthread -ltcmalloc

clean:
	rm -f memorytest
