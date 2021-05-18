#include "buffer.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include "log.h"

#define BUFFERINITSIZE 4096

void BufferInit(Buffer *buffer) {
  buffer->data = (char *)malloc(BUFFERINITSIZE);
  buffer->size = BUFFERINITSIZE;
  buffer->readIndex = 0;
  buffer->writeIndex = 0;
}

void BufferInitWithData(Buffer *buffer, char *data, int size) {
  buffer->data = data;
  buffer->size = size;
  buffer->writeIndex = size;
}

static void resize(Buffer *buffer, int newsize) {
  assert(newsize > buffer->size);
  char *newdata = (char *)malloc(newsize);
  int oldreadsize = buffer->writeIndex - buffer->readIndex;
  memcpy(newdata, buffer->data + buffer->readIndex, oldreadsize);
  free(buffer->data);
  buffer->data = newdata;
  buffer->readIndex = 0;
  buffer->writeIndex = oldreadsize;
  buffer->size = newsize;
}

int BufferReadFromFD(Buffer *buffer, int fd, int *err) {
  char extrabuf[65536];
  struct iovec iov[2];
  iov[0].iov_base = buffer->data + buffer->writeIndex;
  iov[0].iov_len = buffer->size - buffer->writeIndex;
  iov[1].iov_base = extrabuf;
  iov[1].iov_len = 65536;
  int nread = readv(fd, iov, 2);
  // LogDebug("data received from connfd %d, size=%d\n", fd, nread);
  if (nread < 0) {
    // do nothing
    *err = errno;
  } else if (nread <= iov[0].iov_len) {
    buffer->writeIndex += nread;
  } else {
    int extrabytes = nread - buffer->writeIndex;
    resize(buffer, buffer->writeIndex + nread);
    memcpy(buffer->data + buffer->writeIndex, extrabuf, extrabytes);
    buffer->writeIndex += nread;
  }
  // char *start = buffer->data + buffer->readIndex;
  // for (int i = 0; i < BufferGetSize(buffer); ++i) {
  //   printf("%c", start[i]);
  // }
  return nread;
}
int BufferReadFromSSL(Buffer *buffer, SSL *client_context, int *err) {
  int writable_bytes = buffer->size - buffer->writeIndex;
  assert(writable_bytes > 0);
  int nread = SSL_read(client_context, buffer->data + buffer->writeIndex, writable_bytes);
  if (nread < 0) {
    *err = SSL_get_error(client_context, nread);
  } else if (nread < writable_bytes) {
    buffer->writeIndex += nread;
  } else {
    resize(buffer, buffer->size + 1024);
  }
  // for (int i = 0; i < nread; ++i) {
  //   printf("%c", buffer->data[i]);
  // }
  return nread;
}

void BufferFree(Buffer *buffer) { free(buffer->data); }

void BufferAppend(Buffer *buffer, const char *data, int size) {
  if (size == 0) {
    return;
  }
  if (size > buffer->size - buffer->writeIndex) {
    resize(buffer, size + buffer->writeIndex);
  }
  memcpy(buffer->data + buffer->writeIndex, data, size);
  buffer->writeIndex += size;
}

int BufferFindCRLF(Buffer *buffer) {
  char *start = buffer->data + buffer->readIndex;
  for (int i = 0; i < buffer->writeIndex - buffer->readIndex - 1; ++i) {
    if (start[i] == '\r' && start[i + 1] == '\n') {
      return i + 2;
    }
  }
  return -1;
}

char *BufferRetrieveData(Buffer *buffer, int length) {
  assert(length <= buffer->writeIndex - buffer->readIndex);
  int read_index = buffer->readIndex;
  buffer->readIndex += length;
  if (buffer->readIndex == buffer->writeIndex) {
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
  }
  return buffer->data + read_index;
}

int BufferWriteToFD(Buffer *buffer, int fd, int *err) {
  int readableBytes = buffer->writeIndex - buffer->readIndex;
  int nwrite = write(fd, buffer->data + buffer->readIndex, readableBytes);
  assert(nwrite <= readableBytes);
  if (nwrite < 0) {
    *err = errno;
    return ERROR;
  }
  BufferRetrieveData(buffer, nwrite);
  if (readableBytes == nwrite) {
    return ALLWRITTEN;
  }
  return PARTIALWRITTEN;
}
int BufferWriteToSSL(Buffer *buffer, SSL *client_context, int *err) {
  int readable_bytes = BufferGetSize(buffer);
  int nwrite = SSL_write(client_context, buffer->data + buffer->readIndex, readable_bytes);
  if (nwrite < 0) {
    *err = SSL_get_error(client_context, nwrite);
    return ERROR;
  }

  BufferRetrieveData(buffer, nwrite);
  if (readable_bytes == nwrite) {
    return ALLWRITTEN;
  }
  return PARTIALWRITTEN;
}

int BufferFindString(Buffer *buffer) {
  char *start = buffer->data + buffer->readIndex;
  for (int i = 0; i < buffer->writeIndex - buffer->readIndex; ++i) {
    if (start[i] == 0) {
      return i + 1;
    }
  }
  return -1;
}

int BufferGetSize(Buffer *buffer) { return buffer->writeIndex - buffer->readIndex; }
