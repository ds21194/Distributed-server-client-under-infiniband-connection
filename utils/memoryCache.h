//
// Created by dani94 on 6/18/19.
//

#ifndef NETWORK_WORKSHOP_EX1_MEMORYCACHE_H
#define NETWORK_WORKSHOP_EX1_MEMORYCACHE_H

#include <string.h>
#include <stdio.h>
#include "uthash/uthash.h"

/**
 * create cache memory
 * @param memory_size
 */
void* create_cache(size_t memory_size);

/**
 * finds entry in cache
 * @param key
 * @return
 */
char *find_in_cache(const char *key, void* cache);

/**
 * add to cache entry with key and value.
 * @param key
 * @param value
 */
void add_to_cache(const char *key, const char *value, void* cache);

/**
 * delete object cache
 * @param cache
 */
void destroy_cache(void* cache);

#endif //NETWORK_WORKSHOP_EX1_MEMORYCACHE_H
