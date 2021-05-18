#ifndef LS_ACCEPTOR_H
#define LS_ACCEPTOR_H
#include "buffer.h"
#include "eventloop.h"
#include "threadpool.h"

// Acceptor负责管理监听套接字
typedef struct {
  Channel accept_channel;
  EventLoop *loop;
  ThreadPool *thread_pool;
  void (*handleNewConn)(EventLoop *, int);
} Acceptor;

/**
 * @brief 初始化一个Acceptor
 *
 * @param acceptor
 * @param loop 该acceptor所在的位置
 * @param pool 本acceptor使用的线程池
 * @param port 端口
 * @param handleNewConn 新连接到来时会调用该函数以创建新的连接对象
 */
void AcceptorInit(Acceptor *acceptor, EventLoop *loop, ThreadPool *pool, int port,
                  void (*handleNewConn)(EventLoop *, int));
#endif