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

	PID pid = fork();
	if( FAILED == pid )
	{
		
	}
	if( 0 == pid )//child process
	{
		
	}
	
	
	return nListenFd;
}

BOOL CloseFD(SOCK_FD fd)
{
	if(-1 != fd)
	{
		close(fd);
	}

	return true;
}
