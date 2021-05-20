#ifndef INCLUDE_TCPCONN_H_
#define INCLUDE_TCPCONN_H_

#include "buffer.h"
#include "eventloop.h"
#include "http.h"

// this structure stores connection information
typedef struct {
  Channel channel;
  Buffer input_buffer;
  Buffer output_buffer;
  HTTPClient *http_client;
  int closed;
} TCPConn;
Channel *TCPConnGetChannel(void *conn);
void TCPConnSend(void *conn, const char *data, int size);
void TCPConnClose(void *conn);
void TCPConnHandleRead(void *chan);
void TCPConnHandleWrite(void *chan);
void TCPConnHandleNewConn(EventLoop *loop, int connfd);
#endif  // INCLUDE_TCPCONN_H_
