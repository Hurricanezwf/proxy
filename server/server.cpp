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
    close(nListenFD);//关闭父进程的监听FD，让子进程监听吧

    usleep(10000);
    printf("Enter 'q' to quit!\n");
    s8 achInput[5] = "0";
    while (0 != strcmp(achInput, "q\n")) {
        fgets(achInput, sizeof(achInput), stdin);
    }

    ptShared->bAccepting = false;
    Destroy();

    LOG::LogHint("Nodify: server stop!");

    return 1;
}


SOCK_FD Setup()
{
    nListenFD = socket(AF_INET, SOCK_STREAM, 0); 
    CHK( (nListenFD != FAILED), "[Setup] create socket failed! reason:%s", NULL);

    //设置套接字选项
    s32 nSetVal = 1;
    s32 nSetRst = FAILED;
    nSetRst = setsockopt(nListenFD, SOL_SOCKET, SO_REUSEADDR, &nSetVal, sizeof(s32));
    CHK( (FAILED != nSetRst), "[Setup] set reuseaddr failed! reason:%s", Destroy);
    nSetRst = setsockopt(nListenFD, SOL_SOCKET, SO_REUSEPORT, &nSetVal, sizeof(s32));
    CHK( (FAILED != nSetRst), "[Setup] set reuseport failed! reason:%s", Destroy);
    nSetRst = setsockopt(nListenFD, SOL_SOCKET, SO_KEEPALIVE, &nSetVal, sizeof(s32));
    CHK( (FAILED != nSetRst), "[Setup] set keepalive failed! reason:%s", Destroy);

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
    //std::set<SOCK_FD> setConFDs;

    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    socklen_t tAddrLen = sizeof(struct sockaddr_in);

    while (ptShared->bAccepting) {
        FD_ZERO(&rfds);
        FD_SET(nListenFD, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        /**
         * 对accept加锁的解释: 
         * 内核维护已完成连接和未完成连接的队列，每次accept的时候,内核都从已完成
         * 连接的队列中取出一个返回。因此，无论有多少个进程在等待accept,内核中并
         * 不存在并发accept，所有进程都是共享内核中的accept操作，是需要相互竞争的。
         */
        SOCK_FD nReadyNum = select(nListenFD+1, &rfds, NULL, NULL, &tv);
        if (nReadyNum > 0) {
            if (FD_ISSET(nListenFD, &rfds)) {
                pthread_mutex_lock(&ptShared->mutex);
                SOCK_FD nConnFD = accept(nListenFD, (sockaddr*)&clientAddr, &tAddrLen);
                pthread_mutex_unlock(&ptShared->mutex);
                if (nConnFD == FAILED) {
                    LOG::LogErr("[ChildProcessHandle] process_%d accept connection failed! reason:%s", getpid(), strerror(errno));
                } else {
                    //accept成功时候，开启服务
                    LOG::LogHint("[ChildProcessHandle] process_%d says: %s:%d connected!", getpid(), inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
                }
            }
        }
        usleep(100000);
    }

    close(nListenFD);
    //CloseConnectedFDs(setConFDs);
}

//子进程终止处理
void ChildTerminate(int nSigNo)
{
    LOG::LogHint("[ChildTerminate] recv SIGCHLD!");
    pid_t pid = -1; 
    while( 0 < (pid = waitpid( -1, NULL, WNOHANG )) )
    {
        if( 0 < pid)
        {
            LOG::LogHint("[ChildTerminate] child:%d terminate!", pid);
        }
    }
}

void Destroy() {
    if (nListenFD > 0) {
        close(nListenFD);
    }
    
    pthread_mutex_destroy(&ptShared->mutex);
    munmap(ptShared, sizeof(TShared));
    shm_unlink(SHM_NAME);
}

//关闭所有的已连接套接字
void CloseConnectedFDs(std::set<SOCK_FD> &setFDs) {
    std::set<SOCK_FD>::iterator it = setFDs.begin();
    for (it; it != setFDs.end(); it++) {
        close(*it);
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
    close(shm_fd);
    if (ptShared == MAP_FAILED) {
        shm_unlink(SHM_NAME);
        return;
    }

    //init shared memory
    ptShared->bAccepting = TRUE;    
    pthread_mutex_init(&ptShared->mutex, NULL);
}

