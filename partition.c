#include <math.h>
#include <endian.h>
#include "shared.h"

#ifdef USE_STDIO
static FILE *outfile[MAXPARTITIONS];
#else
static int outfd[MAXPARTITIONS];
#endif

int main(int argc, char *argv[])
{
#ifdef PERF_DEBUG
  struct timespec start;
  int r = clock_gettime(CLOCK_MONOTONIC, &start);
  assert(r == 0);
#endif
  
  if(argc < 4) {
    printf("Usage: %s INFILE BASENAME PARTITIONS\n", argv[0]);
    return 0;
  }

  // Open input file
  int npartitions = atoi(argv[3]);
  assert(npartitions <= MAXPARTITIONS && npartitions > 1 && POWEROFTWO(npartitions));
  int infd = open(argv[1], O_RDONLY);
  assert(infd != -1);
  size_t fsize = filesize(argv[1]);
  assert(fsize % sizeof(struct record) == 0);
  size_t nrecords = fsize / sizeof(struct record);
  assert(nrecords % npartitions == 0);

  // Open all output partitions
  for(int i = 0; i < npartitions; i++) {
    char fname[256];
    snprintf(fname, 256, "%s.%u", argv[2], i);
#ifdef USE_STDIO
    outfile[i] = fopen(fname, "w");
    assert(outfile[i] != NULL);
    int ret = setvbuf(outfile[i], malloc(STDIO_BUFFER_SIZE), _IOFBF,
		      STDIO_BUFFER_SIZE);
    assert(ret == 0);
#else
    outfd[i] = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    assert(outfd[i] != -1);
#endif
  }

  struct record *input = mmap(NULL, fsize, PROT_READ, MAP_SHARED | MAP_POPULATE, infd, 0);
  assert(input != MAP_FAILED);

#ifdef PERF_DEBUG
  struct timespec setup;
  r = clock_gettime(CLOCK_MONOTONIC, &setup);
  assert(r == 0);
#endif

  // Copy input to output range partitions
  int bits = log2(npartitions);
  for(size_t i = 0; i < nrecords; i++) {
    uint32_t outpart = be32toh(input[i].msb32) >> (32 - bits);
#ifdef USE_STDIO
    size_t ret = fwrite(&input[i], sizeof(struct record), 1, outfile[outpart]);
    assert(ret == 1);
#else
    ssize_t ret = write(outfd[outpart], &input[i], sizeof(struct record));
    assert(ret == sizeof(struct record));
#endif
  }

#ifdef PERF_DEBUG
  struct timespec end;
  r = clock_gettime(CLOCK_MONOTONIC, &end);
  assert(r == 0);
#endif

  close(infd);
  for(int i = 0; i < npartitions; i++) {
#ifdef USE_STDIO
    fclose(outfile[i]);
#else
    close(outfd[i]);
#endif
  }

#ifdef PERF_DEBUG
  struct timespec closetime;
  r = clock_gettime(CLOCK_MONOTONIC, &closetime);
  assert(r == 0);
  float tsetup = ts2s(&setup) - ts2s(&start),
    tpartition = ts2s(&end) - ts2s(&setup),
    tclose = ts2s(&closetime) - ts2s(&end);

  fprintf(stderr, "time(s): setup = %.2f, partition = %.2f, close = %.2f\n",
	  tsetup, tpartition, tclose);
#endif

  return 0;
}
