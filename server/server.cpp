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

    //fork  child process to accept connect
    for (u8 byIndex = 0; byIndex < MAX_CHILD_NUM; byIndex++) {
        if (0 == fork()) {
            InitChild(byIndex);
            ChildProcessHandle();
            exit(0);
        }
    }
    close(nListenFD);//关闭父进程的监听FD，让子进程监听吧

    usleep(10000);
   
    s8 achInput[10] = "0";
    while (TRUE) {
        printf("Enter:");
        fgets(achInput, sizeof(achInput), stdin);
        
        if (0 == strcmp(achInput, "q\n") ) {
            break;
        } else if (0 == strcmp(achInput, "sc\n")) {
            ShowChildConnInfo();
        } else if (0 == strcmp(achInput, "help\n")) {
            ShowHelp();
        } else {
            LOG::LogErr("unknown command!");
        }
    }

    //ptShared->bAccepting = false; //用于非阻塞式select
    for (u8 byIndex = 0; byIndex < MAX_CHILD_NUM; byIndex++) {
        kill(ptShared->infos[byIndex].pid, SIGKILL);
        waitpid(ptShared->infos[byIndex].pid, NULL, 0);
    } 

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
    nSetRst = setsockopt(nListenFD, SOL_SOCKET, SO_REUSEADDR, &nSetVal, sizeof(s32));   //ip地址复用
    CHK( (FAILED != nSetRst), "[Setup] set reuseaddr failed! reason:%s", Destroy);
    nSetRst = setsockopt(nListenFD, SOL_SOCKET, SO_REUSEPORT, &nSetVal, sizeof(s32));   //端口复用
    CHK( (FAILED != nSetRst), "[Setup] set reuseport failed! reason:%s", Destroy);
    nSetRst = setsockopt(nListenFD, SOL_SOCKET, SO_KEEPALIVE, &nSetVal, sizeof(s32));   //保活
    CHK( (FAILED != nSetRst), "[Setup] set keepalive failed! reason:%s", Destroy);
    
    //设置套接字为非阻塞
    nSetRst = fcntl(nListenFD, F_SETFL, O_NONBLOCK | fcntl(nListenFD, F_GETFL));
    CHK( (FAILED != nSetRst), "[Setup] set sockfd NONBLOCK failed! reason:%s", Destroy);

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
// 此处暂未实现accept的负载均衡
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

        s32 nMaxFD = nListenFD;

        /**
         *   此处采用了阻塞式select，当nListenFD准备好后，所有子进程中的select
         * 都会被唤醒[惊群效应]，然后通过FD_ISSET判断是哪个描述符就绪了，FD_ISSET
         * 检验到特定的描述符就绪后,会将在相应的fd_set中将就绪的描述符进行FD_CLR,
         * 从每次轮询的描述符集中踢除。这就是为什么对于同一个描述符，只要有一个进程
         * 通过了FD_ISSET，其他的进程都通不过；这也是为什么，我们每次循环都需要重新
         * FD_SET相应的文件描述符。
         */
        s32 nReadyNum = select(nMaxFD+1, &rfds, NULL, NULL, NULL);
        if (nReadyNum > 0) {
            if (FD_ISSET(nListenFD, &rfds)) {
                SOCK_FD nConnFD = accept(nListenFD, (sockaddr*)&clientAddr, &tAddrLen);
                if (nConnFD == FAILED) {
                    printf("failed!\n");
                    LOG::LogErr("[ChildProcessHandle] process_%d accept connection failed! reason:%s", getpid(), strerror(errno));
                } else {
                    //accept成功时候，开启服务
                    LOG::LogHint("[ChildProcessHandle] process_%d says: %s:%d connected!", getpid(), inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
                }
            }
        }
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
        LOG::LogHint("[ChildTerminate] child:%d terminate!", pid);
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
    //ptShared->pids.clear();
}

//初始化子进程
void InitChild(u8 byIndex) {
    //初始化进程连接信息
    ptShared->infos[byIndex].pid = getpid();
    ptShared->infos[byIndex].nMaxConnNum = MAX_BACKLOG / MAX_CHILD_NUM;
    ptShared->infos[byIndex].nLeftConnNum = 0;
}


//显示所有子进程的连接信息
void ShowChildConnInfo() {
    LOG::LogHint("---------- Child Process Connection Info ------------");
    for (u8 byIndex = 0; byIndex < MAX_CHILD_NUM; byIndex++) {
        LOG::LogHint("pid=%d, nMaxConnNum=%d, nLeftConnNum=%d", 
                ptShared->infos[byIndex].pid,
                ptShared->infos[byIndex].nMaxConnNum,
                ptShared->infos[byIndex].nLeftConnNum);
    }
    LOG::LogHint("-----------------------------------------------------");
}


//显示调试命令的帮助信息
void ShowHelp() {
    LOG::LogHint("------------ Debug Help ----------");
    LOG::LogHint("\'q\'   -- quit!");
    LOG::LogHint("\"sc\"  -- Show Child Process Connection Info!");
    LOG::LogHint("---------------------------------");
}
