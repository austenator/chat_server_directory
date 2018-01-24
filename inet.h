#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERV_TCP_PORT 12148
#define SERV_HOST_ADDR "127.0.0.1"

// The port the client will connect to on the directory server.
#define DIRECTORY_CLIENT_PORT 12151

// The port any new servers will connect to on the directory server.
#define DIRECTORY_SERVER_PORT 25678

// Max number of clients for each chat room.
#define MAX_CLIENTS 10

// Max buffer size. 
#define MAX 100

// Error handler.
void my_error(char * error_message){
    perror(error_message);
    exit(1);
}

// Strips a newline character off a string.
void strip_newline(char *s){
    while(*s != '\0'){
        if(*s == '\r' || *s == '\n'){
            *s = '\0';
        }
        s++;
    }
}
