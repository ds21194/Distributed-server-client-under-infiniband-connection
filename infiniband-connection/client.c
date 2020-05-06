//
// Created by dani94 on 5/6/19.
//

#include "client.h"

#define RESULT_FILE_NAME ("infiniband")

int connect_to_server(arguments *arguments, pingpongContext* session){
//    get the infiniband address of my NIC:
    ibInformation* my_dest;

    my_dest = source_ib_information_create(session);
    if(my_dest == NULL){
        return -1;
    }

//    get the infiniband address of the remote NIC:
    ibInformation* remote_dest = destination_ib_info_create(arguments, my_dest);
    if(remote_dest == NULL){
        ib_information_destroy(my_dest);
        return -1;
    }

    for(uint i = 0; i < session->streams_number; i++)
    {
        if (pingpong_context_connect(&session[i], arguments, (&my_dest[i])->psn, (&remote_dest[i])))
        {
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


ibInformation* destination_ib_info_create(arguments *args, ibInformation *ib_source_info){

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

    if (asprintf(&service, "%d", args->tcp_port) < 0) return NULL;

    n = getaddrinfo(args->server_name, service, &hints, &res);

    if (n < 0)
    {
        fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), args->server_name, args->tcp_port);
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
        fprintf(stderr, "Couldn't connect to %s:%d\n", args->server_name, args->tcp_port);
        return NULL;
    }

    ibInformation* remote_dest;
    remote_dest = (ibInformation*)calloc(1, args->streams_number*(sizeof *remote_dest));
    if (!remote_dest) return NULL;

    for(uint i = 0; i < args->streams_number; i++)
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


int create_pingpong_request(pingpongContext* session, uint message_size){
//    printf("Send message: %.*s with size of %u\n", message_size, (char *) session->buffer,
//            message_size);

    if(send_message(session, message_size))
    {
        fprintf(stderr, "unsuccessful send, message size is %d", message_size);
        return -1;
    }
    int msg_received;
    do{
        msg_received = wait_for_completion(session);
        if(msg_received == -1) return -1;
    }while(msg_received);

    sscanf(session->buffer, "%d", &message_size);
    return 0;

}


int warm_up_cycles(pingpongContext *session, uint message_size, uint cycles){
    build_msg((char*)session->buffer, message_size);
    for(uint i = 0; i < cycles; i++){
        if(create_pingpong_request(session, message_size)) return -1;
    }
    return 0;
}


time_measurement* time_measurement_create(pingpongContext *session, size_t mcycles){
    double time_passed;
    double_t average_latency = 0, last_average;
    double_t throughput, packet_rate;
    uint packet_number;

    uint iterations_each_message = (uint)mcycles;
    uint message_size;
    size_t cycles = MESSAGES_SIZE;

    struct timeval time_start, time_end;

    time_measurement* measures = (time_measurement*)malloc(cycles*sizeof
            (time_measurement));

    for(uint i = 0; i < cycles; i++){
        message_size = (uint)pow(2, i);
        build_msg((char*)session->buffer, message_size);
        last_average = 0;
        average_latency = 0;

        //calculate average latency until convergence:
        while(!check_convergence(last_average, average_latency, 0.01)){
            last_average = average_latency;
            //measure time:
            current_time_micro(&time_start);
            for(uint j = 0; j < iterations_each_message; j++) {
                if(create_pingpong_request(session, message_size)){
                    fprintf(stderr, "could not send message with size %d - warm up", message_size);
                    return NULL;
                }
            }
            current_time_micro(&time_end);
            time_passed = sub_timevals(&time_end, &time_start);
            average_latency = (time_passed)/(iterations_each_message*2);
        }
        throughput = ((message_size*8)/average_latency)/1000;
        packet_number = (message_size/session->mtu+1)*iterations_each_message;
        packet_rate = packet_number/average_latency;

        measures[i].message_size = message_size;
        measures[i].latency = average_latency;
        measures[i].throughput = throughput;
        measures[i].packet_rate = packet_rate;
    }

    return measures;
}


void time_measurement_destroy(time_measurement* measures){
    free(measures);
    measures = NULL;
}


int pingpong_context_disconnect(pingpongContext* session){
    build_msg((char*)session->buffer, 1);
    ((char*)session->buffer)[0] = 'b';

    if(send_message(session, 5)){
        fprintf(stderr, "could not end connection");
        return -1;
    }

    return 0;
}


int several_streams_connection(int argc, char *argv[], uint streams_number){
    arguments* arguments = pingpong_arguments_create(argc, argv, streams_number);
    if(arguments == NULL){ return -1; }

    pingpongContext* session = pingpong_context_create(arguments->streams_number);
    if(session == NULL){
        pingpong_arguments_destroy(arguments);
        return -1;
    }

    for(uint i = 0; i < arguments->streams_number; i++)
    {
        if (pingpong_context_init(&session[i], arguments)){
            pingpong_context_destroy(session, i+1);
            pingpong_arguments_destroy(arguments);
            return -1;
        }

        //    add post receive request, "fill up the gas":
        (&session[i])->routs = get_post_receive(&session[i]);
        if((&session[i])->routs == -1){
            pingpong_context_destroy(session, i+1);
            pingpong_arguments_destroy(arguments);
            return -1;
        }
    }

    if(connect_to_server(arguments, session)){
        printf("failed\n");
        pingpong_context_destroy(session, arguments->streams_number);
        pingpong_arguments_destroy(arguments);
        session=NULL;
        arguments=NULL;
        return -1;
    }

    char* title = NULL;
    asprintf(&title, "single thread with %d streams", session->streams_number);
    printf("calculating measurements for %s\n", title);

    time_measurement* final_measures = (time_measurement*)calloc(1, MESSAGES_SIZE*sizeof(time_measurement));
    for(uint i = 0; i < arguments->streams_number; i++)
    {
        if(warm_up_cycles(&session[i], 5, 10)){
            free(title);
            free(final_measures);
            pingpong_context_destroy(session, arguments->streams_number);
            pingpong_arguments_destroy(arguments);
            return -1;
        }

        time_measurement *measures = time_measurement_create(&session[i], MEASURE_SIZE);
        if(measures == NULL){
            free(title);
            free(final_measures);
            pingpong_context_destroy(session, arguments->streams_number);
            pingpong_arguments_destroy(arguments);
            return -1;
        }

        for(uint j = 0; j < MESSAGES_SIZE; j++){
            (final_measures+j)->latency += (measures+j)->latency;
            (final_measures+j)->throughput += (measures+j)->throughput;
            (final_measures+j)->packet_rate += (measures+j)->packet_rate;
            if(i == 0) (final_measures+j)->message_size = (measures+j)->message_size;
        }
        time_measurement_destroy(measures);
        pingpong_context_disconnect(&session[i]);
    }

    for(uint j = 0; j < MESSAGES_SIZE; j++){
        (final_measures+j)->latency /= session->streams_number;
        (final_measures+j)->throughput /= session->streams_number;
        (final_measures+j)->packet_rate /= session->streams_number;

    }

    measures_to_csv(final_measures, MESSAGES_SIZE, RESULT_FILE_NAME, title);
    free(final_measures);
    free(title);
    title = NULL;
    pingpong_context_destroy(session, arguments->streams_number);
    pingpong_arguments_destroy(arguments);
    return 0;
}


void* threaded_calculation(void* void_session){
    pingpongContext *session = (pingpongContext*)void_session;

    warm_up_cycles(session, 5, 10);

    size_t cycles = (MEASURE_SIZE/session->streams_number+1)*session->streams_number;
    time_measurement *measures = time_measurement_create(session, cycles);
    pingpong_context_disconnect(session);
    pthread_exit(measures);
}


int multi_threaded_connection(int argc, char* argv[], uint connections_number){
    arguments* arguments = pingpong_arguments_create(argc, argv, connections_number);
    if(arguments == NULL){ return -1; }

    pingpongContext* session = pingpong_context_create(arguments->streams_number);
    if(session == NULL){
        pingpong_arguments_destroy(arguments);
        return -1;
    }

    for(uint i = 0; i < arguments->streams_number; i++)
    {
        if (pingpong_context_init(&session[i], arguments)){
            pingpong_context_destroy(session, i+1);
            pingpong_arguments_destroy(arguments);
            return -1;
        }

        //    add post receive request, "fill up the gas":
        (&session[i])->routs = get_post_receive(&session[i]);
        if((&session[i])->routs == -1){
            pingpong_context_destroy(session, i+1);
            pingpong_arguments_destroy(arguments);
            return -1;
        }
    }

    if(connect_to_server(arguments, session)){
        printf("failed\n");
        pingpong_context_destroy(session, arguments->streams_number);
        pingpong_arguments_destroy(arguments);
        session=NULL;
        arguments=NULL;
        return -1;
    }


    char* title = NULL;
    asprintf(&title, "multi threaded with %d threads", session->streams_number);
    printf("calculating measurements for %s\n", title);

    //create threads:
    uint threads_number = arguments->streams_number;
    pthread_t *threads_id = (pthread_t*)malloc(threads_number*sizeof(*threads_id));
    for(uint i = 0; i < session->streams_number; i++){
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        if(pthread_create(&threads_id[i], &attr, threaded_calculation, (void*)(&session[i])))
        {
            fprintf(stderr, "could not create thread %d\n", i);
            pingpong_context_destroy(session, threads_number);
            pingpong_arguments_destroy(arguments);
            free(threads_id);
            free(title);
            title = NULL;
            return -1;
        }
    }

    //create measurements result:
    time_measurement* final_measures = (time_measurement*)calloc(1, MESSAGES_SIZE*sizeof(time_measurement));
    time_measurement* res_measures = (time_measurement*)calloc
            (1, threads_number*MESSAGES_SIZE*sizeof(time_measurement));
    time_measurement *ret_val = NULL;
    size_t curr_thread = 0;
    //join threads:
    for(uint i = 0; i < threads_number; i++){
        curr_thread = MESSAGES_SIZE*i;
        if(pthread_join(threads_id[i], (void**)&ret_val)){
            free(final_measures);
            final_measures = NULL;
            free(res_measures);
            res_measures = NULL;
            pingpong_context_destroy(session, threads_number);
            pingpong_arguments_destroy(arguments);
            free(threads_id);
            threads_id = NULL;
            free(title);
            title = NULL;
            fprintf(stderr, "joining thread %d failed\n", i);
            return -1;
        }
        printf("thread %d has been joined\n", i+1);
        memcpy(res_measures + curr_thread, ret_val,
               MESSAGES_SIZE * sizeof(time_measurement));
        free(ret_val);
        ret_val = NULL;
    }

    //sum up the result and take average:
    for(uint i = 0; i < MESSAGES_SIZE; ++i){
        for(uint j = 0; j < threads_number; j++){
            curr_thread = j*MESSAGES_SIZE;
            (final_measures+i)->latency += (res_measures+curr_thread+i)->latency;
            (final_measures+i)->throughput += (res_measures+curr_thread+i)->throughput;
            (final_measures+i)->packet_rate += (res_measures+curr_thread+i)->packet_rate;
        }
        (final_measures+i)->message_size = (res_measures+i)->message_size;
        (final_measures+i)->latency /= threads_number;
        (final_measures+i)->throughput /= threads_number;
        (final_measures+i)->packet_rate /= threads_number;
    }

    measures_to_csv(final_measures, MESSAGES_SIZE, RESULT_FILE_NAME, title);

    free(title);
    free(threads_id);
    free(res_measures);
    free(final_measures);
    title = NULL;
    threads_id = NULL;
    res_measures = NULL;
    final_measures = NULL;
    pingpong_context_destroy(session, arguments->streams_number);
    pingpong_arguments_destroy(arguments);
    return 0;
}


int main(int argc, char* argv[]){
    //one thread with several streams measurement:
    for(uint i = 1; i < 11; i++)
    {
        several_streams_connection(argc, argv, i);
        sleep(1);
    }
    //multi-threaded measurement:
    for(uint i = 1; i < 11; i++)
    {
        multi_threaded_connection(argc, argv, i);
        sleep(1);
    }

    return 0;
}

