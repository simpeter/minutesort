# All the -m flags didn't seem to make a performance difference
#CFLAGS = -O3 -march=skylake-avx512 -mavx512f -mavx512cd -mavx512vl -mavx512bw -mavx512dq -g -std=gnu99 -Wall
CFLAGS = -O3 -g -std=gnu99 -Wall
#CFLAGS = -g -std=gnu99 -Wall
CFLAGS += -DPERF_DEBUG
LDFLAGS = -lm

all: qsort partition check inplace

#qsort: CFLAGS += -DUSE_PMDK -DDRAM_KEYS
qsort: CFLAGS += -DUSE_STDIO -DDRAM_KEYS -DUSE_HUGEPAGES
# qsort: LDFLAGS = -lpmem
qsort: qsort.o quicksort.o
inplace: inplace.o
partition: CFLAGS += -DUSE_STDIO
partition: partition.o
check: check.o

clean:
	rm -f *.o qsort partition check
