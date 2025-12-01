#include <winsock.h>
#include <ws2tcpip.h>
//#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
//#include <sys/socket.h>
//#include <unistd.h>
#include <stdlib.h>
//#include <pthread.h>
//#include <windows.h>
#include <time.h>
#include "users.h"

#define SERVER_HOSTNAME "zos.ospreys.biz"
#define SERVER_PORT 50074

//this struct holds all the arguments required for sendRecieve
typedef struct{
    int port;
    char addr[30];
}netStruct;