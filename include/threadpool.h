#ifndef INCLUDE_THREADPOOL_H_
#define INCLUDE_THREADPOOL_H_
#include <pthread.h>
#include "eventloop.h"
typedef struct {
  int threadNum;
  int next;
  EventLoop **loops;
  pthread_t *tids;
} ThreadPool;

void InitThreadPool(ThreadPool *threadpool, int threadnum);
void StopAndWaitTP(ThreadPool *threadpool);
EventLoop *NextLoop(ThreadPool *threadpool);
#endif  // INCLUDE_THREADPOOL_H_
