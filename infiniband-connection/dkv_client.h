//
// Created by dani94 on 7/7/19.
//

#ifndef NETWORK_WORKSHOP_EX1_DKV_CLIENT_H
#define NETWORK_WORKSHOP_EX1_DKV_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include "kv_client.h"
#include "utils/helper.h"

struct kv_server_address{
    char* server_name;
    short port;
};

int dkv_open(struct kv_server_address *servers, struct kv_server_address *indexer, void **dkv_h);

int dkv_set(void *dkv_h, const char *key, const char *value);

int dkv_get(void *dkv_h, const char *key, char **value, unsigned *length);

void dkv_release(char *value);

int dkv_close(void *dkv_h);

#endif //NETWORK_WORKSHOP_EX1_DKV_CLIENT_H
