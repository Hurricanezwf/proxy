#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "log.h"

#define		SOCK_FD		s32

#define		SERVER_IP		"127.0.0.1"
#define		LISTEN_PORT		9000

#define 	MAX_BACKLOG		10






/* function declare */

SOCK_FD Setup();
BOOL CloseFD(SOCK_FD fd);
void ChildTerminate(int sig);

#endif	//SERVER_H_
