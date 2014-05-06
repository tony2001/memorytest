all: jemalloc

MA_PREFIX ?= /local
JUDY_PREFIX ?= /local

MA_CFLAGS = -I$(MA_PREFIX)/include
MA_LDFLAGS = -L$(MA_PREFIX)/lib -L$(MA_PREFIX)/lib64 -Wl,-rpath,$(MA_PREFIX)/lib -Wl,-rpath,$(MA_PREFIX)/lib64

JUDY_CFLAGS = -I$(JUDY_PREFIX)/include
JUDY_LDFLAGS = -L$(JUDY_PREFIX)/lib -L$(JUDY_PREFIX)/lib64 -Wl,-rpath,$(JUDY_PREFIX)/lib -Wl,-rpath,$(JUDY_PREFIX)/lib64 -lJudy

FLAGS = $(MA_CFLAGS) $(MA_LDFLAGS) $(JUDY_CFLAGS) $(JUDY_LDFLAGS)

libc: memorytest.c zmalloc.c config.h
	gcc --std=gnu99 -g3 -O0 -o memorytest memorytest.c zmalloc.c -pthread

jemalloc: memorytest.c zmalloc.c config.h
	gcc -DHAVE_JEMALLOC -DUSE_JEMALLOC $(FLAGS) --std=gnu99 -g3 -O0 -o memorytest memorytest.c zmalloc.c -pthread -ljemalloc

tcmalloc: memorytest.c zmalloc.c config.h
	gcc -DHAVE_TCMALLOC -DUSE_TCMALLOC $(FLAGS) --std=gnu99 -g3 -O0 -o memorytest memorytest.c zmalloc.c -pthread -ltcmalloc

clean:
	rm -f memorytest
