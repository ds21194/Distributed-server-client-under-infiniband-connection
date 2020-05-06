//
// Created by dani94 on 6/18/19.
//

#include "memoryPool.h"

#define MAX_MEMORY_SIZE (5000000)
#define ALLOCATION_AMOUNT (100)
#define KEY_SIZE (65)
#define EMPTY ("")


typedef struct
{
    char* key;
    char* value;
}record;

size_t memory_size;
record* placeholder;
record* MEMORY;

record* _next_placeholder(record* placeholder){
    if(placeholder + 1 == MEMORY + memory_size){
        MEMORY = (record*)realloc(MEMORY, 2*memory_size);
        if(MEMORY == NULL){
            fprintf(stderr, "could not allocate more memory");
            return NULL;
        }
        memory_size *= 2;
    }
    placeholder++;
    return placeholder;
}

int _get_record_index(const char* key){
    for(size_t i = 0; i < memory_size; i++){
        if(MEMORY[i].key == NULL){
            return -1;
        }
        if(strcmp(MEMORY[i].key, key) == 0){
            return (int)i;
        }
    }
    return -1;
}

int memory_pool_create(size_t memory_amount){
    memory_size = memory_amount;
    MEMORY = (record*)calloc(1, memory_amount*sizeof(*MEMORY));
    if(MEMORY == NULL){
        fprintf(stderr, "could not allocate memory in memoryHandler\n");
        return -1;
    }
    placeholder = MEMORY;
    return 0;
}


int add_record(const char* key, const char* value, size_t value_size){
//    char* val = get_record(key);
    int index = _get_record_index(key);
    if(index > -1 && strcmp(MEMORY[index].value, EMPTY) != 0){
        MEMORY[index].value = (char*)realloc(MEMORY[index].value, value_size + 1);
        strncpy(MEMORY[index].value, value, value_size);
        return 0;
    }

    placeholder->key = (char*)malloc(KEY_SIZE);
    placeholder->value = (char*)malloc(value_size+1);
    if(placeholder->value == NULL || placeholder->key == NULL){
        fprintf(stderr, "could not allocate memory in memoryHandler\n");
        return -1;
    }

    memcpy(placeholder->key, key, KEY_SIZE);
    memcpy(placeholder->value, value, value_size+1);

    placeholder = _next_placeholder(placeholder);
    if(placeholder == NULL){
        return -1;
    }

    return 0;
}

char* get_record(const char* key){
    for(size_t i = 0; i < memory_size; i++){
        if(MEMORY[i].key == NULL){
            return EMPTY;
        }
        if(strcmp(MEMORY[i].key, key) == 0){
            return MEMORY[i].value;
        }
    }
    return "";
}

void memory_pool_destroy(){
    for(uint i = 0; i < memory_size; i++){
        free(MEMORY[i].key);
        free(MEMORY[i].value);
        MEMORY[i].key = NULL;
        MEMORY[i].value = NULL;
    }
    free(MEMORY);
    MEMORY = NULL;
}

