#include "server.h"


static SOCK_FD nListenFD = FAILED;
static fd_set rfds;
static TShared *ptShared = NULL;

int main(int argc, char* argv[])
{
    server_init();

    //fork  child process to accept connect
    for (u8 byIndex = 0; byIndex < MAX_CHILD_NUM; byIndex++) {
        if (0 == fork()) {
            InitChild(byIndex);
            ChildProcessHandle(byIndex);
            exit(0);
        }
        usleep(1000);
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

    ptShared->bAccepting = false; //用于非阻塞式轮询退出
    /*for (u8 byIndex = 0; byIndex < MAX_CHILD_NUM; byIndex++) {
        kill(ptShared->infos[byIndex].pid, SIGKILL);
        waitpid(ptShared->infos[byIndex].pid, NULL, 0);
    }*/

    Destroy();
    LOG::LogHint("Nodify: server stop!");

    return 1;
}

void server_init() {
    //设置缓冲区无缓存
    setbuf(stdout, NULL);

    //create share memory
    CreateShareMemory();
    CHK( (ptShared != NULL), "[Setup] create share memory failed! reason:%s", NULL);

    Setup();

    //setup signal for catching child process terminate
    sighandler_t tSigRtn = signal(SIGCHLD, ChildTerminate);
    CHK( (tSigRtn != SIG_ERR), "[Setup] setup SIGCHLD failed! reason:%s", Destroy);
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
void ChildProcessHandle(u8 byIndex) {
    u8 byProIdx = byIndex;      //该进程在TShared中的索引
    LOG::LogHint("[ChildProcessHandle] Child process %d is running! nIndex=%d", getpid(), byProIdx); 

    //客户端列表
    std::list<SOCK_FD> clientList;

    s32 nEpollHandle = epoll_create(MAX_EPOLL_SIZE);
    CHK2( (FAILED != nEpollHandle), "[ChildProcessHandle] create epoll failed! reason:%s", ChildGabageClear, NULL);

    struct epoll_event ev, events[MAX_EPOLL_SIZE];
    s32 nRegRst = FAILED;
    //ev.events = EPOLLIN | EPOLLET;
    //ev.data.fd = nListenFD;

    //注册对监听套接字的监测
    //nRegRst = epoll_ctl(nEpollHandle, EPOLL_CTL_ADD, nListenFD, &ev);
    //CHK2( (FAILED != nRegRst), "[ChildProcessHandle] epoll regists listenFD faild! reason:%s", ChildGabageClear, (void*)&clientList);

    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    socklen_t tAddrLen = sizeof(struct sockaddr_in);

    while (ptShared->bAccepting) {
        if ((getpid() == FindMostIdleChild()) && (0 == pthread_mutex_trylock(&ptShared->tAcceptMutex.accept_mutex))) {
            ptShared->tAcceptMutex.mutex_owner = getpid();

            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = nListenFD;

            //注册对监听套接字的监测
            nRegRst = epoll_ctl(nEpollHandle, EPOLL_CTL_ADD, nListenFD, &ev);
            CHK2( (FAILED != nRegRst), "[ChildProcessHandle] epoll regists listenFD faild! reason:%s", ChildGabageClear, (void*)&clientList);
        }

        s32 nEventCount = epoll_wait(nEpollHandle, events, MAX_EPOLL_SIZE, 0);
        CHK2( (FAILED != nEventCount), "[ChildProcessHandle] epoll_wait failed! reason:%s", ChildGabageClear, (void*)&clientList);

        for (s32 nIndex = 0; nIndex < nEventCount; nIndex++) {
            if (events[nIndex].data.fd == nListenFD) {
                //移除监听注册
                epoll_ctl(nEpollHandle, EPOLL_CTL_DEL, nListenFD, &ev);
                pthread_mutex_unlock(&ptShared->tAcceptMutex.accept_mutex);                

                //监测到连接请求到来
                SOCK_FD nClientFD;
                
                while ((ptShared->infos[byProIdx].nLeftConnNum > 0)
                        && (nClientFD = accept(nListenFD, (struct sockaddr*)&clientAddr, &tAddrLen)) != FAILED )
                {
                //CHK2( (FAILED != nClientFD), "[ChildProcessHandle] server accept failed! reason:%s", ChildGabageClear, (void*)&clientList);

                clientList.push_back(nClientFD);
                ptShared->infos[byProIdx].nLeftConnNum--;

                //设置非阻塞
                fcntl(nClientFD, F_SETFL, O_NONBLOCK | fcntl(nClientFD, F_GETFL));

                //注册进epoll
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = nClientFD;
                nRegRst = epoll_ctl(nEpollHandle, EPOLL_CTL_ADD, nClientFD, &ev);
                CHK2( (FAILED != nRegRst), "[ChildProcessHandle] epoll regists ClientFD failed! reason:%s", ChildGabageClear, (void*)&clientList);

                char wBuf[10] = "success!";
                write(nClientFD, wBuf, sizeof(wBuf));

                LOG::LogHint("[ChildProcessHandle] process_%d says: %s:%d connect success!",
                        getpid(), inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
                }
            } else {
               //do logic handle

               char rBuf[128] = "0";
               s32 nSize = read(events[nIndex].data.fd, rBuf, sizeof(rBuf));
               if (nSize == 0) {
                   ptShared->infos[byProIdx].nLeftConnNum++;
               } 

               LOG::LogHint("[ChildProcessHandle] process_%d says: %s:%d send some datas to me! content:%d",
                        getpid(), inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port, nSize);
            }
        }
    }

    ChildGabageClear((void*)&clientList);
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
    
    pthread_mutex_destroy(&ptShared->tAcceptMutex.accept_mutex);
    munmap(ptShared, sizeof(TShared));
    shm_unlink(SHM_NAME);
}

//子进程资源清理
void ChildGabageClear(void *pList) {
    //关闭监听
    if (nListenFD > 0) {
        close(nListenFD);
    }
   
    //关闭所有连接
    if (NULL != pList) {
        std::list<SOCK_FD> *pClientList = (std::list<SOCK_FD>*)pList;
        CloseConnectedFDs(pClientList);
    }

    //释放该进程加的互斥锁
    if (ptShared->tAcceptMutex.mutex_owner == getpid()) {
        pthread_mutex_unlock(&ptShared->tAcceptMutex.accept_mutex);
    }

    //清理保存的负载信息
    for (u8 byIdx = 0; byIdx < MAX_CHILD_NUM; byIdx++) {
        if (ptShared->infos[byIdx].pid == getpid()) {
            ptShared->infos[byIdx].pid = -1;
            ptShared->infos[byIdx].nMaxConnNum = -1;
            ptShared->infos[byIdx].nLeftConnNum = -1;
        }
    }
}

//关闭所有的已连接套接字
void CloseConnectedFDs(std::list<SOCK_FD> *pclientList) {
    if (NULL != pclientList) {
        std::list<SOCK_FD>::iterator it = pclientList->begin();
        for (it; it != pclientList->end(); it++) {
            if (*it > 0) {
                close(*it);
            }
        }

        pclientList->clear();
    }
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

//返回值校验函数
void CHK2(BOOL bSuccess, s8 *pchMsg, void (*func)(void*), void *param) {
   if (!bSuccess) {
       LOG::LogErr(pchMsg, strerror(errno));
       if (NULL != func) {
        (*func)(param);
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
    ptShared->tAcceptMutex.mutex_owner = -1;
    pthread_mutex_init(&ptShared->tAcceptMutex.accept_mutex, NULL);
}

//初始化子进程
void InitChild(u8 byIndex) {
    //初始化进程连接信息
    ptShared->infos[byIndex].pid = getpid();
    ptShared->infos[byIndex].nMaxConnNum = MAX_BACKLOG / MAX_CHILD_NUM;
    ptShared->infos[byIndex].nLeftConnNum = ptShared->infos[byIndex].nMaxConnNum;
}

//查找最空闲的子进程
pid_t FindMostIdleChild() {
    pid_t target = -1;
    s32 nLeftConnNum = 0;

    for (u8 byIdx = 0; byIdx < MAX_CHILD_NUM; byIdx++) {
        s32 nLeft = ptShared->infos[byIdx].nLeftConnNum;
        if (nLeft > nLeftConnNum) {
            nLeftConnNum = ptShared->infos[byIdx].nLeftConnNum;
            target = ptShared->infos[byIdx].pid;
        }    
    }

    return target;
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
