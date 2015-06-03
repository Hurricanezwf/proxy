#include "server.h"


static SOCK_FD nListenFD = FAILED;
static fd_set rfds;
static TShared *ptShared = NULL;


int main(int argc, char* argv[])
{
    setbuf(stdout, NULL);

    //create share memory
    CreateShareMemory();
    CHK( (ptShared != NULL), "[Setup] create share memory failed! reason:%s", NULL);

    Setup();

    //setup signal for catching child process terminate
    sighandler_t tSigRtn = signal(SIGCHLD, ChildTerminate);
    CHK( (tSigRtn != SIG_ERR), "[Setup] setup SIGCHLD failed! reason:%s", Destroy);

    //fork 2 child process to accept connect
    for (s8 chChildNum = 0; chChildNum < 2; chChildNum++) {
        if (0 == fork()) {
            ChildProcessHandle();
            exit(0);
        }
    }
    CloseFD(nListenFD);//关闭父进程的监听FD，让子进程监听吧

    usleep(10000);
    printf("Enter 'q' to quit!\n");
    s8 chInput = 'a';
    while (chInput != 'q') {
        chInput = (s8)getchar();
    }
    ptShared->bAccepting = false;

    Destroy();

    LOG::LogHint("Nodify: server will terminate within 3 sec!");
    sleep(3);

    return 1;

}


SOCK_FD Setup()
{
    nListenFD = socket(AF_INET, SOCK_STREAM, 0); 
    CHK( (nListenFD != FAILED), "[Setup] create socket failed! reason:%s", Destroy);

    struct sockaddr_in server_addr;
    memset( &server_addr, 0, sizeof(struct sockaddr_in) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(LISTEN_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    s32 nBindRtn = bind(nListenFD, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
    CHK( (nBindRtn != FAILED), "[Setup] bind ip and port failed! reason:%s", Destroy);

    s32 nListenRtn = listen(nListenFD, MAX_BACKLOG);
    CHK( (nListenRtn != FAILED), "[Setup] listen failed! reason:%s", Destroy);

    LOG::LogHint("server listen on %s : %d", SERVER_IP, LISTEN_PORT);


    return nListenFD;
}


// 此处子进程copy了父进程的nListenFD，有几个子进程就copy了几次，注意在子进程结束后释放
// 此处暂未实现accept的负载均衡，暂时先用锁
void ChildProcessHandle() {
    LOG::LogHint("[ChildProcessHandle] Child process %d is running!", getpid()); 
    usleep(10000);

    //create a set to save connectd FD
    //-- Quesion:如果客户端断链了，服务器如何删除set中的FD?  --//
    std::set<SOCK_FD> setConFDs;

    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    socklen_t tAddrLen = sizeof(struct sockaddr_in);

    while (ptShared->bAccepting) {
        FD_ZERO(&rfds);
        FD_SET(nListenFD, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        SOCK_FD nReadyNum = select(nListenFD+1, &rfds, NULL, NULL, &tv);
        if (nReadyNum > 0) {
            if (FD_ISSET(nListenFD, &rfds)) {
                SOCK_FD nConnFD = accept(nListenFD, (sockaddr*)&clientAddr, &tAddrLen);
                if (nConnFD == FAILED) {
                    LOG::LogErr("[ChildProcessHandle] process_%d accept connection failed! reason:%s", getpid(), strerror(errno));
                } else {
                    setConFDs.insert(nConnFD);
                    LOG::LogHint("[ChildProcessHandle] process_%d says: %s:%d connected!", getpid(), inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
                }
            }
        }
        usleep(100000);
    }

    CloseFD(nListenFD);
    CloseConnectedFDs(setConFDs);
}

//子进程终止处理
void ChildTerminate(int nSigNo)
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

void CloseFD(s32 fd) {
    if(-1 != fd) {
        close(fd);
    }
}


void Destroy() {
    if (nListenFD > 0) {
        CloseFD(nListenFD);
    }

    munmap(ptShared, sizeof(TShared));
    shm_unlink(SHM_NAME);
}

//关闭所有的已连接套接字
void CloseConnectedFDs(std::set<SOCK_FD> &setFDs) {
    std::set<SOCK_FD>::iterator it = setFDs.begin();
    for (it; it != setFDs.end(); it++) {
        CloseFD(*it);
    }

    setFDs.clear();
}

//返回值校验函数
void CHK(BOOL bSuccess, s8 *pchMsg, TResourceClear func) {
    if (!bSuccess) {
        LOG::LogErr(pchMsg, strerror(errno));
        if (NULL != func) {
            (*func)();
        }
        exit(FAILED);
    }
}

//创建共享内存区
void CreateShareMemory() {
    int shm_fd = FAILED;
    ptShared = NULL;

    //create share memory
    shm_unlink(SHM_NAME);
    shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (shm_fd == FAILED) {
        return;
    }
    ftruncate(shm_fd, sizeof(TShared));

    //do memory mapping
    ptShared = (TShared*)mmap(NULL, sizeof(TShared), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    CloseFD(shm_fd);
    if (ptShared == MAP_FAILED) {
        shm_unlink(SHM_NAME);
        return;
    }

    //init shared memory
    ptShared->bAccepting = TRUE;    
}

