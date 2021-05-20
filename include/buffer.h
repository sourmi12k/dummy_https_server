#ifndef INCLUDE_BUFFER_H_
#define INCLUDE_BUFFER_H_
#include <openssl/ssl.h>

typedef struct {
  char *data;
  int writeIndex;
  int readIndex;
  int size;
} Buffer;

void BufferInit(Buffer *buffer);
void BufferInitWithData(Buffer *buffer, char *data, int size);
int BufferReadFromFD(Buffer *buffer, int fd, int *err);
int BufferReadFromSSL(Buffer *buffer, SSL *client_context, int *err);
void BufferAppend(Buffer *buffer, const char *data, int size);
void BufferFree(Buffer *buffer);
int BufferFindCRLF(Buffer *buffer);
char *BufferRetrieveData(Buffer *buffer, int length);
int BufferGetSize(Buffer *buffer);
int BufferFindString(Buffer *buffer);

#define ALLWRITTEN 0
#define PARTIALWRITTEN 1
#define ERROR -1
int BufferWriteToFD(Buffer *buffer, int fd, int *err);
int BufferWriteToSSL(Buffer *buffer, SSL *client_context, int *err);
#endif  // INCLUDE_BUFFER_H_
