#include "eventloop.h"

void ChannelInit(Channel *channel, int fd, EventLoop *eventloop, void (*handleRead)(void *),
                 void (*handleWrite)(void *)) {
  channel->events = 0;
}
void ChannelFree(Channel *channel) {}
void ChannelEnableReading(Channel *channel) { channel->events = 0; }
void ChannelEnableWriting(Channel *channel) {}
void ChannelDisableReading(Channel *channel) { channel->events = 1; }
void ChannelDisableWriting(Channel *channel) {}
// void DisableAll(Channel *channel);
int ChannelReadingEnabled(Channel *channel) { return 0; }
int ChannelWritingEnabled(Channel *channel) { return 0; }
int ChannelGetFD(Channel *channel) { return 0; }
void ChannelShutDownWrite(Channel *channel) {}

EventLoop *NewLoop() { return NULL; }
void LoopSetThread(EventLoop *loop, pthread_t tid) {}
void Loop(EventLoop *loop) {}
void QuitLoop(EventLoop *loop) {}