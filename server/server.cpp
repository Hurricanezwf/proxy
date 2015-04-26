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

	//设置I/O复用
	struct timeval tm;
	tm.tv_sec = 0;
	tm.tv_usec = 0;

	fd_set rset;
	FD_ZERO( &rset );
	FD_SET(nListenFd, &rset);
	FD_SET( fileno(stdin), &rset );


	PID pid = fork();
	if( FAILED == pid )
	{
		LOG::LogErr("[Setup] create process failed! reason:%s", strerror(errno) );
		CloseFD( nListenFd );

		exit( FAILED );
	}else if( 0 == pid )
	{
		//TODO
		LOG::LogHint("child process running!");

		struct sockaddr_in peer_addr;
		socklen_t addr_len = sizeof(struct sockaddr_in);
		memset( &peer_addr, 0, addr_len );
		
		for(;;)
		{
			FD_SET( nListenFd, &rset);//每次都要重新set一下
			select( nListenFd+1, &rset, NULL, NULL, &tm);
			if( FD_ISSET(nListenFd, &rset) )
			{
				break;
			}
		}

		SOCK_FD nConFd = accept(nListenFd, (sockaddr*)&peer_addr, &addr_len );
		if(FAILED == nConFd)
		{
			LOG::LogErr("[Setup] error occured when accepting! reason:%s", strerror(errno));
			CloseFD( nListenFd );
			exit(FAILED);
		}
		LOG::LogHint("[Setup] server accept!");

		FD_SET( nConFd, &rset );
		FD_CLR( nListenFd, &rset );
		CloseFD( nListenFd );
		
		for(;;)
		{
			FD_SET( nConFd, &rset );
			select( nConFd+1, &rset, NULL, NULL, &tm);
			if( FD_ISSET(nConFd, &rset) )
			{
				break;
			}
		}
		s8 chReadBuf[256] = {0};
		read( nConFd, chReadBuf, 256 );
		printf("%s\n",chReadBuf);

		FD_CLR( nConFd, &rset );
		CloseFD( nConFd );
		exit(1);
	}

	sleep(100);

	s8 chInput[32] = {0};
	do{
		printf("Enter your command:");
		fflush(stdout);//防止上面这段打印没来得及输出就被阻塞了

		for(;;)
		{
			FD_SET( fileno(stdin), &rset );
			select(nListenFd+1, &rset, NULL, NULL, &tm);
			if( FD_ISSET(fileno(stdin), &rset) )
			{
				break;
			}
		}
		fgets(chInput, sizeof(chInput), stdin);

		if( 0 == strcmp(chInput, "quit\n") )
		{
			break;
		}else if( 0 == strcmp(chInput, "help\n") )
		{
			LOG::LogHint("********** help info ************");
			LOG::LogHint("quit: Enter 'quit' to quit!");
		}
		else if( 0 == strcmp(chInput, "\n") )
		{
			//do nothing
		}else{
			LOG::LogErr("unrecognize command! command=%s", chInput);
		}
	}while(true);
	
	return nListenFd;
}


//子进程终止处理
void ChildTerminate(int sig)
{
	LOG::LogHint("[ChildTerminate] recv SIGCHLD!");
	pid_t pid = -1;	
	while( 0 <= (pid = waitpid( -1, NULL, WNOHANG )) )
	{
		if( 0 < pid)
		{
			LOG::LogHint("[ChildTerminate] child:%d terminate!", pid);
		}
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
