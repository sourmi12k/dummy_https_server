#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "http.h"
#include "log.h"

static char www_dir[] = "www";

#define LEGAL_CHAR_NUM 18
static char legal_chars[] = " .~;+*/=:()-_\",&?";

static int isLetter(char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'); }
static int isDigit(char ch) { return ch >= '0' && ch <= '9'; }
static int isOtherLegalChar(char ch) {
  for (int i = 0; i < LEGAL_CHAR_NUM; ++i) {
    if (ch == legal_chars[i]) {
      return 1;
    }
  }
  return 0;
}

static int isLegalToken(const char *token, int size) {
  // HttpMethod(token)全是大写英文字母
  for (int i = 0; i < size; i++) {
    if (!isLetter(token[i])) {
      return 0;
    }
  }
  return 1;
}

static int isLegalText(const char *text, int size) {
  for (int i = 0; i < size; i++) {
    if (!isLetter(text[i]) && !isDigit(text[i]) && !isOtherLegalChar(text[i])) {
      return 0;
    }
  }
  return 1;
}

static int parseRequestLine(RequestLine *line, const char *data, int size) {
  // \r\n结尾
  assert(size >= 2 && data[size - 1] == '\n' && data[size - 2] == '\r');
  // 枚举变量，赋值 0，1，2，3
  enum { METHOD = 0, URI, VERSION, END };
  int last = 0, curr = 0, state = METHOD;
  while (curr < size - 1) {
    // method和 uri是空格判断，version是结尾
    if (data[curr] == ' ' || curr == size - 2) {
      if (last >= curr) {
        return WRONG_FORMAT;
      }
      char *dest;
      int (*isLegal)(const char *, int) = isLegalText;
      int maxlength;
      switch (state) {
        case METHOD:
          isLegal = isLegalToken;
          maxlength = MAX_SHORT_STRLEN - 1;
          dest = line->httpMethod;
          break;
        case URI:
          maxlength = MAX_LONG_STRLEN - 1;
          dest = line->httpUri;
          break;
        case VERSION:
          maxlength = MAX_SHORT_STRLEN - 1;
          dest = line->httpVersion;
          break;
        case END:
          return WRONG_FORMAT;
      }
      if (curr - last > maxlength || !isLegal(data + last, curr - last)) {
        return WRONG_FORMAT;
      }
      strncpy(dest, data + last, curr - last);
      dest[curr - last] = '\0';
      ++state;
      last = curr + 1;
    }
    ++curr;
  }

  if (state != END) return WRONG_FORMAT;
  return SUCCESS;
}

static int parseRequestHeader(HeaderList *list, const char *data, int size) {
  // TODO(sourmi12k):
  //      1. find character ':'
  //      2. string before ':' is header name and string after is header value
  assert(size >= 2 && data[size - 1] == '\n' && data[size - 2] == '\r');
  if (size == 2) {
    return END_OF_HEAD;
  }

  enum { HEADER_NAME = 0, HEADER_VALUE, END };
  int last = 0, curr = 0, state = HEADER_NAME;
  struct Header *new_header = (struct Header *)malloc(sizeof(struct Header));
  new_header->next = NULL;
  assert(list->tail->next == NULL);
  list->tail->next = new_header;
  list->tail = new_header;
  while (curr < size - 1) {
    if ((data[curr] == ':' && state == HEADER_NAME) || curr == size - 2) {
      if (curr <= last) {
        return WRONG_FORMAT;
      }
      char *dest;
      switch (state) {
        case HEADER_NAME:
          dest = new_header->headerName;
          break;
        case HEADER_VALUE:
          dest = new_header->headerValue;
          break;
        case END:
          return WRONG_FORMAT;
      }
      if (curr - last >= MAX_LONG_STRLEN || !isLegalText(data + last, curr - last - 1)) {
        return WRONG_FORMAT;
      }
      strncpy(dest, data + last, curr - last);
      dest[curr - last] = '\0';
      ++state;
      last = curr + 1;
    }
    ++curr;
  }

  if (state != END) {
    return WRONG_FORMAT;
  }
  return SUCCESS;
}

void InitRequest(Request *request) {
  struct Header *head = (struct Header *)malloc(sizeof(struct Header));
  head->next = NULL;
  request->list.head = head;
  request->list.tail = head;
  request->body_size = 0;
}
static void clearRequest(Request *request) {
  if (request->body_size > 0) {
    free(request->body);
  }
  request->body_size = 0;
  struct Header *last = request->list.head->next;
  while (last != NULL) {
    struct Header *tmp = last;
    last = last->next;
    free(tmp);
  }
  request->list.head->next = NULL;
  request->list.tail = request->list.head;
}

static void toLowerCase(char *string, int size) {
  for (int i = 0; i < size; ++i) {
    if (string[i] >= 'A' && string[i] <= 'Z') {
      string[i] += 'a' - 'A';
    }
  }
}

static int methodToNumber(const char *method, int size) {
  if (strcmp("GET", method) == 0) {
    return METHOD_GET;
  }
  if (strcmp("POST", method) == 0) {
    return METHOD_POST;
  }
  if (strcmp("HEAD", method) == 0) {
    return METHOD_HEAD;
  }
  return -1;
}
static int parseHead(Request *request, Buffer *buffer, int *state, int *method) {
  while (1) {
    int length = BufferFindCRLF(buffer);
    if (length < 0) {
      return CONTINUE;
    }
    char *data = BufferRetrieveData(buffer, length);
    switch (*state) {
      case LINE: {
        int ret = parseRequestLine(&request->request_line, data, length);
        switch (ret) {
          case WRONG_FORMAT:
            return WRONG_FORMAT;
          case SUCCESS:
            *method = methodToNumber(request->request_line.httpMethod, strlen(request->request_line.httpMethod));
            break;
        }
        ++(*state);
      } break;
      case HEADER: {
        int ret = parseRequestHeader(&request->list, data, length);
        switch (ret) {
          case WRONG_FORMAT:
            return WRONG_FORMAT;
          case END_OF_HEAD:
            ++(*state);
            return SUCCESS;
          case SUCCESS: {
            char *header_name = request->list.tail->headerName;
            toLowerCase(header_name, strlen(header_name));
            if (strcmp("content-length", header_name) == 0) {
              request->body_size = atoi(request->list.tail->headerValue);
              request->body = (char *)malloc(request->body_size);
            }
          } break;
        }
      } break;
    }
  }
}
static int parseRequest(HTTPClient *client, Buffer *buffer) {
  int ret;
  if (client->state != BODY &&
      (ret = parseHead(&client->request, buffer, &client->state, &client->method)) != SUCCESS) {
    return ret;
  }
  assert(client->state == BODY);
  if (strcmp(client->request.request_line.httpVersion, "HTTP/1.1") != 0) {
    return WRONG_VERSION;
  }
  if (client->method < 0) {
    return METHOD_NOT_IMPLEMENTED;
  }
  if (client->request.body_size <= 0) {
    return SUCCESS;
  }
  if (BufferGetSize(buffer) < client->request.body_size) {
    return CONTINUE;
  }
  char *data = BufferRetrieveData(buffer, client->request.body_size);
  memcpy(client->request.body, data, client->request.body_size);
  return SUCCESS;
}
static int isCGIRequest(char *uri) {
  // uri begin with /cgi is a CGI request
  return strlen(uri) > 5 && uri[0] == '/' && uri[1] == 'c' && uri[2] == 'g' && uri[3] == 'i';
}
static void response200(HTTPClient *client) {
  char response[] = "HTTP/1.1 200 OK\r\nServer: Liso\r\n\r\n";
  client->send_response(client->conn, response, strlen(response));
}
static void response400(HTTPClient *client) {
  char response[] = "HTTP/1.1 400 Bad Request\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
  client->send_response(client->conn, response, strlen(response));
}
static void response404(HTTPClient *client) {
  char response[] = "HTTP/1.1 404 Not Found\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
  client->send_response(client->conn, response, strlen(response));
  client->close_conn(client->conn);
}
// static void response408(HTTPClient *client) {
//   char response[] = "HTTP/1.1 408 Request Timeout\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
//   client->send_response(client->conn, response, strlen(response));
// }
// static void response411(HTTPClient *client) {
//   char response[] = "HTTP/1.1 411 Length Required\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
//   client->send_response(client->conn, response, strlen(response));
// }
static void response500(HTTPClient *client) {
  char response[] = "HTTP/1.1 500 Internal Error\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
  client->send_response(client->conn, response, strlen(response));
  client->close_conn(client->conn);
}
static void response501(HTTPClient *client) {
  char response[] = "HTTP/1.1 501 Not Implemented\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
  client->send_response(client->conn, response, strlen(response));
  client->close_conn(client->conn);
}
static void response505(HTTPClient *client) {
  char response[] = "HTTP/1.1 505 HTTP Version Not Supported\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
  client->send_response(client->conn, response, strlen(response));
  client->close_conn(client->conn);
}
static int processRequest(HTTPClient *client) {
  char file_path[4296];
  snprintf(file_path, sizeof(file_path), "%s%s", www_dir, client->request.request_line.httpUri);
  if (strstr(file_path, "../") != NULL || strstr(file_path, "./") != NULL) {
    response400(client);
    client->close_conn(client->conn);
    return -1;
  }
  switch (client->method) {
    case METHOD_GET: {
      int fd = open(file_path, O_RDONLY);
      if (fd < 0) {
        response404(client);
        return -1;
      }
      char response_head[400] = "HTTP/1.1 200 OK\r\nServer: Liso\r\nContent-length: ";
      struct stat file_stat;
      stat(file_path, &file_stat);
      ssize_t content_length = file_stat.st_size;
      snprintf(response_head + strlen(response_head), sizeof(response_head), "%ld\r\n\r\n", content_length);
      int head_len = strlen(response_head);
      char *response = (char *)malloc(content_length + head_len);
      snprintf(response, content_length + head_len, "%s", response_head);
      ssize_t nread = read(fd, response + head_len, content_length);
      close(fd);
      if (nread == content_length) {
        client->send_response(client->conn, response, head_len + content_length);
      }
      free(response);
      if (nread != content_length) {
        response500(client);
        return -1;
      }
    } break;
    case METHOD_HEAD: {
      struct stat file_stat;
      if (stat(file_path, &file_stat) < 0) {
        response404(client);
        return -1;
      } else {
        response200(client);
      }
    } break;
    case METHOD_POST:
      // POST should be handled by cgi
      response400(client);
      return -1;
      break;
  }
  return 0;
}
static void HTTPClientReset(HTTPClient *client) {
  clearRequest(&client->request);
  client->state = LINE;
}
void HTTPClientHandleMessage(HTTPClient *client, Buffer *buffer) {
  int ret = parseRequest(client, buffer);
  switch (ret) {
    case WRONG_VERSION: {
      client->wrong_format = 0;
      response505(client);
      return;
    } break;
    case METHOD_NOT_IMPLEMENTED: {
      client->wrong_format = 0;
      response501(client);
      return;
    } break;
    case CONTINUE:
      return;
    case WRONG_FORMAT: {
      if (!client->wrong_format) {
        response400(client);
        client->wrong_format = 1;
      }
    } break;
    case SUCCESS: {
      client->wrong_format = 0;
      if (isCGIRequest(client->request.request_line.httpUri)) {
        LogDebug("CGI request, disable reading, fd: %d\n", client->get_channel(client->conn)->fd);
        ChannelDisableReading(client->get_channel(client->conn));
        CGIPush(client);
        client->buffer = buffer;
        return;
      } else if (processRequest(client) < 0) {
        return;
      }
    } break;
  }
  HTTPClientReset(client);
  // process requests revursively
  HTTPClientHandleMessage(client, buffer);
}

HTTPClient *HTTPClientNew(void *conn, void (*send_response)(void *, const char *, int), Channel *(*get_channel)(void *),
                          void (*close_conn)(void *)) {
  HTTPClient *client = (HTTPClient *)malloc(sizeof(HTTPClient));
  InitRequest(&client->request);
  client->conn = conn;
  client->send_response = send_response;
  client->get_channel = get_channel;
  client->state = LINE;
  client->wrong_format = 0;
  client->close_conn = close_conn;
  return client;
}
void HTTPClientFree(HTTPClient *client) {
  clearRequest(&client->request);
  free(client->request.list.head);
  free(client);
}
void HTTPClientSend(HTTPClient *client, const char *data, int size) {
  if (size <= 0) {
    response500(client);
  } else {
    // potential bug
    client->send_response(client->conn, data, size);
  }
}

void HTTPSendComplete(HTTPClient *client) {
  LogDebug("CGI send completed, fd: %d\n", client->get_channel(client->conn)->fd);
  ChannelEnableReading(client->get_channel(client->conn));
  HTTPClientReset(client);
  // process requests recursively
  HTTPClientHandleMessage(client, client->buffer);
}
