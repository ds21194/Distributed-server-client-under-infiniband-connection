#include "nweb.h"

#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpg" }, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"ico", "image/ico" },  
	{"zip", "image/zip" },  
	{"gz",  "image/gz"  },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{0,0} };

void logger(int type, char *s1, char *s2, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];

	switch (type) {
        case ERROR: (void)sprintf(logbuffer, "ERROR: %s:%s Errno=%d exiting pid=%d", s1, s2, errno,
                getpid());
            break;
        case FORBIDDEN:
            (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
            (void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2);
            break;
        case NOTFOUND:
            (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
            (void)sprintf(logbuffer, "NOT FOUND: %s:%s", s1, s2);
            break;
        case LOG: (void)sprintf(logbuffer, " INFO: %s:%s:%d", s1, s2, socket_fd); break;
        default: break;
    }
	/* No checks here, nothing can be done with a failure anyway */
	if((fd = open("nweb.log", O_CREAT| O_WRONLY | O_APPEND, 0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer)); 
		(void)write(fd,"\n",1);      
		(void)close(fd);
	}
	if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit, void* dkv_h)
{
	int j, buflen;
//	int file_fd;
	long i, ret, len;
	char *fstr;
	static char buffer[BUFSIZE+1]; /* static so zero filled */

	ret = read(fd, buffer, BUFSIZE); /* read Web request in one go */
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		logger(FORBIDDEN,"failed to read browser request", "", fd);
		fprintf(stderr, "failed to read browser request\n");
	}

	if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
		buffer[ret]=0;		/* terminate the buffer */
	else buffer[0]=0;
	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	logger(LOG,"request", buffer, hit);
	if( strncmp(buffer,"GET ",4) != 0 && strncmp(buffer,"get ", 4) != 0 ) {
		logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
	}

	for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = 0;
			break;
		}
	}
    /* check for illegal parent directory use .. */
	for(j=0;j<i-1;j++)
    {
        if (buffer[j] == '.' && buffer[j + 1] == '.')
        {
            logger(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
        }
    }

    /* convert no filename to index file */
	if( !strncmp(&buffer[0],"GET /\0", 6) || !strncmp(&buffer[0],"get /\0", 6) )
		(void)strcpy(buffer,"GET /index.html");

	/* work out the file type and check we support it */
    buflen=(int)strlen(buffer);
	fstr = (char *)0;
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, (size_t)len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) logger(FORBIDDEN, "file extension type not supported", buffer, fd);

    /* open the file for reading: */
//	if(( file_fd = open(&buffer[5], O_RDONLY)) == -1) {
//		logger(NOTFOUND, "failed to open file", &buffer[5], fd);
//	}

	char *file_content;
	uint file_length;

    char buf[PATH_MAX]; /* PATH_MAX incudes the \0 so +1 is not required */
    char *res = realpath(&buffer[5], buf);
    printf("test_key to search is: %s\n", res);
    fflush(stdout);
	dkv_get(dkv_h, res, &file_content, &file_length);

	logger(LOG,"SEND",&buffer[5],hit);
//	len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
//	(void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */

    /* Header + a blank line: */
	(void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: "
                      "%d\nConnection: close\nContent-Type: %s\n\n", VERSION, file_length, fstr);
	logger(LOG,"Header", buffer, hit);
    size_t header_len = strlen(buffer);

    /* send HTTP header: */
    write(fd, buffer, header_len);

    /* send file content: */

//    ssize_t n = write(fd, file_content, file_length);
//    if(n < 0){
//        logger(ERROR, "HTTP", "Could not send html file",  hit);
//    }

    ssize_t n = 0;
    char *start = file_content;
    char *end = start + file_length;
    while (start < end) {
        n = write(fd, start, end - start);
        if(n < 0){
            logger(ERROR, "HTTP", "Could not send html file",  hit);
            return;
        }
        start += n;
	}

    /* allow socket to drain before signalling the socket is closed */
    dkv_release(file_content);

    sleep(1);
    close(fd);
	exit(1);
}

int open_dkv(char const* dirname, void** dkv_h){
    uint servers_num = 2;
    struct kv_server_address *servers = (struct kv_server_address*)calloc((servers_num+1), sizeof
    (*servers));
    for(uint i = 0; i < servers_num; i++){
        (&servers[i])->server_name = (char*)calloc(1, sizeof("xxx.xxx.xxx")+1);
    }

//    uint i = 0;
//    for(i = 0; i < servers_num; i++){
//        sprintf((&servers[i])->server_name, "10.164.164.10%d", i+2);
//        (&servers[i])->port = (short)(i+1);
//    }

    sprintf((&servers[0])->server_name, "10.164.164.10%d", 2);
    (&servers[0])->port = (short)(1);

    sprintf((&servers[1])->server_name, "10.164.164.10%d", 3);
    (&servers[1])->port = (short)(3);

    if(dkv_open(servers+1, servers, dkv_h)) return -1;

    printf("opened dkv servers successfully\ndir name: %s\n", dirname); //TODO: delete

    if(recursive_fill_kv(dirname, *dkv_h)) return -1;


    for(uint i = 0; i < servers_num; i++){
        free((&servers[i])->server_name);
    }
    free(servers);

    return 0;
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd, hit;
	socklen_t length;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */

	if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
		(void)printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
	"\tnweb is a small and very safe mini web server\n"
	"\tnweb only servers out file/web pages with extensions named below\n"
	"\t and only from the named directory or its sub-directories.\n"
	"\tThere is no fancy features = safe and secure.\n\n"
	"\tExample: nweb 8181 /home/nwebdir &\n\n"
	"\tOnly Supports:", VERSION);
		for(i=0;extensions[i].ext != 0;i++)
			(void)printf(" %s",extensions[i].ext);

		(void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
	"\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
	"\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n"  );
		exit(0);
	}

	if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
	    !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
	    !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
	    !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
		(void)printf("ERROR: Bad top directory %s, see nweb -?\n",argv[2]);
		exit(3);
	}

    printf("processing data\n"); //TODO: delete
    void *dkv_h = NULL;
    printf("dir name: %s\n", argv[2]); //TODO: delete
    if(open_dkv(argv[2], &dkv_h)) return -1;

    char* test_value;
    char* test_key = "/cs/usr/dani94/Desktop/networks-workshop-ex4/nwebdir/index.html";
    uint test_value_size;
    dkv_get(dkv_h, test_key, &test_value, &test_value_size);
    char *to_print = (char*)malloc(500);
    sprintf(to_print, "got from server:\n\t test_key: %s\n\t value: %s\n", test_key, test_value);
    logger(LOG, "jjj", to_print, getpid());
    free(to_print);
    to_print = NULL;

    if(chdir(argv[2]) == -1){
        (void)printf("ERROR: Can't Change to directory %s\n",argv[2]);
        exit(4);
    }

	/* Become deamon + unstopable and no zombies children (= no wait()) */
	if(fork() != 0)
		return 0; /* parent returns OK to shell */

	(void)signal(SIGCLD, SIG_IGN); /* ignore child death */
	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	for(i=0;i<32;i++)
    {
        close(i); /* close open files */
    }
	setpgrp();		/* break away from process group */
	logger(LOG,"nweb starting", argv[1], getpid());
	/* setup the network socket     */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) < 0)
		logger(ERROR, "system call","socket",0);
	port = atoi(argv[1]);
	if(port < 0 || port > 60000)
		logger(ERROR,"Invalid port number (try 1->60000)", argv[1], 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons((uint16_t)port);

//    dkv_get(dkv_h, test_key, &test_value, &test_value_size);
//    to_print = (char*)malloc(500);
//    sprintf(to_print, "got from server:\n\t test_key: %s\n\t value: %s\n", test_key, test_value);
//    logger(LOG, "", to_print, getpid());
//    free(to_print);
//    to_print = NULL;

	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
		logger(ERROR,"system call", "bind", 0);
	if( listen(listenfd,64) < 0)
		logger(ERROR,"system call","listen",0);

//    dkv_get(dkv_h, test_key, &test_value, &test_value_size);
//    to_print = (char*)malloc(500);
//    sprintf(to_print, "got from server:\n\t test_key: %s\n\t value: %s\n", test_key, test_value);
//    logger(LOG, "", to_print, getpid());
//    free(to_print);
//    to_print = NULL;

	for(hit=1; ;hit++) {
		length = sizeof(cli_addr);
        logger(LOG, "before accepting\n", "", hit);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			logger(ERROR,"system call","accept", 0);
		if((pid = fork()) < 0) {
			logger(ERROR,"system call", "fork", 0);
		}
		else {
			if(pid == 0) { 	/* child */
				(void)close(listenfd);

                logger(LOG, "", "taking care of web request\n", hit);
                dkv_get(dkv_h, test_key,
                        &test_value, &test_value_size);
                printf("got from server:\n\t test_key: %s\n\t value: %s\n", test_key, test_value);

				web(socketfd, hit, dkv_h); /* never returns */
			} else { 	/* parent */
				(void)close(socketfd);
                logger(LOG, "closing web request\n", "", hit);
                fflush(stdout);
			}
		}
	}
}
