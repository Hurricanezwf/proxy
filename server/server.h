#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <list>
#include <set>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include "base.h"
#include "log.h"

#define		SOCK_FD		    s32

#define		SERVER_IP		"192.168.152.128"
#define		LISTEN_PORT		9000
#define 	MAX_BACKLOG		(MAX_CHILD_NUM * MAX_EPOLL_SIZE)

#define     SHM_NAME        "/shm_zwf"

#define     MAX_CHILD_NUM   2
#define     MAX_EPOLL_SIZE  15 

typedef void (*TResourceClear)();
typedef void (*ChildClear)(std::list<SOCK_FD> *pList);

typedef struct {
    pid_t   pid;                //所属进程号
    s32     nMaxConnNum;        //最大连接数量
    s32     nLeftConnNum;       //剩余连接数量
}TConnectionInfo;

typedef struct {
    BOOL bAccepting;                    //控制子进程接收连接的控制位
    pthread_mutex_t mutex;
    TConnectionInfo infos[MAX_CHILD_NUM];
}TShared;




/*
 * Description: 验证函数的执行结果,如果执行失败,则终止程序执行
 * @Param:      [IN] bSuccess--是否成功执行,TRUE为成功,FALSE为失败
 * @Param:      [IN] pchMsg--写入日志的描述
 * @Param:      [IN] func--程序终止前的垃圾回收函数指针
 * Return:      无
 * Note:        无
 */
inline static void CHK(BOOL bSuccess, s8 *pchMsg, TResourceClear func);
inline static void CHK2(BOOL bSuccess, s8 *pchMsg, void (*func)(void*), void *param);



static void server_init();
static SOCK_FD Setup();
static void Destroy();
static void ChildTerminate(int nSigNo);                                         //SIGCHLD信号处理
static void ChildProcessHandle(u8 byIndex);                                               //子进程逻辑处理
static void ChildGabageClear(void *pList);                                      //子进程资源清理
static void CloseConnectedFDs(std::list<SOCK_FD> *pclientList);                 //关闭所有的已连接套接字
static void CreateShareMemory();                                                //创建共享内存
static void InitChild(u8 byIndex);                                              //初始化指定的子进程
static s32  FindClientFromList(std::list<SOCK_FD> &clientList, SOCK_FD fd);     //从list中查找制定的fd


//调试信息打印
static void ShowChildConnInfo();                                    //显示所有子进程的连接信息
static void ShowHelp();                                             //显示调试命令的帮助信息


#endif	//SERVER_H_
