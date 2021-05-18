#include "log.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#define MAX_LOG_LENGTH 500

static FILE *fp;
static volatile int quit;
static BlockQueue block_queue;
static pthread_t tid;

#define LOG_WRITE(log_str)                             \
  {                                                    \
    char *start = log_str + strlen(log_str);           \
    va_list args;                                      \
    va_start(args, fmt);                               \
    vsprintf(start, fmt, args);                        \
    va_end(args);                                      \
    BlockQueuePushBack(&block_queue, (void *)log_str); \
  }

static void *consumer(void *args) {
  DArray tmp;
  DArrayInit(&tmp, 20);
  while (!quit) {
    BlockQueueWaitAndSwap(&block_queue, &tmp);
    for (int i = 0; i < DArraySize(&tmp); ++i) {
      char *content = (char *)DArrayGet(&tmp, i);
      fputs(content, fp);
      free(content);
    }
    DArrayClear(&tmp);
    fflush(fp);
  }
  DArrayFree(&tmp);
  return NULL;
}

int LogInit(const char *filename) {
  if (filename == NULL) {
    fp = stdout;
  } else if ((fp = fopen(filename, "a+")) == NULL) {
    return -1;
  }
  BlockQueueInit(&block_queue, 20);
  quit = 0;
  if (pthread_create(&tid, NULL, consumer, NULL) < 0) {
    BlockQueueFree(&block_queue);
    return -1;
  }
  return 0;
}

void LogDebug(const char *fmt, ...) {
  char *log_str = (char *)malloc(MAX_LOG_LENGTH);
  memset(log_str, 0, MAX_LOG_LENGTH);
  strcpy(log_str, "[Debug] ");
  LOG_WRITE(log_str);
}
void LogFatal(const char *fmt, ...) {
  char *log_str = (char *)malloc(MAX_LOG_LENGTH);
  memset(log_str, 0, MAX_LOG_LENGTH);
  strcpy(log_str, "[Fatal] ");
  LOG_WRITE(log_str);
}
void LogError(const char *fmt, ...) {
  char *log_str = (char *)malloc(MAX_LOG_LENGTH);
  memset(log_str, 0, MAX_LOG_LENGTH);
  strcpy(log_str, "[Error] ");
  LOG_WRITE(log_str);
}

void LogStopAndWait() {
  quit = 1;
  char *c = (char *)malloc(1);
  *c = 0;
  BlockQueuePushBack(&block_queue, (void *)c);
  pthread_join(tid, NULL);
  BlockQueueFree(&block_queue);
}