#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define KEYLEN	10
#define VALLEN	90

#define MAXPARTITIONS	512			// Needs to fit in a uint32_t
#define POWEROFTWO(x)	(!(x & (x - 1)) && x)

#ifdef USE_STDIO
#	define STDIO_BUFFER_SIZE	(1 * 1024 * 1024)
#endif

struct record {
  union {
    uint8_t key[KEYLEN];
    uint32_t msb32;
  } __attribute__ ((packed));
  uint8_t val[VALLEN];
} __attribute__ ((packed));

static size_t filesize(const char *filename)
{
  struct stat buf;
  int r = stat(filename, &buf);
  assert(r == 0);
  return buf.st_size;
}

#ifdef PERF_DEBUG
__attribute__ ((unused))
static float ts2s(struct timespec *ts)
{
  return (float)ts->tv_sec + (ts->tv_nsec / (float)1000000000.0);
}
#endif

#endif
