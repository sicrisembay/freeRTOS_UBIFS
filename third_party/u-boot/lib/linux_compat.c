#ifndef __ZPL_BUILD__
#include <common.h>
#include <linux/compat.h>
#else
#include "string.h"
#include "zplCompat.h"
#include "linux/compat.h"
#endif /* __ZPL_BUILD__ */

struct p_current cur = {
	.pid = 1,
};

struct p_current *current = &cur;

unsigned long copy_from_user(void *dest, const void *src,
		     unsigned long count)
{
	memcpy((void *)dest, (void *)src, count);
	return 0;
}

void *pvPortMalloc( size_t xWantedSize );

void *kmalloc(size_t size, int flags)
{
	void *p;

	p = pvPortMalloc(size);
	if (flags & __GFP_ZERO)
		memset(p, 0, size);

	return p;
}

struct kmem_cache *get_mem(int element_sz)
{
	struct kmem_cache *ret;

	ret = pvPortMalloc(sizeof(struct kmem_cache));
	ret->sz = element_sz;

	return ret;
}

void *kmem_cache_alloc(struct kmem_cache *obj, int flag)
{
	return pvPortMalloc(obj->sz);
}
