//
// Created by dani94 on 7/24/19.
//

#ifndef NETWORK_WORKSHOP_EX1_NWEB_H
#define NETWORK_WORKSHOP_EX1_NWEB_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "infiniband-connection/dkv_client.h"

void logger(int type, char *s1, char *s2, int socket_fd);

void web(int fd, int hit, void* dkv_h);

int open_dkv(char const* dirname, void** dkv_h);

int main(int argc, char **argv);

#endif //NETWORK_WORKSHOP_EX1_NWEB_H
