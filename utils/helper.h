//
// Created by dani94 on 5/23/19.
//

#ifndef NETWORK_WORKSHOP_EX1_HELPER_H
#define NETWORK_WORKSHOP_EX1_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "math.h"
#include "infiniband-connection/dkv_client.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

/**
 * print error to standard error and may exit
 * @param msg
 * @param exit_m
 */
void error(const char *msg, short exit_m);

/**
 * check convergence of two following results
 * @param last_average last result
 * @param current_average new result
 * @param percent the percent of the convergence
 * @return 1 if converged and 0 otherwise
 */
int check_convergence(double last_average, double current_average, double percent);

/**
 * calculate the current time given an timval structure
 * @param time struct timeval variable
 * @return the current time in microseconds
 */
long current_time_micro(struct timeval *time);

/**
 * calculate the current time given an timval structure
 * @param time struct timeval variable
 * @return the current time in seconds
 */
long current_time_seconds(struct timeval *time);


/**
 * build message to send to the server
 * @param msg is a string that start with the size of the message in bytes
 * @param msize the size of the message in bytes
 */
void build_msg(char* msg, size_t msize);

/**
 * subtract two times intervals and return the subtraction in microseconds
 * will return end_time - start_time
 * @param end_time
 * @param start_time
 * @return double number, time in microseconds
 */
double_t sub_timevals(struct timeval *end_time, struct timeval *start_time);


int recursive_fill_kv(char const* dir_name, void *dkv_h);

#endif //NETWORK_WORKSHOP_EX1_HELPER_H
