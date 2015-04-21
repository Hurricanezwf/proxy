#include "server.h"

int main(int argc, char* argv[])
{
	SOCK_FD nListenFD = Setup();
	
	s8 chMsg[] = {"dabai"};
	s8 chFormat[] = {"hello,%s"};
	LOG::LogHint(chFormat, chMsg);
	LOG::LogWarn(chFormat, chMsg);
	LOG::LogErr(chFormat, chMsg);

	return 1;

}



SOCK_FD Setup()
{
	SOCK_FD nListenFd = -1;
	nListenFd = socket(AF_INET, SOCK_STREAM, 0); 
	if( FAILED == nListenFd )
	{
		LOG::LogErr("[Setup] create socket failed! reason:%s", strerror(errno) );
		CloseFD(nListenFd);
		
		exit(FAILED);
	}	

	struct sockaddr_in server_addr;
	memset( &server_addr, 0, sizeof(struct sockaddr_in) );
	server_addr.sin_family = AF_INET;
	server_addr.sin_port   = htons(LISTEN_PORT);
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

	if( FAILED == bind(nListenFd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) )
	{
		LOG::LogErr("[Setup] bind ip and port failed! reason:%s", strerror(errno) );
		CloseFD(nListenFd);

		exit(FAILED);
	}

	if( FAILED == listen(nListenFd, MAX_BACKLOG) )
	{
		LOG::LogErr("[Setup] listen failed! reason:%s", strerror(errno) );
		CloseFD(nListenFd);

		exit(FAILED);
	}

	LOG::LogHint("server listen on %s : %d", SERVER_IP, LISTEN_PORT);

	//安装捕获子进程终止信号
	if( SIG_ERR == signal(SIGCHLD, ChildTerminate) )
	{
		LOG::LogErr("[Setup] setup SIGCHLD failed!");
		CloseFD(nListenFd);

		exit(FAILED);
	}

	PID pid = fork();
	if( FAILED == pid )
	{
		LOG::LogErr("[Setup] create process failed! reason:%s", strerror(errno) );
		CloseFD( nListenFd );

		exit( FAILED );
	}else if( 0 == pid )
	{
		CloseFD( nListenFd );//关闭子进程中的监听描述符的copy	
	
		//TODO:
		LOG::LogHint("child process running!");

		exit(1);
	}


	//TODO:
	sleep(1000);
	s8 chInput[32] = {0};
	do{
		printf("Enter your command:");
		s8 chInput[32] = {0};
		fgets(chInput, sizeof(chInput), stdin);

		if( 0 == strcmp(chInput, "quit\n") )
		{
			break;
		}else if( 0 == strcmp(chInput, "debug\n") )
		{
			LOG::LogHint("you input debug!");
		}else{
			LOG::LogErr("unrecognize command! command=%s", chInput);
		}
	}while(true);
	
	pid_t child_pid = -1;	
	while( 0 == (child_pid = waitpid( -1, NULL, WNOHANG )) )
	{
		LOG::LogHint("[Setup] child_%d terminate!", child_pid);
	}

	CloseFD(nListenFd);
	
	return nListenFd;
}


//子进程终止处理
void ChildTerminate(int sig)
{
	pid_t pid = -1;
	
	while( 0 == (pid = waitpid( -1, NULL, WNOHANG )) )
	{
		LOG::LogHint("[ChildTerminate] child_%d terminate!", pid);
	}
}

BOOL CloseFD(SOCK_FD fd)
{
	if(-1 != fd)
	{
		close(fd);
	}

	return true;
}
