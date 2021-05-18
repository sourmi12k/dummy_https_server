#ifndef LS_EVENTLOOP_H
#define LS_EVENTLOOP_H
#include <pthread.h>
#include <stdint.h>

#include "utils.h"
typedef struct {
  int fd;
  void *loop;
  uint64_t events;
  int op;
  void (*handleRead)(void *);
  void (*handleWrite)(void *);
} Channel;
typedef struct {
  int epfd;
  volatile int quit;
  pthread_t tid;
  Channel eventCh;
  DArray pendingChannels;
  pthread_mutex_t lock;
} EventLoop;

void ChannelInit(Channel *channel, int fd, EventLoop *eventloop, void (*handleRead)(void *),
                 void (*handleWrite)(void *));
void ChannelFree(Channel *channel);
void ChannelEnableReading(Channel *channel);
void ChannelEnableWriting(Channel *channel);
void ChannelDisableReading(Channel *channel);
void ChannelDisableWriting(Channel *channel);
// void DisableAll(Channel *channel);
int ChannelReadingEnabled(Channel *channel);
int ChannelWritingEnabled(Channel *channel);
int ChannelGetFD(Channel *channel);
void ChannelShutDownWrite(Channel *channel);

EventLoop *NewLoop();
void LoopSetThread(EventLoop *loop, pthread_t tid);
void Loop(EventLoop *loop);
void QuitLoop(EventLoop *loop);

#endif