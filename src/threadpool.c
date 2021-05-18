
#include "threadpool.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
static void *threadInitFunc(void *loop) {
  LoopSetThread((EventLoop *)loop, pthread_self());
  Loop((EventLoop *)loop);
  return NULL;
}

void InitThreadPool(ThreadPool *threadpool, int threadnum) {
  assert(threadnum >= 1);
  memset(threadpool, 0, sizeof(ThreadPool));
  if (threadnum <= 0) return;
  threadpool->next = 0;
  threadpool->threadNum = threadnum;
  threadpool->loops = (EventLoop **)malloc(sizeof(EventLoop *) * threadnum);
  threadpool->tids = (pthread_t *)malloc(sizeof(pthread_t) * threadnum);
  for (int i = 0; i < threadnum; ++i) {
    threadpool->loops[i] = NewLoop();
    if (pthread_create(&threadpool->tids[i], NULL, threadInitFunc, threadpool->loops[i]) < 0) {
      LogFatal("thread create failed\n");
      exit(-1);
    }
  }
}

void StopAndWaitTP(ThreadPool *threadpool) {
  for (int i = 0; i < threadpool->threadNum; ++i) {
    QuitLoop(threadpool->loops[i]);
  }
  for (int i = 0; i < threadpool->threadNum; ++i) {
    pthread_join(threadpool->tids[i], NULL);
  }
  free(threadpool->tids);
  free(threadpool->loops);
}

EventLoop *NextLoop(ThreadPool *threadpool) {
  threadpool->next = (threadpool->next + 1) % threadpool->threadNum;
  return threadpool->loops[threadpool->next];
}