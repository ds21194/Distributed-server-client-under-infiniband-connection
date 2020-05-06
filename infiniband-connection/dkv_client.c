//
// Created by dani94 on 7/7/19.
//

#include "dkv_client.h"


#define MAX_CACHE_SIZE (100000)

// --------------------------------- private declarations: ---------------------------------------//

typedef struct{
    struct kv_server_address* addresses;
    void **connectionsHandler;
    uint servers_num;
}_dkv_connections;

int _server_address_cmp(const void *arg1, const void *arg2);

uint _count_servers(struct kv_server_address *servers);

int _duplicate_check(struct kv_server_address *servers, uint srvs_num, struct kv_server_address
        *indexer);

void _cpy_kv_server_address(struct kv_server_address *dest, struct kv_server_address *source);

// ------------------------------------ implementations: -----------------------------------------//

void _cpy_kv_server_address(struct kv_server_address *dest, struct kv_server_address *source){
    size_t address_len = strlen(source->server_name);
    dest->server_name = (char*)malloc(address_len+1);
    dest->port = source->port;
    strncpy(dest->server_name, source->server_name, address_len);
}

int _server_address_cmp(const void *arg1, const void *arg2){
    if(arg1 == NULL || arg2 == NULL){
        fprintf(stderr, "Could not compare since one of the values are NULL");
        return -2;
    }

    struct kv_server_address *srv1 = (struct kv_server_address*)arg1;
    struct kv_server_address *srv2 = (struct kv_server_address*)arg2;

    if(strcmp(srv1->server_name, srv2->server_name) < 0) return -1;
    if(strcmp(srv1->server_name, srv2->server_name) > 0) return 1;
    if(srv1->port < srv2->port) return -1;
    if(srv1->port > srv2->port) return 1;
    return 0;
}

uint _count_servers(struct kv_server_address *servers){
    struct kv_server_address *server = servers;
    uint counter = 0;
    while (server != NULL && server->server_name != NULL){
        counter++;
        server++;
    }
    return counter;
}

int _duplicate_check(struct kv_server_address *servers, uint srvs_num, struct kv_server_address
        *indexer){
    //checks if indexer is in servers list:
    for(uint i = 0; i < srvs_num; i++){
        if(_server_address_cmp(indexer, servers+i) == 0 && (&servers[i])->port == indexer->port){
            fprintf(stderr, "indexer server + port appears in servers list\n");
            return -1;
        }
    }

    //checks inside servers list for duplications:
    //sort and search for two adjacent servers if there is duplication:
    qsort(servers, srvs_num, sizeof(*servers), _server_address_cmp);
    for(uint i = 0; i < srvs_num-1; i++){
        if(_server_address_cmp(servers+i, servers+i+1) == 0
        &&(&servers[i])->port == (&servers[i+1])->port){
            fprintf(stderr, "found duplication on servers list (server + port appears twice or more");
            return -1;
        }
    }

    return 0;
}


int dkv_open(struct kv_server_address *servers, struct kv_server_address *indexer, void **dkv_h){
    uint servers_number = _count_servers(servers);
    printf("number of servers is: %d\n", servers_number); //TODO: delete

    if(_duplicate_check(servers, servers_number, indexer)) return -1;

    _dkv_connections *dkv_c = (_dkv_connections*)malloc(sizeof(*dkv_c));

    dkv_c->servers_num = servers_number+1; // including indexer

    //memory allocation:
    dkv_c->addresses = (struct kv_server_address*)calloc((servers_number+1),
            sizeof(*dkv_c->addresses));
    dkv_c->connectionsHandler = calloc((servers_number+1), sizeof(*dkv_c->connectionsHandler));
    if(dkv_c->addresses == NULL || dkv_c->connectionsHandler == NULL){
        fprintf(stderr, "Could not allocate memory");
        return -1;
    }

    //open connection with indexer:
    if(kv_open(indexer->server_name, 0, dkv_c->connectionsHandler)){
        free(dkv_c->connectionsHandler);
        free(dkv_c->addresses);
        return -1;
    }

    _cpy_kv_server_address(dkv_c->addresses, indexer);

    //open connection with servers:
    for(uint i = 0; i < servers_number; i++){
        if(kv_open((&servers[i])->server_name, MAX_CACHE_SIZE, &dkv_c->connectionsHandler[i+1])){
            free(dkv_c->connectionsHandler);
            free(dkv_c->addresses);
            return -1;
        }

        _cpy_kv_server_address(dkv_c->addresses+i+1, servers+i);
    }

    *dkv_h = (void*)dkv_c;

    return 0;

}

int dkv_close(void *dkv_h){
    _dkv_connections* dkv_c = (_dkv_connections*)dkv_h;

    //close connection with indexer:
    kv_close(*dkv_c->connectionsHandler);
    free(dkv_c->addresses->server_name);

    //close connection with other servers:
    for(uint i = 1; i < dkv_c->servers_num; i++){
        free((&dkv_c->addresses[i])->server_name);
        if(kv_close(*(dkv_c->connectionsHandler+i))) return -1;
    }

    free(dkv_c->connectionsHandler);
    free(dkv_c->addresses);
    return 0;
}

int dkv_set(void* dkv_h, const char* key, const char *value){
    _dkv_connections *dkv_c = (_dkv_connections*)dkv_h;
    static int chosen_server = 1;

    //save key and the right server in the indexer:
    printf("server chosen is: %s with port %d\n", dkv_c->addresses[chosen_server].server_name,
           dkv_c->addresses[chosen_server].port); //TODO: delete

    char* server_value = malloc(10);
    sprintf(server_value, "%d", chosen_server);
    if(kv_set(*dkv_c->connectionsHandler, key, server_value)) return -1;

    //save key and value in the chosen server:
    void *current_handler = *(dkv_c->connectionsHandler + chosen_server);
    if(kv_set(current_handler, key, value)) return -1;

    chosen_server++;
    if(chosen_server % (dkv_c->servers_num) == 0) chosen_server = 1;

    free(server_value);
    return 0;
}


int dkv_get(void *dkv_h, const char *key, char **value, unsigned *length){
    _dkv_connections *dkv_c = (_dkv_connections*)dkv_h;
    char* server_value;

    //get the right server to search for key:
    printf("searching value with key: %s\n", key);
    if(kv_get(*dkv_c->connectionsHandler, key, &server_value)) return -1;

    if(strcmp(server_value, "") == 0){
        *value = "";
        *length = 0;
        return 0;
    }

//    long chosen_server = atoi(server_value); // TODO: switch if not working
    long chosen_server = strtol(server_value, NULL, 10);

    printf("got value from server %s with port %d\n", dkv_c->addresses[chosen_server].server_name,
           dkv_c->addresses[chosen_server].port);

    // get value from the right server:
    void *current_handler = *(dkv_c->connectionsHandler + chosen_server);
    if(kv_get(current_handler, key, value)) return -1;

    *length = (unsigned)strlen(*value);

    return 0;
}

void dkv_release(char *value){
    kv_release(value);
}
