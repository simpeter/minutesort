#include <stdlib.h>
#include <stdio.h>
#include "quicksort.h"

static int compare(const void *a, const void *b, void *dummy)
{
  int *x = (int *)a, *y = (int *)b;

  return (*x) - (*y);
}
  
int main(int argc, char **argv)
{
  static int unsorted[16];

  for(int i = 0; i < 16; i++) {
    unsorted[i] = ((float)rand() / (float)RAND_MAX) * 15;
    printf("unsorted[%d] = %u\n", i, unsorted[i]);
  }
  
  for(int i = 0; i < 16; i++) {
    quickselect(unsorted, 16, sizeof(int), compare, NULL, &unsorted[i]);
    printf("sorted[%d] = %u\n", i, unsorted[i]);
  }
  
  return 0;
}
