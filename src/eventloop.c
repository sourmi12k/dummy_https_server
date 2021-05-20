#include "eventloop.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include "log.h"

static const int noneEvent = 0;
static const int readEvent = EPOLLIN | EPOLLPRI;
static const int writeEvent = EPOLLOUT;

void ChannelInit(Channel *channel, int fd, EventLoop *eventloop, void (*handleRead)(void *),
                 void (*handleWrite)(void *)) {
  channel->fd = fd;
  channel->loop = eventloop;
  channel->events = noneEvent;
  channel->handleRead = handleRead;
  channel->handleWrite = handleWrite;
}

int ChannelReadingEnabled(Channel *channel) { return channel->events & readEvent; }

int ChannelWritingEnabled(Channel *channel) { return channel->events & writeEvent; }

int ChannelGetFD(Channel *channel) { return channel->fd; }

void ChannelShutDownWrite(Channel *channel) {
  // TODO(sourmi12k): error handling
  assert(!ChannelWritingEnabled(channel));
  shutdown(channel->fd, SHUT_WR);
}

static void modifyChannel(Channel *chan) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = chan->events;
  event.data.ptr = chan;
  EventLoop *loop = chan->loop;
  assert(loop->tid == pthread_self());
  if (epoll_ctl(loop->epfd, chan->op, chan->fd, &event) < 0) {
    LogError("epoll_ctl failed, fd: %d\n", chan->fd);
  } else if (chan->op == EPOLL_CTL_ADD) {
    LogDebug("New Channel %d in loop %d, events: %d\n", chan->fd, loop->epfd, event.events);
  }
  chan->op = 0;
}

// bad function, vely vely bad
static void update(int op, Channel *chan) {
  // assert(chan->op == 0);
  chan->op = op;
  EventLoop *loop = chan->loop;
  if (pthread_self() == loop->tid) {
    modifyChannel(chan);
  } else {
    pthread_mutex_lock(&loop->lock);
    DArrayPushBack(&loop->pendingChannels, chan);
    pthread_mutex_unlock(&loop->lock);
    uint64_t one = 1;
    write(ChannelGetFD(&loop->eventCh), &one, sizeof(one));
  }
}

void ChannelFree(Channel *channel) {
  channel->events = 0;
  update(EPOLL_CTL_DEL, channel);
  close(channel->fd);
}

void ChannelEnableReading(Channel *channel) {
  assert((channel->events & readEvent) == 0);
  int op = EPOLL_CTL_MOD;
  if (channel->events == noneEvent) {
    op = EPOLL_CTL_ADD;
  }
  channel->events |= readEvent;
  update(op, channel);
}

void ChannelEnableWriting(Channel *channel) {
  assert((channel->events & writeEvent) == 0);
  int op = EPOLL_CTL_MOD;
  if (channel->events == noneEvent) {
    op = EPOLL_CTL_ADD;
  }
  channel->events |= writeEvent;
  update(op, channel);
}

void ChannelDisableReading(Channel *channel) {
  assert(channel->events & readEvent);
  channel->events &= ~readEvent;
  int op = EPOLL_CTL_MOD;
  if (channel->events == noneEvent) {
    op = EPOLL_CTL_DEL;
  }
  update(op, channel);
}

void ChannelDisableWriting(Channel *channel) {
  assert(channel->events & writeEvent);
  channel->events &= ~writeEvent;
  int op = EPOLL_CTL_MOD;
  if (channel->events == noneEvent) {
    op = EPOLL_CTL_DEL;
  }
  update(op, channel);
}
// void DisableAll(Channel *channel) {
//   if (channel->events != noneEvent) {
//     channel->events = noneEvent;
//     update(channel);
//   }
// }

static void evtfdHandleRead(void *chan) {
  Channel *evtch = (Channel *)chan;
  EventLoop *loop = container_of(chan, EventLoop, eventCh);
  uint64_t one;
  read(evtch->fd, &one, sizeof(one));
  DArray arr;
  DArrayInit(&arr, 20);
  pthread_mutex_lock(&loop->lock);
  DArraySwap(&arr, &loop->pendingChannels);
  pthread_mutex_unlock(&loop->lock);
  void **start = DArrayStart(&arr);
  for (int i = 0; i < DArraySize(&arr); ++i) {
    Channel *ch = start[i];
    modifyChannel(ch);
  }
  DArrayFree(&arr);
}

static const int timeoutms = 200;
static const int maxfds = 4096;
EventLoop *NewLoop() {
  EventLoop *loop = (EventLoop *)malloc(sizeof(EventLoop));
  memset(loop, 0, sizeof(EventLoop));
  loop->epfd = epoll_create(maxfds);
  loop->quit = 0;
  if (loop->epfd < 0) {
    free(loop);
    return 0;
  }
  DArrayInit(&loop->pendingChannels, 20);
  pthread_mutex_init(&loop->lock, NULL);
  LogDebug("new loop, epfd: %d\n", loop->epfd);
  return loop;
}

void LoopSetThread(EventLoop *loop, pthread_t tid) {
  loop->tid = tid;
  int evtfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (evtfd < 0) {
    close(loop->epfd);
    free(loop);
    LogFatal("event fd init failed\n");
    exit(-1);
  }
  ChannelInit(&loop->eventCh, evtfd, loop, evtfdHandleRead, NULL);
  loop->eventCh.op = EPOLL_CTL_ADD;
  loop->eventCh.events = readEvent;
  modifyChannel(&loop->eventCh);
  // LogDebug("event fd channel ok\n");
}

void Loop(EventLoop *loop) {
  // todo: use a dynamic array
  struct epoll_event events[200];
  while (!loop->quit) {
    int nfds = epoll_wait(loop->epfd, events, 200, timeoutms);
    if (nfds < 0) {
      // handle error
    } else if (nfds == 0) {
      // timeout
    } else {
      for (int i = 0; i < nfds; ++i) {
        Channel *chan = events[i].data.ptr;
        uint64_t event = events[i].events;
        // TODO(sourmi12k): handle more epoll events
        if (event & (EPOLLIN | EPOLLPRI | EPOLLHUP)) {
          assert(chan->handleRead != NULL);
          chan->handleRead(chan);
        }

        if (event & EPOLLOUT) {
          assert(chan->handleWrite != NULL);
          chan->handleWrite(chan);
        }
      }
    }
  }
  DArrayFree(&loop->pendingChannels);
  close(loop->epfd);
  free(loop);
}

void QuitLoop(EventLoop *loop) { loop->quit = 1; }
