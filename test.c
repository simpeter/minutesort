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
  static int unsorted[32];

  for(int i = 0; i < 32; i++) {
    unsorted[i] = ((float)rand() / (float)RAND_MAX) * 1000;
    printf("unsorted[%d] = %u\n", i, unsorted[i]);
  }
  
  for(int i = 0; i < 32; i++) {
    quickselect(unsorted, 32, sizeof(int), compare, NULL, &unsorted[i]);
    printf("sorted[%d] = %u\n", i, unsorted[i]);
  }
  
  return 0;
}
