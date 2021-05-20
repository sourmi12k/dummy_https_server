#include "tcpconn.h"
#include <errno.h>
#include <string.h>
#include "log.h"
// 返回该连接中的Channel,HTTP解析层使用
Channel *TCPConnGetChannel(void *conn) {
  TCPConn *tcpconn = (TCPConn *)conn;
  return &tcpconn->channel;
}
// 发送数据
void TCPConnSend(void *conn, const char *data, int size) {
  TCPConn *tcpconn = (TCPConn *)conn;
  BufferAppend(&tcpconn->output_buffer, data, size);
  if (!ChannelWritingEnabled(&tcpconn->channel)) {
    int err;
    int ret = BufferWriteToFD(&tcpconn->output_buffer, ChannelGetFD(&tcpconn->channel), &err);
    switch (ret) {
      case ERROR:
        if (err != EAGAIN && err != EINTR) {
          LogError("Write Error, connection closed, fd: %d, Error: %s\n", tcpconn->channel.fd, strerror(err));
          TCPConnClose(tcpconn);
        }
        break;
      case ALLWRITTEN:
        break;
      case PARTIALWRITTEN:
        ChannelEnableWriting(&tcpconn->channel);
        break;
    }
  }
}
static void doCloseTCPConn(TCPConn *conn) {
  ChannelFree(&conn->channel);
  BufferFree(&conn->input_buffer);
  BufferFree(&conn->output_buffer);
  HTTPClientFree(conn->http_client);
  free(conn);
}
// 关闭连接
void TCPConnClose(void *tcpconn) {
  TCPConn *conn = (TCPConn *)tcpconn;
  if (ChannelWritingEnabled(&conn->channel)) {
    conn->closed = 1;
  } else {
    doCloseTCPConn(tcpconn);
  }
}
// 当Channel可读时调用的函数
void TCPConnHandleRead(void *chan) {
  Channel *ch = (Channel *)chan;
  assert(ChannelReadingEnabled(ch));
  TCPConn *conn = container_of(chan, TCPConn, channel);

  int err;
  int size = BufferReadFromFD(&conn->input_buffer, ChannelGetFD(&conn->channel), &err);
  if (size < 0) {
    if (err != EAGAIN && err != EINTR) {
      // this happens mainly because of receiving RST from client
      LogError("Read Error, connection closed, fd: %d, Error: %s\n", conn->channel.fd, strerror(err));
      TCPConnClose(conn);
    }
  } else if (size == 0) {
    LogDebug("connection closed, fd: %d\n", ch->fd);
    TCPConnClose(conn);
  } else {
    assert(pthread_self() == ((EventLoop *)(ch->loop))->tid);
    HTTPClientHandleMessage(conn->http_client, &conn->input_buffer);
  }
}
// 当Channel 可写时调用的函数
void TCPConnHandleWrite(void *chan) {
  Channel *ch = (Channel *)chan;
  assert(ChannelWritingEnabled(ch));
  TCPConn *conn = container_of(ch, TCPConn, channel);
  int err;
  int ret = BufferWriteToFD(&conn->output_buffer, ChannelGetFD(&conn->channel), &err);
  switch (ret) {
    case ALLWRITTEN:
      LogDebug("all written, fd: %d\n", ch->fd);
      ChannelDisableWriting(ch);
      if (conn->closed) {
        doCloseTCPConn(conn);
      }
      break;
    case PARTIALWRITTEN:
      break;
    case ERROR:
      if (err != EAGAIN && err != EINTR) {
        LogError("Write Error, connection closed, fd: %d, Error: %s\n", conn->channel.fd, strerror(err));
        TCPConnClose(conn);
      }
      break;
  }
}
// 当有新连接到来时调用的函数
void TCPConnHandleNewConn(EventLoop *loop, int connfd) {
  TCPConn *conn = (TCPConn *)malloc(sizeof(TCPConn));
  ChannelInit(&conn->channel, connfd, loop, TCPConnHandleRead, TCPConnHandleWrite);
  conn->closed = 0;
  BufferInit(&conn->input_buffer);
  BufferInit(&conn->output_buffer);
  conn->http_client = HTTPClientNew(conn, TCPConnSend, TCPConnGetChannel, TCPConnClose);
  ChannelEnableReading(&conn->channel);
}
