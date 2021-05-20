#include <signal.h>
#include "acceptor.h"
#include "log.h"
#include "tcpconn.h"
#include "tlsconn.h"

typedef struct {
  Acceptor http_acceptor;
  Acceptor https_acceptor;
  ThreadPool thread_pool;
  EventLoop *loop;
} LisoServer;

void LisoServerInit(LisoServer *server, int http_port, int https_port, int threadnum, const char *log_file) {
  signal(SIGPIPE, SIG_IGN);
  assert(LogInit(log_file) == 0);
  server->loop = NewLoop();
  assert(server->loop != NULL);
  LoopSetThread(server->loop, pthread_self());
  InitThreadPool(&server->thread_pool, threadnum);
  AcceptorInit(&server->http_acceptor, server->loop, &server->thread_pool, http_port, TCPConnHandleNewConn);
  AcceptorInit(&server->https_acceptor, server->loop, &server->thread_pool, https_port, TLSHandleNewConn);
  TLSInit();
  CGIInit(30);
  LogDebug("server init ok\n");
}
void LisoServerServeForever(LisoServer *server) { Loop(server->loop); }
static LisoServer liso_server;

int main(int argc, char **argv) {
  char *log_file = NULL;
  if (argc == 5) {
    log_file = argv[4];
  } else if (argc != 4) {
    printf("usage: %s [http_port] [https_port] [thread_num] [log_file]\n", argv[0]);
    return -1;
  }
  LisoServerInit(&liso_server, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), log_file);
  LisoServerServeForever(&liso_server);
}
