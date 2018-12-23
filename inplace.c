#include "shared.h"

static int infd[MAXPARTITIONS];
static struct record *unsorted[MAXPARTITIONS];
static ssize_t infsize[MAXPARTITIONS];

static int compare(const void *a, const void *b)
{
  const struct record *ra = a, *rb = b;
  return memcmp(&ra->key, &rb->key, KEYLEN);
}

int main(int argc, char *argv[])
{
  int r;
#ifdef PERF_DEBUG
  struct timespec start;
  r = clock_gettime(CLOCK_MONOTONIC, &start);
  assert(r == 0);
#endif
  
  if(argc < 2) {
    printf("Usage: %s FILES...\n", argv[0]);
    return 0;
  }

  ssize_t fsize = 0;
  int npartitions = argc - 1;
  assert(npartitions == 1);
  for(int i = 0; i < npartitions; i++) {
    infd[i] = open(argv[i + 1], O_RDWR);
    assert(infd[i] != -1);
    infsize[i] = filesize(argv[i + 1]);
    unsorted[i] = mmap(NULL, infsize[i], PROT_READ | PROT_WRITE,
		       MAP_SHARED | MAP_POPULATE, infd[i], 0);
    assert(unsorted[i] != MAP_FAILED);
    fsize += infsize[i];
  }

  assert(fsize % sizeof(struct record) == 0);
  size_t nrecords = fsize / sizeof(struct record);

#ifdef PERF_DEBUG
  struct timespec sort_start;
  r = clock_gettime(CLOCK_MONOTONIC, &sort_start);
  assert(r == 0);
#endif

  // Sort the pointers
  qsort(unsorted[0], nrecords, sizeof(struct record), compare);

#ifdef PERF_DEBUG
  struct timespec sort_end;
  r = clock_gettime(CLOCK_MONOTONIC, &sort_end);
  assert(r == 0);
#endif

  for(int i = 0; i < npartitions; i++) {
    close(infd[i]);
  }
  
#ifdef PERF_DEBUG
  struct timespec closetime;
  r = clock_gettime(CLOCK_MONOTONIC, &closetime);
  assert(r == 0);
  float tsetup = ts2s(&sort_start) - ts2s(&start),
    tsort = ts2s(&sort_end) - ts2s(&sort_start);

  fprintf(stderr, "time(s): setup = %.2f, sort = %.2f\n", tsetup, tsort);
#endif

  return 0;
}
