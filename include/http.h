#ifndef INCLUDE_HTTP_H_
#define INCLUDE_HTTP_H_
#include <assert.h>
#include "buffer.h"
#include "eventloop.h"

#define MAX_LONG_STRLEN 4096
#define MAX_SHORT_STRLEN 50

enum { METHOD_GET = 0, METHOD_HEAD, METHOD_POST };
typedef struct {
  char httpVersion[MAX_SHORT_STRLEN];
  char httpMethod[MAX_SHORT_STRLEN];
  char httpUri[MAX_LONG_STRLEN];
} RequestLine;
struct Header {
  char headerName[MAX_LONG_STRLEN];
  char headerValue[MAX_LONG_STRLEN];
  struct Header *next;
};
typedef struct {
  struct Header *head;
  struct Header *tail;
  int length;
} HeaderList;
typedef struct {
  RequestLine request_line;
  HeaderList list;
  char *body;
  int body_size;
} Request;

#define WRONG_VERSION 5
#define METHOD_NOT_IMPLEMENTED 4
#define CONTINUE 3
#define END_OF_HEAD 2
#define WRONG_FORMAT 1
#define SUCCESS 0

enum { LINE = 0, HEADER, BODY, END };
typedef struct {
  int state;
  int wrong_format;
  int method;
  Buffer *buffer;
  Request request;
  void *conn;
  void (*send_response)(void *, const char *, int);
  Channel *(*get_channel)(void *);
  void (*close_conn)(void *);
} HTTPClient;
/**
 * @brief 创建新的HTTPClient
 *
 * @param conn 该client对应的TCPConn结构
 * @param send_response 用于发送数据的回调函数
 * @param get_channel 用于获取该连接的channel
 * @return HTTPClient* 创建的新client
 */
HTTPClient *HTTPClientNew(void *conn, void (*send_response)(void *, const char *, int), Channel *(*get_channel)(void *),
                          void (*close_conn)(void *));
/**
 * @brief 释放client相关资源
 *
 * @param client 目标
 */
void HTTPClientFree(HTTPClient *client);
/**
 * @brief 从buffer中取出数据进行处理
 *
 * @param client 用于保存客户端的状态
 * @param buffer 存储数据的缓冲区
 */
void HTTPClientHandleMessage(HTTPClient *client, Buffer *buffer);
/**
 * @brief CGI用于发送回复的函数
 *
 * @param client
 * @param data 数据
 * @param size 数据长度
 */
void HTTPClientSend(HTTPClient *client, const char *data, int size);
void HTTPSendComplete(HTTPClient *client);

void CGIInit(int queue_size);
void CGIPush(HTTPClient *client);
void CGIStopAndWait();
#endif  // INCLUDE_HTTP_H_
