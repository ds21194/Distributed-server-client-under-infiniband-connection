//
// Created by dani94 on 5/6/19.
//

#ifndef NETWORK_WORKSHOP_EX1_SERVER_H
#define NETWORK_WORKSHOP_EX1_SERVER_H


#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "establishment.h"
#include "utils/memoryPool.h"
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <getopt.h>
#include <arpa/inet.h>

/**
 * run the server
 * @return 0 upon success and -1 upon failure
 */
int server_runner();

#endif //NETWORK_WORKSHOP_EX1_SERVER_H
