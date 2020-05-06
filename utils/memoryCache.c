//
// Created by dani94 on 6/18/19.
//

#include "memoryCache.h"


//struct CacheEntry *cache = NULL;
//size_t max_cache_size;
//int called_init = 0;

struct CacheEntry {
    char *key;
    char *value;
    UT_hash_handle hh;
};

typedef struct{
    struct CacheEntry *cache;
    size_t max_cache_size;
}_cache_param;


void* create_cache(size_t memory_size){
    _cache_param *params = (_cache_param*)calloc(1, sizeof(*params));
    params->max_cache_size = memory_size;
    params->cache = NULL;
    return params;
}


char *find_in_cache(const char *key, void* cache)
{
    if(cache == NULL)
        return NULL;
    _cache_param *params = (_cache_param*)cache;
    if(params->max_cache_size <= 0) return NULL;

    struct CacheEntry *entry;
    HASH_FIND_STR(params->cache, key, entry);
    if (entry) {
        // remove it (so the subsequent add will throw it on the front of the list)
        HASH_DELETE(hh, params->cache, entry);
        HASH_ADD_KEYPTR(hh, params->cache, entry->key, strlen(entry->key), entry);
        return entry->value;
    }
    return NULL;
}

void add_to_cache(const char *key, const char *value, void* cache)
{
    if(cache == NULL){
        fprintf(stderr, "call \"create_cache\" method first");
        return;
    }

    _cache_param *params = (_cache_param*)cache;

    if(params->max_cache_size <= 0) return;

    struct CacheEntry *entry, *tmp_entry;
    entry = malloc(sizeof(struct CacheEntry));
    entry->key = strdup(key);
    entry->value = strdup(value);
    HASH_ADD_KEYPTR(hh, params->cache, entry->key, strlen(entry->key), entry);

    // prune the cache to MAX_CACHE_SIZE
    if (HASH_COUNT(params->cache) >= params->max_cache_size) {
        HASH_ITER(hh, params->cache, entry, tmp_entry) {
            // prune the first entry (loop is based on insertion order so this deletes the oldest item)
            HASH_DELETE(hh, params->cache, entry);
            free(entry->key);
            free(entry->value);
            free(entry);
            break;
        }
    }
}

void destroy_cache(void* cache){
    if(cache == NULL){
        fprintf(stderr, "call \"create_cache\" method first");
        return;
    }

    free(cache);
}