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
#include <set>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "base.h"
#include "log.h"

#define		SOCK_FD		    s32

#define		SERVER_IP		"192.168.152.128"
#define		LISTEN_PORT		9000
#define 	MAX_BACKLOG		10

#define     SHM_NAME        "/shm_zwf"

typedef void (*TResourceClear)();




typedef struct {
    BOOL bAccepting;
    pthread_mutex_t mutex;
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





static SOCK_FD Setup();
static void Destroy();
static void ChildTerminate(int nSigNo);                             //SIGCHLD信号处理
static void ChildProcessHandle();                                   //子进程逻辑处理
static void CloseConnectedFDs(std::set<SOCK_FD> &setFDs);           //关闭所有的已连接套接字
static void CreateShareMemory();                                    //创建共享内存

#endif	//SERVER_H_
