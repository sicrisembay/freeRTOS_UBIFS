#ifndef _LINUX_LIST_SORT_H
#define _LINUX_LIST_SORT_H

#ifndef __ZPL_BUILD__
#include <linux/types.h>
#else
#include "zplCompat.h"
#endif /* __ZPL_BUILD__ */

struct list_head;

void list_sort(void *priv, struct list_head *head,
	       int (*cmp)(void *priv, struct list_head *a,
			  struct list_head *b));
#endif
