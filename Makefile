all: jemalloc

MA_PREFIX ?= /local

MA_CFLAGS = -I$(MA_PREFIX)/include
MA_LDFLAGS = -L$(MA_PREFIX)/lib -L$(MA_PREFIX)/lib64 -Wl,-rpath,$(MA_PREFIX)/lib -Wl,-rpath,$(MA_PREFIX)/lib64

libc: memorytest.c zmalloc.c config.h
	gcc --std=gnu99 -g3 -O0 -o memorytest memorytest.c zmalloc.c -pthread

jemalloc: memorytest.c zmalloc.c config.h
	gcc -DHAVE_JEMALLOC $(MA_CFLAGS) $(MA_LDFLAGS) --std=gnu99 -g3 -O0 -o memorytest memorytest.c zmalloc.c -pthread -ljemalloc

tcmalloc: memorytest.c zmalloc.c config.h
	gcc -DHAVE_TCMALLOC $(MA_CFLAGS) $(MA_LDFLAGS) --std=gnu99 -g3 -O0 -o memorytest memorytest.c zmalloc.c -pthread -ltcmalloc

clean:
	rm -f memorytest
