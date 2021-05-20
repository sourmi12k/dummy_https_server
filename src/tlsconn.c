#include "tlsconn.h"
#include <signal.h>
#include "log.h"
#include "utils.h"
static SSL_CTX *ssl_context;
static char private_key_file[] = "localhost.key";
static char public_key_file[] = "localhost.crt";
static SSL_CTX *createContext() {
  SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key_file, SSL_FILETYPE_PEM)) {
    printf("SSL_CTX_use_PrivateKey_file failed\n");
    exit(-1);
  }
  if (!SSL_CTX_use_certificate_file(ssl_ctx, public_key_file, SSL_FILETYPE_PEM)) {
    printf("SSL_CTX_use_certificate_file failed\n");
    exit(-1);
  }
  return ssl_ctx;
}
static void sighup_handler(int sig) {
  assert(sig == SIGHUP);
  LogDebug("received SIGHUP, rehash server..\n");
  SSL_CTX *old_ctx = ssl_context;
  ssl_context = createContext();
  SSL_CTX_free(old_ctx);
}

void TLSInit() {
  SSL_load_error_strings();
  SSL_library_init();
  signal(SIGHUP, sighup_handler);
  ssl_context = createContext();
}

Channel *TLSGetChannel(void *conn) {
  TLSConn *tlsconn = (TLSConn *)conn;
  return tlsconn->channel;
}
void TLSSend(void *conn, const char *data, int size) {
  TLSConn *tlsconn = (TLSConn *)conn;
  BufferAppend(&tlsconn->output_buffer, data, size);
  if (!ChannelWritingEnabled(&tlsconn->channel)) {
    int err;
    int ret = BufferWriteToSSL(&tlsconn->output_buffer, tlsconn->client_context, &err);
    switch (ret) {
      case ERROR:
        break;
      case ALLWRITTEN:
        break;
      case PARTIALWRITTEN:
        LogDebug("ssl partial written, channel fd: %d\n", tlsconn->channel.fd);
        ChannelEnableWriting(&tlsconn->channel);
        break;
    }
  }
}
static void doCloseTLSConn(TLSConn *tlsconn, int connected) {
  ChannelFree(&tlsconn->channel);
  SSL_free(tlsconn->client_context);
  if (connected) {
    BufferFree(&tlsconn->input_buffer);
    BufferFree(&tlsconn->output_buffer);
    HTTPClientFree(tlsconn->http_client);
  }
  free(tlsconn);
}
void TLSConnClose(void *tlsconn) {
  TLSConn *conn = (TLSConn *)tlsconn;
  if (ChannelWritingEnabled(&conn->channel)) {
    conn->state = TLS_CLOSED;
  } else {
    doCloseTLSConn(conn, conn->state == TLS_CONNECTED);
  }
}
void TLSHandleRead(void *chan) {
  Channel *ch = (Channel *)chan;
  assert(ChannelReadingEnabled(ch));
  TLSConn *conn = container_of(ch, TLSConn, channel);

  switch (conn->state) {
    case TLS_TCP_CONNECTED: {
      int ret = SSL_accept(conn->client_context);
      if (ret > 0) {
        conn->state = TLS_CONNECTED;
        BufferInit(&conn->input_buffer);
        BufferInit(&conn->output_buffer);
        conn->http_client = HTTPClientNew(conn, TLSSend, TLSGetChannel, TLSConnClose);
      } else if (SSL_get_error(conn->client_context, ret) != SSL_ERROR_WANT_READ) {
        LogError("handshake failed, close connnection, connfd: %d, errcode: %d\n", conn->channel.fd,
                 SSL_get_error(conn->client_context, ret));
        TLSConnClose((void *)conn);
      }
    } break;
    case TLS_CONNECTED: {
      int err;
      int size = BufferReadFromSSL(&conn->input_buffer, conn->client_context, &err);
      if (size == 0) {
        TLSConnClose(conn);
      } else if (size < 0) {
        switch (err) {
          case SSL_ERROR_WANT_READ:
            // do nothing
            break;
          case SSL_ERROR_ZERO_RETURN:
            LogError("read failed, close connnection, connfd: %d\n", ch->fd);
            TLSConnClose(conn);
            break;
          case SSL_ERROR_SYSCALL:
            LogError("read failed, close connnection, connfd: %d\n", ch->fd);
            TLSConnClose(conn);
            break;
        }
      } else {
        HTTPClientHandleMessage(conn->http_client, &conn->input_buffer);
      }
    } break;
  }
}
void TLSHandleWrite(void *chan) {
  Channel *ch = (Channel *)chan;
  assert(ChannelWritingEnabled(ch));
  TLSConn *conn = container_of(ch, TLSConn, channel);
  int err;
  int ret = BufferWriteToSSL(&conn->output_buffer, conn->client_context, &err);
  switch (ret) {
    case ALLWRITTEN:
      ChannelDisableWriting(ch);
      if (conn->state == TLS_CLOSED) {
        doCloseTLSConn(conn, 1);
      }
      break;
    case PARTIALWRITTEN:
      break;
    case ERROR:
      if (err == SSL_ERROR_SYSCALL) {
        TLSConnClose(conn);
      }
      break;
  }
}
void TLSHandleNewConn(EventLoop *loop, int connfd) {
  TLSConn *conn = (TLSConn *)malloc(sizeof(TLSConn));
  ChannelInit(&conn->channel, connfd, loop, TLSHandleRead, TLSHandleWrite);
  ChannelEnableReading(&conn->channel);
  conn->state = TLS_TCP_CONNECTED;
  conn->client_context = SSL_new(ssl_context);
  SSL_set_fd(conn->client_context, connfd);
}
