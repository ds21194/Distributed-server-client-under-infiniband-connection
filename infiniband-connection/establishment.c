//
// Created by dani94 on 5/6/19.
//

#include <zconf.h>
#include <arpa/inet.h>
#include "establishment.h"

//const uint MESSAGES_SIZE = 21;
//const size_t MEASURE_SIZE = 500;

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

// --------------------------------- private functions: ---------------------------------------//
/**
 * registers new memory region for the use of the client/server
 * @param size amount of memory to register in bytes.
 * @param session
 * @param is_server
 * @return
 */
struct ibv_mr* _new_memory_region(size_t size, connectionContext *session, int is_server){

    long page_size = sysconf(_SC_PAGESIZE);
    session->buffer = calloc(1, (size_t)roundup(size, page_size));
    memset(session->buffer, 0x7b + is_server, size);
    if (!session->buffer)
    {
        fprintf(stderr, "Couldn't allocate work buffer.\n");
        return NULL;
    }

    memset(session->buffer, 0x7b, size);

    session->mr = ibv_reg_mr(session->pd, session->buffer, size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);
    if (!session->mr)
    {
        fprintf(stderr, "Couldn't register MR\n");
        return NULL;
    }
    return session->mr;
}


struct ibv_device** _get_devices(){
    struct ibv_device **device_list = ibv_get_device_list(NULL);

    if (!device_list)
    {
        perror("Failed to get IB devices list");
        return NULL;
    }

    return device_list;

}

/**
 * wait until we get completion of 'opcode_wait'
 * will print also problems that may raise during the send/get process.
 * @param session
 * @param opcode_wait
 * @return
 */
int _wait_for_completion(connectionContext *session, uint opcode_wait){
    struct ibv_wc wc[WC_BATCH];
    int ne, i;
    do
    {
        ne = ibv_poll_cq(session->cq, WC_BATCH, wc);
        if (ne < 0)
        {
            fprintf(stderr, "poll CQ failed %d\n", ne);
            return -1;
        }

    } while (ne < 1);

    //print status errors if there are any.
    for(i = 0; i < ne; i++){
        if(wc[i].status != IBV_WC_SUCCESS){
//            fprintf(stderr, "Error sending packet: %d\n", wc[i].status);
            fprintf(stderr, "Failed status %s (%d) for wr_id %d, with opcode %d\n",
                    ibv_wc_status_str(wc[i].status),
                    wc[i].status, (int) wc[i].wr_id,
                    opcode_wait);
        }
    }

    for(i=0; i < ne; i++)
    {
        if ((int) wc[i].wr_id == LOCAL_RECV_WRID)
        {
            if (--session->routs <= 10)
            {
                session->routs = get_post_receive(session);
                if (session->routs < 0)
                {
                    fprintf(stderr, "Couldn't post receive (%d)\n", session->routs);
                    return -1;
                }
            }
            if(opcode_wait == LOCAL_RECV_WRID)
                return 0;
        }

        if((int)wc[i].wr_id == REMOTE_READ && opcode_wait == REMOTE_READ)
            return 0;

        if((int)wc[i].wr_id == LOCAL_SEND_WRID && opcode_wait == LOCAL_SEND_WRID)
            return 0;
    }

    return 1; // all the completions we got was with send ID
}


// --------------------------------- public functions: ---------------------------------------//


arguments arguments_create(char *server_name){

    _arguments* args = (_arguments*)calloc(1, (sizeof *args) + 1);
    if(args == NULL) return NULL;

    args->tcp_port = 7777;
    args->ib_port = 1;
    args->size = (uint32_t)pow(2, MESSAGES_SIZE-1)+1;
    args->mtu = IBV_MTU_2048;
    args->rx_depth = 100; // number of receives to post at a time (each round)
    args->tx_depth = 100; // round size (every round add amount of rx_depth RR)
    args->server_name = server_name;
    args->streams_number = 1;

//    size_t name_size = strlen(server_name);
//    args->server_name = (char*)calloc(1, name_size+1);
//    strncpy(args->server_name, server_name, name_size);

    return (arguments)args;
}


connectionContext* context_create(uint streams_number){

    connectionContext* session = (connectionContext*)calloc(1, streams_number*(sizeof *session));
    if(session == NULL) return NULL;
    struct ibv_device** devices = _get_devices();
    if(devices == NULL){
        free(session);
        session = NULL;
        return NULL;
    }


    struct ibv_device* device = *devices;
    struct ibv_context* ctx = ibv_open_device(device);

    if(!ctx){
        ibv_free_device_list(devices);
        free(session);
        free(device);
        devices = NULL;
        device = NULL;
        session = NULL;
        fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(device));
        return NULL;
    }

    for(uint i = 0; i < streams_number; i++){
        (&session[i])->context = ctx;
    }

    ibv_free_device_list(devices);
    devices = NULL;
    device = NULL;
    return session;
}


int context_init(connectionContext *session, arguments *arguments){

    _arguments *args = (_arguments*)arguments;
    session->size = args->size;
    session->rx_depth = args->rx_depth;
    session->mtu = args->mtu;
    session->routs = 0;
    session->streams_number = args->streams_number;

    session->pd = ibv_alloc_pd(session->context);
    if (!session->pd)
    {
        fprintf(stderr, "Couldn't allocate PD\n");
        return -1;
    }

    session->mr = _new_memory_region(args->size, session, !args->server_name);
    if(session->mr == NULL){
        ibv_dealloc_pd(session->pd);
        session->pd = NULL;
        return -1;
    }

    int rx = args->rx_depth;
    int tx = args->tx_depth;

    session->cq = ibv_create_cq(session->context, rx + tx, NULL, NULL, 0);
    if (!session->cq)
    {
        ibv_dereg_mr(session->mr);
        session->mr = NULL;
        ibv_dealloc_pd(session->pd);
        session->pd = NULL;
        fprintf(stderr, "Couldn't create CQ\n");
        return -1;
    }


    {
        struct ibv_qp_init_attr attr = {
                .send_cq = session->cq,
                .recv_cq = session->cq,
                .cap     = {
                        .max_send_wr  = (uint32_t)args->tx_depth,
                        .max_recv_wr  = (uint32_t)args->rx_depth,
                        .max_send_sge = 1,
                        .max_recv_sge = 1
                },
                .qp_type = IBV_QPT_RC,
                .sq_sig_all = 0
        };

        //several queue pairs:

        session->qp = ibv_create_qp(session->pd, &attr);
        if (!session->qp)
        {
            ibv_destroy_cq(session->cq);
            session->cq = NULL;
            ibv_dereg_mr(session->mr);
            session->mr = NULL;
            ibv_dealloc_pd(session->pd);
            session->pd = NULL;
            fprintf(stderr, "Couldn't create QP\n");
            return -1;
        }
    }

    session->port_info = (struct ibv_port_attr*)calloc(1, sizeof(*session->port_info));
    if (ibv_query_port(session->context, args->ib_port, session->port_info))
    {
        ibv_destroy_qp(session->qp);
        session->qp = NULL;
        ibv_destroy_cq(session->cq);
        session->cq = NULL;
        ibv_dereg_mr(session->mr);
        session->mr = NULL;
        ibv_dealloc_pd(session->pd);
        session->pd = NULL;
        fprintf(stderr, "Couldn't get tcp_port info\n");
        return -1;
    }


    {
        struct ibv_qp_attr attr = {
                .qp_state        = IBV_QPS_INIT,
                .qp_access_flags = IBV_ACCESS_REMOTE_READ |
                                   IBV_ACCESS_REMOTE_WRITE,
                .pkey_index      = 0,
                .port_num        = args->ib_port
        };

        //make all the qp to stat INIT:
        if (ibv_modify_qp(session->qp, &attr,
                          IBV_QP_STATE |
                          IBV_QP_PKEY_INDEX |
                          IBV_QP_PORT |
                          IBV_QP_ACCESS_FLAGS))
        {
            free(session->port_info);
            session->port_info = NULL;
            ibv_destroy_qp(session->qp);
            session->qp = NULL;
            ibv_destroy_cq(session->cq);
            session->cq = NULL;
            ibv_dereg_mr(session->mr);
            session->mr = NULL;
            ibv_dealloc_pd(session->pd);
            session->pd = NULL;
            fprintf(stderr, "Failed to modify QP to INIT\n");
            return -1;
        }
    }


    return 0;
}


ibInformation* source_ib_information_create(connectionContext *session){
    ibInformation* my_dest = (ibInformation*)malloc(session->streams_number*sizeof(ibInformation));
    if(my_dest == NULL) return NULL;

    for(uint i = 0; i < session->streams_number; i++)
    {
        (&my_dest[i])->lid = (&session[i])->port_info->lid;
        if ((&session[i])->port_info->link_layer == IBV_LINK_LAYER_INFINIBAND && !(&my_dest[i])->lid)
        {
            fprintf(stderr, "Couldn't get local LID\n");
            free(my_dest);
            my_dest = NULL;
            return NULL;
        }

        (&my_dest[i])->qpn = (&session[i])->qp->qp_num;
        (&my_dest[i])->psn = (uint32_t) (lrand48() & 0xffffff);

        printf("local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06u\n",
               (&my_dest[i])->lid, (&my_dest[i])->qpn, (&my_dest[i])->psn);
    }

    return my_dest;
}


int get_post_receive(connectionContext *session)
{
    struct ibv_sge list = {
            .addr    = (uintptr_t) session->buffer,
            .length = (uint32_t)session->size,
            .lkey    = session->mr->lkey
    };
    struct ibv_recv_wr wr = {
            .wr_id      = LOCAL_RECV_WRID,
            .next       = NULL,
            .sg_list    = &list,
            .num_sge    = 1
    };
    struct ibv_recv_wr *bad_wr;

    int i;
    uint post_recv_counter = 0;

    for (i = 0; i < session->rx_depth-session->routs; ++i)
    {
        if (ibv_post_recv(session->qp, &wr, &bad_wr))
        {
            break;
        }
        else{
            post_recv_counter++;
        }
    }

    if(post_recv_counter < (unsigned)(session->rx_depth-session->routs)){
        fprintf(stderr, "Couldn't post receive (%d)\n", session->routs);
        return -1;
    }

    return post_recv_counter + session->routs;
}


int context_connect(connectionContext *session, arguments *args, uint32_t my_psn,
                    ibInformation *dest){
    _arguments *_args = (_arguments*)args;
    struct ibv_qp_attr attr = {
            .qp_state = IBV_QPS_RTR,
            .path_mtu = _args->mtu,
            .rq_psn = dest->psn,
            .dest_qp_num = dest->qpn,
            .ah_attr = {
                    .dlid = dest->lid,
                    .sl = 0,
                    .src_path_bits = 0,
                    .is_global = 0,
                    .port_num = _args->ib_port
            },
            .max_dest_rd_atomic = 1,
            .min_rnr_timer = 12
    };

    if (ibv_modify_qp(session->qp, &attr,
                      IBV_QP_STATE |
                      IBV_QP_AV |
                      IBV_QP_PATH_MTU |
                      IBV_QP_DEST_QPN |
                      IBV_QP_RQ_PSN |
                      IBV_QP_MAX_DEST_RD_ATOMIC |
                      IBV_QP_MIN_RNR_TIMER))
    {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }


    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = my_psn;
    attr.max_rd_atomic = 1;

    if (ibv_modify_qp(session->qp, &attr,
                      IBV_QP_STATE |
                      IBV_QP_TIMEOUT |
                      IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY |
                      IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC))
    {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }



    return 0;
}


int send_message(connectionContext *session, size_t size, enum ibv_wr_opcode opcode, void
        *local_ptr, void *remote_ptr, uint32_t remote_key){

    struct ibv_sge list = {
            .addr    = (uint64_t) (local_ptr ? local_ptr : session->buffer),
            .length = (uint32_t)size,
            .lkey    = session->mr->lkey
    };

    struct ibv_send_wr *bad_wr, wr = {
            .wr_id      = (remote_ptr ? REMOTE_READ : LOCAL_SEND_WRID),
            .next       = NULL,
            .sg_list    = &list,
            .num_sge    = 1,
            .opcode     = opcode,
            .send_flags = IBV_SEND_SIGNALED
    };

    if (remote_ptr) {
        wr.wr.rdma.remote_addr = (uintptr_t) remote_ptr;
        wr.wr.rdma.rkey = remote_key;
    }

    return ibv_post_send(session->qp, &wr, &bad_wr);
}


int wait_for_completion(connectionContext *session, uint opcode_wait){
    int msg_received;
    do{
        msg_received = _wait_for_completion(session, opcode_wait);
        if(msg_received == -1) return -1;
    }while(msg_received);

    return 0;
}


void arguments_destroy(arguments *args){
    _arguments* _args = (_arguments*)args;
//    if(_args->server_name){
//        free(_args->server_name);
//        _args->server_name = NULL;
//    }
    free(_args);
    _args = NULL;
}


int context_destroy(connectionContext *session, uint last_ses){
    for(uint i = 0; i < last_ses; i++)
    {
        free((&session[i])->port_info);
        (&session[i])->port_info = NULL;
        if (ibv_destroy_qp((&session[i])->qp)) return -1;
        (&session[i])->qp = NULL;
        if (ibv_destroy_cq((&session[i])->cq)) return -1;
        (&session[i])->cq = NULL;
        if (ibv_dereg_mr((&session[i])->mr)) return -1;
        (&session[i])->mr = NULL;
        free((&session[i])->buffer);
        (&session[i])->buffer = NULL;
        if (ibv_dealloc_pd((&session[i])->pd)) return -1;
        (&session[i])->pd = NULL;
    }

    if (ibv_close_device(session->context)) return -1;
    session->context = NULL;

    free(session);
    session = NULL;
    return 0;
 }


void ib_information_destroy(ibInformation* ib_info){
    free(ib_info);
    ib_info = NULL;
}

