#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log.h"

#define		SOCK_FD		s32

#define		SERVER_IP		"172.16.73.100"
#define		LISTEN_PORT		9000







/* function declare */

SOCK_FD Setup();
BOOL CloseFD(SOCK_FD fd);


#endif	//SERVER_H_
