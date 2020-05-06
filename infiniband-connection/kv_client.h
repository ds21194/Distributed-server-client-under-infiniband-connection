//
// Created by dani94 on 5/6/19.
//

#ifndef NETWORK_WORKSHOP_EX1_CLIENT_H
#define NETWORK_WORKSHOP_EX1_CLIENT_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "infiniband-connection/establishment.h"
#include "utils/memoryCache.h"
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <getopt.h>
#include <arpa/inet.h>

/**
 * create an object kv_handle which we can use later to set values / get values from the server.
 * @param server_name name of server to connect to.
 * @param kv_handle
 * @param short port
 * @param size_of_cache
 * @return
 */
int kv_open(char* server_name, size_t size_of_cache, void** kv_handle);

/**
 * set new key and value to be store on the server.
 * @param kv_handle
 * @param key
 * @param value
 * @return
 */
int kv_set(void *kv_handle, const char* key, const char *value);

/**
 * get the value stored in the past on the server. the returned value is stored in 'value'.
 * @param kv_handle
 * @param key
 * @param value
 * @return
 */
int kv_get(void* kv_handle, const char* key, char **value);

/**
 * release the memory allocated in the function 'get'
 * @param value
 * @return
 */
void kv_release(char *value);

/**
 * close the connection and release all memory associated with kv_handle.
 * @param kv_handle
 * @return
 */
int kv_close(void *kv_handle);

#endif //NETWORK_WORKSHOP_EX1_CLIENT_H
