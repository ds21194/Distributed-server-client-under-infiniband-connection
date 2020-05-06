//
// Created by dani94 on 5/6/19.
//

#include "server.h"


#define MEMORY_SPACE (10000000) // 10MB
#define EAGER_THRESHOLD (4000)
#define KEY_SIZE (4000)
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

// --------------------------------- private declarations: ---------------------------------------//

int _server_connection_create(arguments *args, connectionContext *session);


int _handle_requests(connectionContext *session);


int _connections_handler(connectionContext *session);

ibInformation* _destination_ib_info_create(arguments *args, ibInformation *ib_source){
    _arguments *_args = (_arguments*)args;
    struct addrinfo *res, *t;
    struct addrinfo hints = {
            .ai_flags    = AI_PASSIVE,
            .ai_family   = AF_INET,
            .ai_socktype = SOCK_STREAM
    };
    char *service;
    char msg[sizeof "0000:000000:000000"];
    ssize_t n;
    int sockfd = -1, connfd;

    if (asprintf(&service, "%d", _args->tcp_port) < 0)  return NULL;

    n = getaddrinfo(NULL, service, &hints, &res);

    if (n < 0)
    {
        fprintf(stderr, "%s for tcp_port %d\n", gai_strerror((int)n), _args->tcp_port);
        free(service);
        service = NULL;
        return NULL;
    }

    for (t = res; t; t = t->ai_next)
    {
        sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
        if (sockfd >= 0)
        {
            n = 1;

            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

            if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
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
    res = NULL;

    if (sockfd < 0)
    {
        fprintf(stderr, "Couldn't listen to tcp_port %d\n", _args->tcp_port);
        return NULL;
    }

    listen(sockfd, 1);
    connfd = accept(sockfd, NULL, 0);
    close(sockfd);
    if (connfd < 0)
    {
        fprintf(stderr, "accept() failed\n");
        return NULL;
    }

    ibInformation* remote_dest = (ibInformation*) malloc(_args->streams_number*(sizeof
            *remote_dest));
    if (!remote_dest) goto out;

    for(uint i = 0; i < _args->streams_number; i++)
    {
        n = read(connfd, msg, sizeof msg);
        if (n != sizeof msg)
        {
            free(remote_dest);
            remote_dest = NULL;
            perror("server read");
            fprintf(stderr, "%zd/%zu: Couldn't read remote address\n", n, sizeof msg);
            goto out;
        }

        sscanf(msg, "%hu:%x:%x", &(&remote_dest[i])->lid, &(&remote_dest[i])->qpn, &(&remote_dest[i])->psn);
        printf("remote address: %hu:%x:%x\n", (&remote_dest[i])->lid, (&remote_dest[i])->qpn, (&remote_dest[i])->psn);

        sprintf(msg, "%04hu:%06x:%06x", (&ib_source[i])->lid, (&ib_source[i])->qpn,
                (&ib_source[i])->psn);
        printf("local address: %hu:%x:%x\n", (&ib_source[i])->lid, (&ib_source[i])->qpn,
               (&ib_source[i])->psn);

        if (write(connfd, msg, sizeof msg) != sizeof msg)
        {
            free(remote_dest);
            remote_dest = NULL;
            fprintf(stderr, "Couldn't send local address\n");
            goto out;
        }

        read(connfd, msg, sizeof msg);
    }

    out:
    close(connfd);
    return remote_dest;
}

int _server_connection_create(arguments *args, connectionContext *session){
    _arguments *_args = (_arguments*)args;
    for(uint i = 0; i < _args->streams_number; i++)
    {
        if (context_init(&session[i], (arguments) _args)) return -1;
        (&session[i])->routs = get_post_receive(&session[i]);
        if ((&session[i])->routs == -1)
        {
            return -1;
        }
    }

    ibInformation* my_dest = source_ib_information_create(session);
    if(!my_dest){
        return -1;
    }

    ibInformation* remote_dest = _destination_ib_info_create((arguments) _args, my_dest);
    if(remote_dest == NULL){
        ib_information_destroy(my_dest);
        return -1;
    }

    for(uint i = 0; i < _args->streams_number; i++)
    {
        if (context_connect(&session[i], (arguments) _args, (&my_dest[i])->psn, (&remote_dest[i])))
        {
            fprintf(stderr, "Couldn't connect to remote QP\n");
            ib_information_destroy(my_dest);
            ib_information_destroy(remote_dest);
            return -1;
        }
        else
        { printf("connected successfully\n"); }
    }

    ib_information_destroy(my_dest);
    ib_information_destroy(remote_dest);

    return 0;
}


int _handle_requests(connectionContext *session){

//    memset(session->buffer, '\0', sizeof(_packet));
    if(wait_for_completion(session, LOCAL_RECV_WRID)) return -1;

    _packet *packet = (_packet*)session->buffer;
    _packet_type type = packet->type;
    if(type < 0) return -1;

    printf("dealing with packet with type: %d\n", packet->type);

    if(type == EAGER_SET_REQUEST){
        //get key and vale sizes:
        size_t value_size = packet->data._eager_set_request.value_length;
        size_t key_size = packet->data._eager_set_request.key_length;
        //get key and values:
        char* key = (char*)(&packet->data+1);
        char* value = (char*)(&packet->data+1) + key_size + 1;

        add_record(key, value, value_size);

        //send message to client to announce that I got the message:
        packet->type = EAGER_SET_RESPONSE;
        if(send_message(session, sizeof(*packet), IBV_WR_SEND, NULL, NULL, 0)){
            fprintf(stderr, "server failed to send message with size %zd", strlen(DONE));
            return -1;
        }

        if(wait_for_completion(session, LOCAL_SEND_WRID)) return -1;

//        memset(packet, '\0', sizeof(packet) + value_size + key_size);
        return 0;
    }

    if(type == EAGER_GET_REQUEST){
        //get key size:
        size_t key_size = packet->data._eager_get_request.key_length;
        char* key = (char*)(&packet->data + 1);

        //searching for the right value:
        char* value = get_record(key);
        size_t value_size = strlen(value);

        size_t message_size = sizeof(*packet) + value_size + 1;
        //send the answer with EAGER:
        memset(packet, '\0', sizeof(*packet)+key_size);

        //send answer with EAGER protocol:
        if(message_size < EAGER_THRESHOLD)
        {
            packet->type = EAGER_GET_RESPONSE;
            packet->data._eager_get_response.value_length = value_size;
            if(strcmp(value, EMPTY) == 0)
                sprintf((char*)(&packet->data+1), "%s", "\0");
            else
                sprintf((char*)(&packet->data+1), "%s", value);
            packet->data._eager_get_response.value = ((char*)(&packet->data+1));

            if(send_message(session, message_size, IBV_WR_SEND, NULL, NULL, 0)){
                fprintf(stderr, "server failed to send message with size %zd", message_size);
                return -1;
            }
            return 0;
        }

        printf("sending message back with rendezvous\n");
        //send answer with RENDEZVOUS protocol:
        packet->type = RENDEZVOUS_GET_REQUEST;
        packet->data._rndv_get_request.value_length = value_size;
        packet->data._rndv_get_request.remote_buffer = session->buffer + sizeof(*packet);
        packet->data._rndv_get_request.rkey = session->mr->rkey;

        //write value on buffer so he can be read remotely
        sprintf((char*)(packet+1), "%s", value);

        // Send server details so the client could perform RDMA READ
        message_size = sizeof(*packet) + 1;
        if(send_message(session, message_size, IBV_WR_SEND, NULL, NULL, 0)){
            fprintf(stderr, "Error trying to send server details(rkey, ect) to client\n");
        }
        printf("sending following details: \n\trkey: %d\n\tpointer: %p\n\tvalue size: %zd\n",
               packet->data._rndv_get_request.rkey, packet->data._rndv_get_request.remote_buffer,
               packet->data._rndv_get_request.value_length); //TODO: delete

        //Wait for the client to send ack so we know that he finished to read.
        if(wait_for_completion(session, LOCAL_RECV_WRID)) return -1;

        if(packet->type != RENDEZVOUS_GET_RESPONSE) {
            fprintf(stderr, "ERROR: didn't get the right got approval - need eager set response\n");
        }

//        memset(packet, '\0', message_size);
        memset(session->buffer, '\0', message_size + value_size);
        return 0;
    }

    if(type == RENDEZVOUS_SET_REQUEST){
        void* remote_ptr = packet->data._rndv_set_request.remote_buffer;
        uint32_t rkey = packet->data._rndv_set_request.rkey;
        size_t value_size = packet->data._rndv_set_request.value_length;
        size_t key_size = packet->data._rndv_set_request.key_length;

        size_t message_size = value_size + key_size + 2;

        //read from remote buffer to my buffer: (not sending anything actually but receiving, the
        // function name is deceiving)
        if(send_message(session, message_size, IBV_WR_RDMA_READ, NULL, remote_ptr, rkey)){
            fprintf(stderr, "can not remote read from client message with size %zd", message_size);
        }

        //wait for remote read to finish it's job
        if(wait_for_completion(session, REMOTE_READ)) return -1;

        char* key = (char*)packet;
        char* value = (char*)packet + key_size + 1;

        add_record(key, value, value_size);
        printf("added to memory:\n\t key: %s\n\t value: %s\n", key, value); // TODO: delete

        //send to client response so he know I got all what needed to pass
        packet->type = RENDEZVOUS_SET_RESPONSE;
        if(send_message(session, sizeof(*packet), IBV_WR_SEND, NULL, NULL, 0)){
            fprintf(stderr, "Server failed to send RDMA read ack\n");
            return -1;
        }

        wait_for_completion(session, LOCAL_SEND_WRID);
        memset(packet, '\0', sizeof(packet) + value_size + key_size);
        return 0;
    }

    fprintf(stderr, "packet type out of range, got: %d\n", packet->type);

    return 0;
}


int _connections_handler(connectionContext *session){
    while(1){
        int a = _handle_requests(session);
        if(a == 1){
            printf("client decided to close the connection!\n");
            break;
        }
        if(a == -1){
            return -1;
        }
    }
    return 0;
}


// ------------------------------------ implementations: -----------------------------------------//


int server_runner(){
    _arguments* args = arguments_create(NULL);
    if(args == NULL) return -1;

    connectionContext* session = context_create(args->streams_number);
    if(session == NULL){
        arguments_destroy((arguments) args);
        return -1;
    }

    if(_server_connection_create((arguments) args, session)){
        context_destroy(session, args->streams_number);
        arguments_destroy((arguments) args);
        return -1;
    }

    memory_pool_create(MEMORY_SPACE);

    for(uint i = 0; i < session->streams_number; i++)
    {
        if (_connections_handler(&session[i]))
        {
            context_destroy(session, args->streams_number);
            arguments_destroy((arguments) args);
            return -1;
        }
    }
    printf("connected successfully\n");

    memory_pool_destroy();
    context_destroy(session, args->streams_number);
    arguments_destroy((arguments) args);
    return 0;
}


int main(){
    if(server_runner() < 0) return -1;
    printf("finished!");
    return 0;
}

