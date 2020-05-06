//
// Created by dani94 on 5/6/19.
//

#include "kv_client.h"

#define RESULT_FILE_NAME ("infiniband")

#define KEY_SIZE (4000)
#define EAGER_THRESHOLD (4000)
#define DONE ("Done")
#define EMPTY ("")


typedef struct
{
    char*  server_name;
    uint16_t tcp_port;
    uint8_t ib_port;
    uint32_t size;
    uint current_message_size;
    enum   ibv_mtu mtu;
    int    rx_depth; // number of receives to post at a time (each round)
    int    tx_depth; // round size (every round add amount of rx_depth RR)
    uint streams_number;
}_arguments;

typedef enum
{
    EAGER_GET_REQUEST = 1,
    EAGER_GET_RESPONSE = 2,
    EAGER_SET_REQUEST = 3,
    EAGER_SET_RESPONSE = 4,

    RENDEZVOUS_GET_REQUEST = 5,
    RENDEZVOUS_GET_RESPONSE = 6,
    RENDEZVOUS_SET_REQUEST = 7,
    RENDEZVOUS_SET_RESPONSE = 8,
}_packet_type;

union Data{
    struct {
        char *value;
        size_t value_length;
    } _eager_get_response;

    struct {
        char *key;
        size_t key_length;
    } _eager_get_request;

    struct {
        char *key;
        char *value;
        size_t key_length;
        size_t value_length;
    } _eager_set_request;

    /* RENDEZVOUS PROTOCOL PACKETS */
    struct {
        uint32_t rkey;
        size_t value_length;
        void *remote_buffer;
    } _rndv_get_request;

    struct {
        uint32_t rkey;
        size_t value_length;
        size_t key_length;
        void* remote_buffer;
    } _rndv_set_request;
};

typedef struct {
    _packet_type type;
    union Data data;

}_packet;

typedef struct{
    connectionContext *session;
    void* cache;
}_kv_object;
// --------------------------------- private declarations: ---------------------------------------//

int _connect_to_server(arguments *args, connectionContext *session);

ibInformation* _destination_ib_info_create(arguments *args, ibInformation *ib_source_info);

// ------------------------------------ implementations: -----------------------------------------//

int _connect_to_server(arguments *args, connectionContext *session){
    _arguments *_args = (arguments)args;
//    get the infiniband address of my NIC:
    ibInformation* my_dest;

    my_dest = source_ib_information_create(session);
    if(my_dest == NULL){
        return -1;
    }

//    get the infiniband address of the remote NIC:
    ibInformation* remote_dest = _destination_ib_info_create((arguments) _args, my_dest);
    if(remote_dest == NULL){
        ib_information_destroy(my_dest);
        return -1;
    }

    for(uint i = 0; i < session->streams_number; i++)
    {
        if (context_connect(&session[i], (arguments) _args, (&my_dest[i])->psn, (&remote_dest[i])))
        {
            ib_information_destroy(my_dest);
            ib_information_destroy(remote_dest);
            return -1;
        }
//        else
//        { printf("connected successfully\n"); }
    }

    ib_information_destroy(my_dest);
    ib_information_destroy(remote_dest);
    return 0;
}

ibInformation* _destination_ib_info_create(arguments *args, ibInformation *ib_source_info){
    _arguments* _args = (arguments)args;
    struct addrinfo *res = NULL, *t;
    struct addrinfo hints = {
            .ai_family   = AF_INET,
            .ai_socktype = SOCK_STREAM
    };

    char *service = NULL;
    char msg[sizeof "0000:000000:000000"];
    memset(msg, 0, sizeof msg);
    int n = 0;
    int sockfd = -1;

    if (asprintf(&service, "%d", _args->tcp_port) < 0) return NULL;

    n = getaddrinfo(_args->server_name, service, &hints, &res);

    if (n < 0)
    {
        fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), _args->server_name, _args->tcp_port);
        free(service);
        service = NULL;
        return NULL;
    }

    for (t = res; t; t = t->ai_next)
    {
        sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
        if (sockfd >= 0)
        {
            if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
            {
                break;
            }
            close(sockfd);
            sockfd = -1;
        }
    }

    freeaddrinfo(res);
    free(service);
    service = NULL;

    if (sockfd < 0)
    {
        fprintf(stderr, "Couldn't connect to %s:%d\n", _args->server_name, _args->tcp_port);
        return NULL;
    }

    ibInformation* remote_dest;
    remote_dest = (ibInformation*)calloc(1, _args->streams_number*(sizeof *remote_dest));
    if (!remote_dest) return NULL;

    for(uint i = 0; i < _args->streams_number; i++)
    {
        sprintf(msg, "%04hu:%06x:%06x", (&ib_source_info[i])->lid, (&ib_source_info[i])->qpn,
                (&ib_source_info[i])->psn);
        printf("local address: %hu:%x:%x\n", (&ib_source_info[i])->lid, (&ib_source_info[i])->qpn,
               (&ib_source_info[i])->psn);

        if (write(sockfd, msg, sizeof msg) != sizeof msg)
        {
            free(remote_dest);
            remote_dest = NULL;
            fprintf(stderr, "Couldn't send local address\n");
            close(sockfd);
            return NULL;
        }

        if (read(sockfd, msg, sizeof msg) != sizeof msg)
        {
            free(remote_dest);
            remote_dest = NULL;
            perror("client read");
            fprintf(stderr, "Couldn't read remote address\n");
            close(sockfd);
            return NULL;
        }

        write(sockfd, "done", sizeof "done");


        sscanf(msg, "%hu:%x:%x", &(&remote_dest[i])->lid, &(&remote_dest[i])->qpn, &(&remote_dest[i])->psn);
        printf("remote address: %hu:%x:%x\n", (&remote_dest[i])->lid, (&remote_dest[i])->qpn, (&remote_dest[i])->psn);
    }

    return remote_dest;
}

int kv_open(char* server_name, size_t size_of_cache, void** kv_handle) {
    *kv_handle = NULL;
    _arguments *args = arguments_create(server_name);
    if (args == NULL)
    {
        return -1;
    }
    connectionContext *session = context_create(args->streams_number);
    if (session == NULL)
    {
        arguments_destroy((arguments) args);
        return -1;
    }

    for (uint i = 0; i < args->streams_number; i++)
    {
        if (context_init(&session[i], (arguments) args))
        {
            context_destroy(session, i + 1);
            arguments_destroy((arguments) args);
            return -1;
        }

        //    add post receive request, "fill up the gas":
        (&session[i])->routs = get_post_receive(&session[i]);
        if ((&session[i])->routs == -1)
        {
            context_destroy(session, i + 1);
            arguments_destroy((arguments) args);
            return -1;
        }
    }

    if (_connect_to_server((arguments) args, session))
    {
        printf("failed\n");
        context_destroy(session, args->streams_number);
        arguments_destroy((arguments) args);
        session = NULL;
        args = NULL;
        return -1;
    }
    printf("connected successfully\n");

    _kv_object *kv_object = (_kv_object*)calloc(1, sizeof(*kv_object));
    void* cache = create_cache(size_of_cache);
    kv_object->session = session;
    kv_object->cache = cache;

    *kv_handle = (void*)kv_object;
    arguments_destroy((arguments) args);
    return 0;
}

int kv_close(void *kv_handle){
    _kv_object *kv_object = (_kv_object*)kv_handle;

    if(context_destroy((connectionContext *) kv_handle, kv_object->session->streams_number)){
        fprintf(stderr, "error release connection handler");
        return -1;
    }
    destroy_cache(kv_object->cache);
    return 0;
}

int kv_get(void* kv_handle, const char* key, char **value){
    _kv_object *kv_object = (_kv_object*)kv_handle;

    size_t key_len = strlen(key);
    if(key_len >= KEY_SIZE){
        fprintf(stderr, "key is to long\n");
        return -1;
    }

    char* ret_val;
    ret_val = find_in_cache(key, kv_object->cache);
    if(ret_val != NULL) {  // found in cache
        size_t value_len = strlen(ret_val);
        *value = (char*)malloc(value_len+1);
        strncpy(*value, ret_val, value_len+1);
        return 0;
    }

    //ask the server for a value:
    _packet *packet = (_packet*)kv_object->session->buffer;
    packet->type = EAGER_GET_REQUEST;
    packet->data._eager_get_request.key_length = key_len;
    sprintf((char*)(&packet->data+1), "%s", key);
    packet->data._eager_get_request.key = (char*)(&packet->data+1);

    printf("searching for:\n\tkey: %s\n\tkey length: %zd\n\tpacket type: %d\n", key, key_len,
            packet->type);

    //assuming key is always delivered by EAGER, because key size not extend 4000 bytes
    size_t message_size = sizeof(*packet) + key_len + 1;
    if(send_message(kv_object->session, message_size, IBV_WR_SEND, NULL, NULL, 0)){
        fprintf(stderr, "server failed to send message with size %zd\n", message_size);
        return -1;
    }

    //wait until we get a message from server:
    if(wait_for_completion(kv_object->session, LOCAL_RECV_WRID)) return -1;

    //parse the answer:
    _packet_type type = packet->type;
    if(type == EAGER_GET_RESPONSE){
        ret_val = (char*)(&packet->data + 1);
        add_to_cache(key, ret_val, kv_object->cache);
        size_t value_size = packet->data._eager_get_response.value_length;
        *value = (char*)malloc(value_size+1);
        strncpy(*value, ret_val, value_size+1);
        return 0;
    }

    if(type == RENDEZVOUS_GET_REQUEST){
        void* remote_ptr = packet->data._rndv_get_request.remote_buffer;
        uint32_t rkey = packet->data._rndv_get_request.rkey;
        size_t value_size = packet->data._rndv_get_request.value_length;
        message_size = value_size + 1;

        //read from remote buffer to my buffer: (not sending anything actually but receiving, the
        // name is deceiving)

        if(send_message(kv_object->session, message_size, IBV_WR_RDMA_READ, NULL, remote_ptr,
                rkey)){
            fprintf(stderr, "can not remote read from client message with size %zd", message_size);
        }

        printf("got from server following details: \n\trkey: %d\n\tpointer: %p\n\tvalue size: ""%zd\n",
               packet->data._rndv_get_request.rkey, packet->data._rndv_get_request.remote_buffer,
               packet->data._rndv_get_request.value_length); //TODO: delete

        //wait for remote read to finish it's job
        if(wait_for_completion(kv_object->session, REMOTE_READ)) return -1;

        ret_val = (char*)packet;
        printf("value is: %s\n", ret_val); // TODO: delete

        add_to_cache(key, ret_val, kv_object->cache);
        *value = (char*)malloc(value_size+1);
        strncpy(*value, ret_val, value_size+1);

        packet->type = RENDEZVOUS_GET_RESPONSE;
        if(send_message(kv_object->session, sizeof(*packet), IBV_WR_SEND, NULL, NULL, 0)){
            fprintf(stderr, "Client failed to send RDMA read ack\n");
        }
        wait_for_completion(kv_object->session, LOCAL_SEND_WRID);
        memset(packet, '\0', sizeof(packet) + value_size + 1);

        return 0;
    }

    return 0;
}

int kv_set(void *kv_handle, const char* key, const char *value){
    _kv_object *kv_object = (_kv_object*)kv_handle;

//    connectionContext* session = (connectionContext*)kv_handle;
    size_t value_len = strlen(value);
    size_t key_len = strlen(key);

    size_t message_size = sizeof(_packet) + value_len + key_len + 2;

    if(key_len >= KEY_SIZE){
        fprintf(stderr, "key is to long");
    }

    //if key is exist in cache, update it's value in cache:
    char* o_value = find_in_cache(key, kv_object->cache);
    if(o_value != NULL && strcmp(value, o_value) != 0){
        add_to_cache(key, value, kv_object->cache);
    }

    _packet *packet = (_packet*)kv_object->session->buffer;

//    printf("sending follwoing packet:\n\tkey: %s\n\tvalue: %s\n\ttype: %d\n", key, value,
//           packet->type); //TODO: delete

    memset(kv_object->session->buffer, '\0', sizeof(packet)+value_len+key_len+2+100);

    //send message with eager protocol:
    if(message_size < EAGER_THRESHOLD){
        packet->type = EAGER_SET_REQUEST;
        //copy key:
        packet->data._eager_set_request.value_length = value_len;
        packet->data._eager_set_request.key_length = key_len;
        sprintf((char*)(&packet->data+1), "%s", key);
        sprintf((char*)(&packet->data+1)+key_len + 1, "%s", value);
        packet->data._eager_set_request.key = (char*)(&packet->data+1);
        packet->data._eager_set_request.value = (char*)(&packet->data+1) + key_len + 1;

//        printf("sending follwoing packet:\n\tkey: %s\n\tvalue: %s\n\ttype: %d\n", key, value,
//                packet->type); //TODO: delete

        if(send_message(kv_object->session, message_size, IBV_WR_SEND, NULL, NULL, 0)){
            fprintf(stderr, "server failed to send message with size %zd\n", message_size);
            return -1;
        }
        //wait for server to get a finished message:
        if(wait_for_completion(kv_object->session, LOCAL_RECV_WRID)) return -1;

        if(packet->type != EAGER_SET_RESPONSE){
            fprintf(stderr, "ERROR: didn't get the right got approval - need eager set response\n");
        }

        memset(packet, '\0', message_size);
        return 0;
    }

    //send message with rendezvous protocol:
    packet->type = RENDEZVOUS_SET_REQUEST;
    packet->data._rndv_set_request.value_length = value_len;
    packet->data._rndv_set_request.key_length = key_len;
    packet->data._rndv_set_request.remote_buffer = kv_object->session->buffer + sizeof(*packet);
    packet->data._rndv_set_request.rkey = kv_object->session->mr->rkey;

    //write to the local buffer so the server could do RDMA READ.
    sprintf((char*)(packet+1), "%s", key);
    sprintf((char*)(packet+1)+key_len + 1, "%s", value);

//    printf("sending follwoing packet:\n\tkey: %s\n\tvalue: %s\n\ttype: %d\n", key, value,
//           packet->type); // TODO: delete

    message_size = sizeof(*packet)+1;
    if(send_message(kv_object->session, message_size, IBV_WR_SEND, NULL, NULL, 0)){
        fprintf(stderr, "server failed to send message with size %zd\n", message_size);
        return -1;
    }

    //wait for rendezvous set response
    if(wait_for_completion(kv_object->session, LOCAL_RECV_WRID)) return -1;

    if(packet->type != RENDEZVOUS_SET_RESPONSE) {
        fprintf(stderr, "ERROR: didn't get the right got approval - need eager set response\n");
    }

    memset(kv_object->session->buffer, '\0', message_size+value_len+key_len);
    return 0;
}

void kv_release(char *value){
    free(value);
    value = NULL;
}


//int main(int argc, char* argv[]){
//
//    if(argc < 2){
//        fprintf(stderr, "need to provide name server\n");
//        return -1;
//    }
//    void *kv_handle;
//    kv_open(argv[1], &kv_handle);
//    if(kv_handle == NULL) return -1;
//    printf("\n");
//
////    EAGER TESTING:
//    char* ret_value;
//    char* name = "Daniel";
//    kv_set(kv_handle, name, "Writing C code");
//    kv_get(kv_handle, name, &ret_value);
//    printf("returned values are:\nkey: %s, value: %s\n\n", name, ret_value);
//
//    kv_set(kv_handle, name, "Writing less C code");
//    kv_get(kv_handle, name, &ret_value);
//    printf("returned values are:\nkey: %s, value: %s\n\n", name, ret_value);
//
////  RENDEZVOUS TESTING:
//    printf("\nRENDEZVOUS Testing:\n");
//    char* key = "some_key";
//    char* value = "This is a very long message. it has more then 30 letters, including spaces.";
//    char* val_from_mem;
//
//    kv_set(kv_handle, key, value);
//    printf("\n");
//
//    kv_get(kv_handle, key, &val_from_mem);
//    printf("returned values are:\nkey: %s, value: %s\n\n", key, val_from_mem);
//    kv_release(val_from_mem);
//
//    kv_get(kv_handle, key, &val_from_mem);
//    printf("returned values are:\nkey: %s, value: %s\n\n", key, val_from_mem);
//    kv_release(val_from_mem);
//
//    char* ovalue = "This is a very long message. it has even more then 70 letters, including "
//                   "spaces.";
//    kv_set(kv_handle, key, ovalue);
//    printf("\n");
//
//    kv_get(kv_handle, key, &val_from_mem);
//    printf("returned values are:\nkey: %s, value: %s\n\n", key, val_from_mem);
//    kv_release(val_from_mem);
//
//    if(kv_close(kv_handle)) return -1;
//
//    return 0;
//}

