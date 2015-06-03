#include "server.h"
#include "base.h"
#include "log.h"

int main(int argc, char* argv[])
{
	SOCK_FD nConFd = FAILED;

	nConFd = socket(AF_INET, SOCK_STREAM, 0);
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
		close( nConFd );
		exit(FAILED);
	}

	s8 chWriteBuf[256] = {0};
	printf("Enter:");
	fgets( chWriteBuf, 256, stdin );
	/*do{
		write(nConFd, chWriteBuf, strlen(chWriteBuf)+1);
		printf("Enter");
		memset(chWriteBuf, 0, 256);
		fgets( chWriteBuf, 256, stdin );
	}while( 0 == strcmp(chWriteBuf,"quit") );
	*/
	//write(nConFd, chWriteBuf,strlen(chWriteBuf)+1);

	close( nConFd );

	return 0;
}
