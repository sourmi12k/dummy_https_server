#include "acceptor.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "log.h"

#include "utils.h"
#define MAXCONN 4096

void AcceptorSetNewConnFunc(Acceptor *acceptor, void (*handleNewConn)(EventLoop *, int)) {
  acceptor->handleNewConn = handleNewConn;
}
static int getListenfd(int port) {
  int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(listenfd >= 0);

  int on = 1;

  int flags = fcntl(listenfd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  fcntl(listenfd, F_SETFD, flags);
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    LogFatal("setsockopt failed\n");
    sleep(1);
    exit(-1);
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LogFatal("bind failed\n");
    sleep(1);
    exit(-1);
  }

  if (listen(listenfd, MAXCONN) < 0) {
    close(listenfd);
    LogFatal("listen failed\n");
    sleep(1);
    exit(-1);
  }
  return listenfd;
}
static void acceptorHandleRead(void *chan) {
  Channel *ch = (Channel *)chan;
  struct sockaddr_in addr;
  socklen_t addrlen;
  int connfd = accept(ch->fd, (struct sockaddr *)&addr, &addrlen);
  if (connfd < 0) {
    LogError("accept returns a negative connfd\n");
    return;
  }
  LogDebug("new connection connfd: %d\n", connfd);

  // nonblock
  int flags = fcntl(connfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(connfd, F_SETFL, flags);

  // close-on-exec
  flags = fcntl(connfd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  fcntl(connfd, F_SETFD, flags);

  Acceptor *acceptor = container_of(chan, Acceptor, accept_channel);
  EventLoop *loop = NextLoop(acceptor->thread_pool);
  if (!loop) {
    loop = acceptor->loop;
  }
  acceptor->handleNewConn(loop, connfd);
}
void AcceptorInit(Acceptor *acceptor, EventLoop *loop, ThreadPool *pool, int port,
                  void (*handleNewConn)(EventLoop *, int)) {
  int listenfd = getListenfd(port);
  acceptor->loop = loop;
  acceptor->thread_pool = pool;
  acceptor->handleNewConn = handleNewConn;
  ChannelInit(&acceptor->accept_channel, listenfd, loop, acceptorHandleRead, NULL);
  ChannelEnableReading(&acceptor->accept_channel);
  LogDebug("acceptor init, port: %d, listenfd: %d\n", port, listenfd);
}
