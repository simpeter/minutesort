#include <stdlib.h>
#include <stdio.h>

typedef int (*__compar_d_fn_t) (const void *, const void *, void *);

void _quicksort (void *const pbase, size_t total_elems,
		 size_t size, __compar_d_fn_t cmp, void *arg);

void
quickselect (void *const pbase, size_t total_elems, size_t size,
	     __compar_d_fn_t cmp, void *arg, void *elem);

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
