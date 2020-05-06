//
// Created by dani94 on 6/18/19.
//

#ifndef NETWORK_WORKSHOP_EX1_DICTIONARY_H
#define NETWORK_WORKSHOP_EX1_DICTIONARY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * create new memory pool with 'memory amount' of memory
 * @param memory_amount number of entries the memory pool can store.
 * @return 0 upon success and -1 upon failure.
 */
int memory_pool_create(size_t memory_amount);

/**
 * release all memory stored during the process.
 */
void memory_pool_destroy();

/**
 * add an entry of 'key, value' to the memory. if there is not enough memory (meaning we didn't
 * asked for enough entries when we called 'memory_pool_create'), this function will give us more
 * memory.
 * ! can possibly be exploited to give heap overflow. not covering this case !'
 * @param key
 * @param value
 * @param value_size
 * @return
 */
int add_record(const char* key, const char* value, size_t value_size);

/**
 * get previously stored value record, which stored with key.
 * if key doesn't exist will return empty string (AKA "")
 * @param key
 * @return
 */
char* get_record(const char* key);

#endif //NETWORK_WORKSHOP_EX1_DICTIONARY_H
