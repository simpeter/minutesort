#ifndef QUICKSORT_H
#define QUICKSORT_H

typedef int (*__compar_d_fn_t) (const void *, const void *, void *);

void _quicksort (void *const pbase, size_t total_elems,
		 size_t size, __compar_d_fn_t cmp, void *arg);

void
quickselect (void *const pbase, size_t total_elems, size_t size,
	     __compar_d_fn_t cmp, void *arg, void *elem);

#endif
