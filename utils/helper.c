//
// Created by dani94 on 5/23/19.
//

#include "helper.h"


void build_msg(char* msg, size_t msize){
    if(msize < 1) msize = 1;

    memset(msg, 0, strlen(msg));
    for(size_t i = 0; i < msize; ++i){
        msg[i] = 'a';
    }
    //add the size of the message to the string
    char* snumber = (char*)malloc(sizeof(unsigned long));
    sprintf(snumber, "%zd", msize);
    size_t digit_number = strlen(snumber);
    strncpy(msg, snumber, digit_number);
    free(snumber);
}

void error(const char *msg, short exit_m)
{
    perror(msg);
    if(exit_m) exit(1);
}

int check_convergence(double last_average, double current_average, double percent){
    double changed_percent = last_average*percent;
    if(fabs(current_average-last_average) < changed_percent){
        return 1;
    }
    return 0;
}

double_t sub_timevals(struct timeval *end_time, struct timeval *start_time){
    struct timeval result;
    timersub(end_time, start_time, &result);
    return result.tv_sec*1000000+result.tv_usec;
}

long current_time_seconds(struct timeval *time){
    gettimeofday(time, NULL);
    long cur_time = time->tv_sec;
    return cur_time;
}

long current_time_micro(struct timeval *time){
    gettimeofday(time, NULL);
    long cur_time = time->tv_usec;
    return cur_time;
}

char* get_full_path(const char* rel_dir){
    char* full_path;
    size_t dir_rel_size = strlen(rel_dir);

    if(strstr(rel_dir, "/cs/") != NULL){
        full_path = (char*)calloc(1, dir_rel_size);
        strncpy(full_path, rel_dir, dir_rel_size);
        return full_path;
    }

    char buf[PATH_MAX]; /* PATH_MAX incudes the \0 so +1 is not required */
    char* curr_dir = getcwd(buf, PATH_MAX);
    if(curr_dir == NULL){
        fprintf(stderr, "got error %d: %s", errno, strerror(errno));
        return NULL;
    }
    size_t curr_dir_size = strlen(curr_dir);


    if(rel_dir[0] == '.'){
        rel_dir = rel_dir + 1;
    }

    if(rel_dir[0] != '/'){
        full_path = calloc(1, curr_dir_size + dir_rel_size + 1);
        strncpy(full_path, curr_dir, curr_dir_size);
        if(rel_dir[dir_rel_size-1] == '/')
            strncpy(full_path+curr_dir_size, rel_dir, dir_rel_size-1);
        else
            strncpy(full_path+curr_dir_size, rel_dir, dir_rel_size);
        free(curr_dir);
        curr_dir = NULL;
        return full_path;
    }

    full_path = calloc(1, curr_dir_size + dir_rel_size + 2);
    strncpy(full_path, curr_dir, curr_dir_size);
    strcat(full_path, "/");
    if(rel_dir[dir_rel_size-1] == '/')
        strncpy(full_path+curr_dir_size, rel_dir, dir_rel_size-1);
    else
        strncpy(full_path+curr_dir_size, rel_dir, dir_rel_size);

    free(curr_dir);
    curr_dir = NULL;
    return full_path;

}

int recursive_fill_kv(char const* dir_name, void *dkv_h) {
    char buf[PATH_MAX]; /* PATH_MAX incudes the \0 so +1 is not required */
    char* full_dir_name = realpath(dir_name, buf);

//    char *full_dir_name = get_full_path(dir_name);

    if(!full_dir_name){
        printf("directory names is: \"%s\"\n", dir_name);
        fprintf(stderr, "could not resolve directory name to full path\n, exit with error: %d, "
                        "error: %s", errno, strerror(errno));
        return -1;
    }

    struct dirent *curr_ent;
    DIR* dirp = opendir(full_dir_name);
    printf("directory names is: %s\n", full_dir_name); //TODO: delete
    if (dirp == NULL) {
        fprintf(stderr, "directory name is not good\n"); // TODO: delete
        return -1;
    }
    else{
        fprintf(stderr, "opened dir successfully\n"); //TODO: delete
    }
    while ((curr_ent = readdir(dirp)) != NULL) {
        if (!((strcmp(curr_ent->d_name, ".") == 0) || (strcmp(curr_ent->d_name, "..") == 0))) {
            char* path = malloc(strlen(full_dir_name) + strlen(curr_ent->d_name) + 2);
            strcpy(path, full_dir_name);
            strcat(path, "/");
            strcat(path, curr_ent->d_name);
            if (curr_ent->d_type == DT_DIR) {
                recursive_fill_kv(path, dkv_h);
            } else if (curr_ent->d_type == DT_REG) {
                int fd = open(path, O_RDONLY);
                size_t fsize = (size_t)lseek(fd, (size_t)0, SEEK_END);
                void *p = mmap(0, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
                printf("page is: \n %s\n", (char*)p); // TODO: delete
                if(dkv_set(dkv_h, path, p)){
                    fprintf(stderr, "problem with dkv_set\n");
                    return -1;
                };
                munmap(p, fsize);
                close(fd);
            }
            free(path);
        }
    }
    closedir(dirp);
//    free(full_dir_name);
    return 0;
}