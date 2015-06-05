#include "server.h"
#include "base.h"
#include "log.h"
#include <fcntl.h>
#include <assert.h>
#include <signal.h>


#define     CONNECT_NUM     1

void Connect();
void ChildTerminateProcess(int signo);


int main(int argc, char* argv[])
{
    setbuf(stdout, NULL);
    signal(SIGCHLD, ChildTerminateProcess);

    for (int i = 0; i < CONNECT_NUM; i++) {
        if (0 == fork()) {
            Connect();
        }
        usleep(10000);
    }

    char input = 'a';
    while (input != 'q') {
        input = (char)getchar();
    }

	return 0;
}


void Connect()
{
    SOCK_FD nConFd = socket(AF_INET, SOCK_STREAM, 0);
	if(FAILED == nConFd)
	{
		LOG::LogErr("create socket failed! reason=%s", strerror(errno));
        exit(FAILED);
	}

    struct sockaddr_in con_addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	memset( &con_addr, 0, addr_len );

	con_addr.sin_family = AF_INET;
	con_addr.sin_port = htons(9000);
	con_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

	if( FAILED == connect(nConFd, (struct sockaddr*)&con_addr, addr_len) )
	{
		LOG::LogErr("connect failed! reason=%s", strerror(errno));
        close(nConFd);
        exit(FAILED);
    } else {
        //connect成功并不能代表服务器接收了你的连接，有可能没进行accept操作，
        //这个需要服务器发消息来告诉你，连接是否成功
        LOG::LogHint("%d connect success!", getpid());
    }

    sleep(10);
    close(nConFd);

    exit(0);
}

void ChildTerminateProcess(int signo)
{
    signal(SIGCHLD, ChildTerminateProcess);
    pid_t pid = -1;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0 ) {
        LOG::LogHint("%d terminate!", pid);
    }
}
