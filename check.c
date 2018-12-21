#include <math.h>
#include "shared.h"

static int compare(const struct record *a, const struct record *b)
{
  return memcmp(&a->key, &b->key, KEYLEN);
}

int main(int argc, char *argv[])
{
  if(argc < 2) {
    printf("Usage: %s FILE [PARTITIONS]\n", argv[0]);
    return 0;
  }

  // Open input file
  int npartitions = 0;
  if(argc == 3) {
    npartitions = atoi(argv[2]);
    assert(npartitions <= MAXPARTITIONS && npartitions > 1 && POWEROFTWO(npartitions));
  }
  int infd = open(argv[1], O_RDONLY);
  assert(infd != -1);
  size_t fsize = filesize(argv[1]);
  assert(fsize % sizeof(struct record) == 0);
  size_t nrecords = fsize / sizeof(struct record);
  /* assert(nrecords % npartitions == 0); */

  struct record *input = mmap(NULL, fsize, PROT_READ, MAP_SHARED | MAP_POPULATE, infd, 0);
  assert(input != MAP_FAILED);

  if(npartitions > 0) {
    // Verify partitioning
    int bits = log2(npartitions);
    uint32_t mypart = be32toh(input[0].msb32) >> (32 - bits);
    printf("This is partition %u\n", mypart);
    
    for(size_t i = 1; i < nrecords; i++) {
      uint32_t outpart = be32toh(input[i].msb32) >> (32 - bits);
      if(mypart != outpart) {
	printf("Record %zu: Partition %u\n", i, outpart);
      }
    }
  } else {
    // Verify sort order
    for(size_t i = 1; i < nrecords; i++) {
      if(compare(&input[i - 1], &input[i]) >= 0) {
	printf("Record %zu < %zu not true!\n", i - 1, i);
      }
    }
  }

  close(infd);
  return 0;
}
