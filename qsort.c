#include "shared.h"
#ifdef USE_PMDK
#	include <libpmem.h>

#	define MEMCPY pmem_memcpy_nodrain
#else
#	define MEMCPY memcpy
#endif

#if defined(USE_PMDK) && defined(USE_STDIO)
#	error PMDK and stdio are mutually exclusive!
#endif

#ifdef DRAM_KEYS
struct keyptr {
  /* struct record *ptr; */
  uint32_t recnum;
  uint8_t key[KEYLEN];
} __attribute__ ((packed));
#endif

static int infd[MAXPARTITIONS];
static struct record *unsorted[MAXPARTITIONS];
static ssize_t infsize[MAXPARTITIONS];

static int compare(const void *a, const void *b)
{
#ifdef DRAM_KEYS
  const struct keyptr *ra = a, *rb = b;
  return memcmp(&ra->key, &rb->key, KEYLEN);
#else
  const struct record * const *ra = a, * const *rb = b;
  return memcmp(&(*ra)->key, &(*rb)->key, KEYLEN);
#endif
}

int main(int argc, char *argv[])
{
  int r;
#ifdef PERF_DEBUG
  struct timespec start;
  r = clock_gettime(CLOCK_MONOTONIC, &start);
  assert(r == 0);
#endif
  
  if(argc < 3) {
    printf("Usage: %s OUTFILE INFILES...\n", argv[0]);
    return 0;
  }

  ssize_t fsize = 0;
  int npartitions = argc - 2;
  for(int i = 0; i < npartitions; i++) {
    infd[i] = open(argv[i + 2], O_RDONLY);
    assert(infd[i] != -1);
    infsize[i] = filesize(argv[i + 2]);
    unsorted[i] = mmap(NULL, infsize[i], PROT_READ, MAP_SHARED | MAP_POPULATE, infd[i], 0);
    assert(unsorted != MAP_FAILED);
    fsize += infsize[i];
  }

  assert(fsize % sizeof(struct record) == 0);
  size_t nrecords = fsize / sizeof(struct record);
#ifdef USE_STDIO
  FILE *outfile = fopen(argv[1], "w");
  assert(outfile != NULL);
  r = setvbuf(outfile, malloc(STDIO_BUFFER_SIZE), _IOFBF, STDIO_BUFFER_SIZE);
  assert(r == 0);
#else
  int outfd = open(argv[1], O_CREAT | O_RDWR, 0644);
  assert(outfd != -1);
  r = ftruncate(outfd, fsize);
  assert(r == 0);

  struct record *sorted = mmap(NULL, fsize, PROT_WRITE, MAP_SHARED | MAP_POPULATE, outfd, 0);
  assert(sorted != MAP_FAILED);
#endif

#ifdef PERF_DEBUG
  struct timespec ptrgen;
  r = clock_gettime(CLOCK_MONOTONIC, &ptrgen);
  assert(r == 0);
#endif

  // Generate array of pointers
#ifdef DRAM_KEYS
#	ifdef USE_HUGEPAGES
  // Use explicit hugepages
  struct keyptr *unsorted_ptrs =
    mmap(NULL, sizeof(struct keyptr) * nrecords,
  	 PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE,
  	 -1, 0);
  assert(unsorted_ptrs != MAP_FAILED);
#	else
  struct keyptr *unsorted_ptrs = malloc(sizeof(struct keyptr) * nrecords);
  assert(unsorted_ptrs != NULL);
#	endif
#else
  struct record **unsorted_ptrs = malloc(sizeof(struct record *) * nrecords);
  assert(unsorted_ptrs != NULL);
#endif
  size_t cnt = 0;
  for(int n = 0; n < npartitions; n++) {
    assert(infsize[n] / sizeof(struct record) < (1 << 24));
    for(size_t i = 0; i < infsize[n] / sizeof(struct record); i++) {
#ifdef DRAM_KEYS
      /* unsorted_ptrs[cnt].ptr = &unsorted[n][i]; */
      unsorted_ptrs[cnt].recnum = (n << 24) | i;
      memcpy(&unsorted_ptrs[cnt].key, &unsorted[n][i].key, KEYLEN);
      cnt++;
#else
      unsorted_ptrs[cnt++] = &unsorted[n][i];
#endif
    }
  }
  assert(cnt == nrecords);

#ifdef PERF_DEBUG
  struct timespec sort_start;
  r = clock_gettime(CLOCK_MONOTONIC, &sort_start);
  assert(r == 0);
#endif

  // Sort the pointers
#ifdef DRAM_KEYS
  qsort(unsorted_ptrs, nrecords, sizeof(struct keyptr), compare);
#else
  qsort(unsorted_ptrs, nrecords, sizeof(struct record *), compare);
#endif

#ifdef PERF_DEBUG
  struct timespec sort_end;
  r = clock_gettime(CLOCK_MONOTONIC, &sort_end);
  assert(r == 0);
#endif

  // Store the output
  for(size_t i = 0; i < nrecords; i++) {
#ifdef USE_STDIO
#	ifdef DRAM_KEYS
    // Attempt to read keys from DRAM (made it slower)
/*     size_t ret = fwrite(&unsorted_ptrs[i].key, KEYLEN, 1, outfile); */
/*     assert(ret == 1); */
/*     ret = fwrite(&unsorted_ptrs[i].ptr->val, VALLEN, 1, outfile); */
/*     assert(ret == 1); */
    /* size_t ret = fwrite(unsorted_ptrs[i].ptr, sizeof(struct record), 1, outfile); */
    int fnum = unsorted_ptrs[i].recnum >> 24;
    size_t recnum = unsorted_ptrs[i].recnum & ((1 << 24) - 1);
    size_t ret = fwrite(&unsorted[fnum][recnum], sizeof(struct record), 1, outfile);
    assert(ret == 1);
#	else
    size_t ret = fwrite(unsorted_ptrs[i], sizeof(struct record), 1, outfile);
    assert(ret == 1);
#	endif
#else
#	ifdef DRAM_KEYS
    MEMCPY(&sorted[i], unsorted_ptrs[i].ptr, sizeof(struct record));
#	else
    MEMCPY(&sorted[i], unsorted_ptrs[i], sizeof(struct record));
#	endif
#endif
  }

#ifdef PERF_DEBUG
  struct timespec end;
  r = clock_gettime(CLOCK_MONOTONIC, &end);
  assert(r == 0);
#endif

  for(int i = 0; i < npartitions; i++) {
    close(infd[i]);
  }
#ifdef USE_STDIO
  fclose(outfile);
#else
  close(outfd);
#endif

#if !defined(DRAM_KEYS) || !defined(USE_HUGEPAGES)
  free(unsorted_ptrs);
#endif
  
#ifdef PERF_DEBUG
  struct timespec closetime;
  r = clock_gettime(CLOCK_MONOTONIC, &closetime);
  assert(r == 0);
  float tsetup = ts2s(&ptrgen) - ts2s(&start),
    tptrgen = ts2s(&sort_start) - ts2s(&ptrgen),
    tsort = ts2s(&sort_end) - ts2s(&sort_start),
    tstore = ts2s(&end) - ts2s(&sort_end),
    tclose = ts2s(&closetime) - ts2s(&end);

  fprintf(stderr, "time(s): setup = %.2f, ptrgen = %.2f, sort = %.2f, store = %.2f, close = %.2f\n",
	  tsetup, tptrgen, tsort, tstore, tclose);
#endif

  return 0;
}
