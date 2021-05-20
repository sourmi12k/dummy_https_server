#ifndef INCLUDE_TLSCONN_H_
#define INCLUDE_TLSCONN_H_
#include <openssl/ssl.h>
#include "buffer.h"
#include "eventloop.h"
#include "http.h"

enum { TLS_TCP_CONNECTED = 0, TLS_CONNECTED, TLS_CLOSED };
typedef struct {
  Channel channel;
  int state;
  SSL *client_context;
  Buffer input_buffer;
  Buffer output_buffer;
  HTTPClient *http_client;
} TLSConn;
void TLSInit();
Channel *TLSGetChannel(void *conn);
void TLSSend(void *conn, const char *data, int size);
void TLSConnClose(void *conn);
void TLSHandleRead(void *chan);
void TLSHandleWrite(void *chan);
void TLSHandleNewConn(EventLoop *loop, int connfd);
#endif  // INCLUDE_TLSCONN_H_
