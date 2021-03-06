#ifndef SHAPITO_CACHE_H
#define SHAPITO_CACHE_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_cache shapito_cache_t;

struct shapito_cache
{
	pthread_spinlock_t  lock;
	int                 count;
	int                 count_allocated;
	int                 total_allocated;
	int                 limit;
	int                 limit_size;
	int                 cache_size;
	shapito_stream_t   *list;
};

static inline void
shapito_cache_init(shapito_cache_t *cache)
{
	cache->list            = NULL;
	cache->count           = 0;
	cache->count_allocated = 0;
	cache->total_allocated = 0;
	cache->limit           = 100;
	cache->limit_size      = 10 * 1024;
	cache->cache_size      = 0;
	pthread_spin_init(&cache->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void
shapito_cache_free(shapito_cache_t *cache)
{
	shapito_stream_t *stream = cache->list;
	while (stream) {
		shapito_stream_t *next = stream->next;
		shapito_stream_free(stream);
		free(stream);
		stream = next;
	}
	pthread_spin_destroy(&cache->lock);
}

static inline void
shapito_cache_set_limit(shapito_cache_t *cache, int limit)
{
	cache->limit = limit;
}

static inline void
shapito_cache_set_limit_size(shapito_cache_t *cache, int size)
{
	cache->limit_size = size;
}

static inline shapito_stream_t*
shapito_cache_pop(shapito_cache_t *cache)
{
	pthread_spin_lock(&cache->lock);
	if (cache->count > 0) {
		shapito_stream_t *stream;
		stream = cache->list;
		cache->list = stream->next;
		stream->next = NULL;
		cache->count--;
		cache->cache_size -= shapito_stream_size(stream);
		pthread_spin_unlock(&cache->lock);
		shapito_stream_reset(stream);
		return stream;
	}
	cache->count_allocated++;
	cache->total_allocated++;
	pthread_spin_unlock(&cache->lock);

	shapito_stream_t *stream;
	stream = malloc(sizeof(shapito_stream_t));
	if (stream == NULL) {
		pthread_spin_lock(&cache->lock);
		cache->count_allocated--;
		pthread_spin_unlock(&cache->lock);
		return NULL;
	}
	shapito_stream_init(stream);
	return stream;
}

static inline void
shapito_cache_push(shapito_cache_t *cache, shapito_stream_t *stream)
{
	pthread_spin_lock(&cache->lock);
	int size_limit_hit;
	size_limit_hit = cache->limit_size > 0 &&
	                 cache->limit_size <= shapito_stream_size(stream);
	if (cache->limit == cache->count || size_limit_hit) {
		cache->count_allocated--;
		pthread_spin_unlock(&cache->lock);
		shapito_stream_free(stream);
		free(stream);
		return;
	}
	stream->next = cache->list;
	cache->list = stream;
	cache->count++;
	cache->cache_size += shapito_stream_size(stream);
	pthread_spin_unlock(&cache->lock);
}

static inline void
shapito_cache_stat(shapito_cache_t *cache, int *count, int *count_allocated,
                   int *total_allocated, int *cache_size)
{
	pthread_spin_lock(&cache->lock);
	*count = cache->count;
	*count_allocated = cache->count_allocated;
	*total_allocated = cache->total_allocated;
	*cache_size      = cache->cache_size;
	pthread_spin_unlock(&cache->lock);
}

#endif /* SHAPITO_CACHE_H */
