//
// Created by dani94 on 5/6/19.
//

#ifndef NETWORK_WORKSHOP_EX1_CLIENT_H
#define NETWORK_WORKSHOP_EX1_CLIENT_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "establishment.h"
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <getopt.h>
#include <arpa/inet.h>


int connect_to_server(arguments *arguments, pingpongContext* session);


ibInformation* destination_ib_info_create(arguments *args, ibInformation *ib_source);


int warm_up_cycles(pingpongContext *session, uint message_size, uint cycles);


time_measurement* time_measurement_create(pingpongContext *session, size_t mcycles);


void time_measurement_destroy(time_measurement* measures);


int pingpong_context_disconnect(pingpongContext* session);

int several_streams_connection(int argc, char *argv[], uint streams_number);

void* threaded_calculation(void* void_session);

int multi_threaded_connection(int argc, char* argv[], uint connections_number);

#endif //NETWORK_WORKSHOP_EX1_CLIENT_H
