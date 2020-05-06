//
// Created by dani94 on 5/6/19.
//

#ifndef NETWORK_WORKSHOP_EX1_ESTABLISHMENT_H
#define NETWORK_WORKSHOP_EX1_ESTABLISHMENT_H

#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <getopt.h>
#include <string.h>
#include <netdb.h>
#include <math.h>
#include <pthread.h>

#define WC_BATCH (10)
#define MB (1000000)
#define no_argument		0
#define required_argument	1
#define optional_argument	2

static const uint MESSAGES_SIZE = 31; // can pass values up to 2GB memory.
static const size_t MEASURE_SIZE = 500;

#define END_MASSAGE ("FINISH\0")

enum
{
    LOCAL_RECV_WRID = 1,
    LOCAL_SEND_WRID = 2,
    REMOTE_READ = 3,
    REMOTE_RECV_WRID = 4,
};


typedef void* kv_handle;

typedef void* arguments;

typedef struct
{
    struct ibv_port_attr* port_info;
    struct ibv_context *context;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    void *buffer;
    enum ibv_mtu mtu;
    uint32_t size;
    int rx_depth;
    int routs;
    uint streams_number;
}connectionContext;


typedef struct
{
    uint16_t lid;
    uint32_t qpn;
    uint32_t psn;
}ibInformation;


void usage(const char *argv0);


// --------------------------------- objects creation: ---------------------------------------//

/**
 * create arguments object. necessary for the connection with other host.
 * @param server_name ip address of the remote server. NULL if the server is calling this function.
 * @return
 */
arguments arguments_create(char *server_name);

/**
 * create context information object. this is the most important object in this lib,
 * since it's saves the values of the connection.
 * @param streams_number
 * @return
 */
connectionContext* context_create(uint streams_number);

/**
 * create object that saves Infiniband's lid, ect.
 * @param session
 * @return
 */
ibInformation* source_ib_information_create(connectionContext *session);


// --------------------------------- objects destroy: ---------------------------------------//

/**
 * free the memory of the context connection
 * @param session
 * @param last_ses
 * @return
 */
int context_destroy(connectionContext *session, uint last_ses);

/**
 * free memory of arguments object
 * @param args
 */
void arguments_destroy(arguments *args);

/**
 * free memory if ib information
 * @param ib_info
 */
void ib_information_destroy(ibInformation* ib_info);


// --------------------------------- lib functions: ---------------------------------------//
/**
 * add more post receive so we will be able to receive request from the other host
 * @param session
 * @return the number of post receive that the host can use.
 */
int get_post_receive(connectionContext *session);

/**
 * init the connection session with all the necessary details in order to establish a connection
 * @param session
 * @param args
 * @return 0 upon success and 1 upon failure
 */
int context_init(connectionContext *session, arguments *args);

/**
 * connect between this host to 'dest' host.
 * @param session context of the connection.
 * @param args
 * @param my_psn packet-sequence-number
 * @param dest
 * @return
 */
int context_connect(connectionContext *session, arguments *args, uint32_t my_psn, ibInformation *dest);

/**
 * send a message to the other host. will work only if 'context_connect' has been called before.
 * @param session session context of the connection.
 * @param size size of message to send
 * @param opcode the code which will tell how to send / receive the message.
 * @param local_ptr the pointer which we will read the data from / to once received / sent.
 * @param remote_ptr if we using opcode RDMA_READ, this value is the pointer of the buffer on the
 * other host
 * @param remote_key key access to the remote buffer (only for reading)
 * @return
 */
int send_message(connectionContext *session, size_t size, enum ibv_wr_opcode opcode, void
                    *local_ptr, void *remote_ptr, uint32_t remote_key);

/**
 * wait until we get completion on job with code 'opcode_wait'
 * @param session
 * @param opcode_wait
 * @return 0 if we got the completion on the job we wanted, -1 for error.
 * !important! this function will not halt until we get that specific completion.
 */
int wait_for_completion(connectionContext *session, uint opcode_wait);


#endif //NETWORK_WORKSHOP_EX2_ESTABLISHMENT_H
