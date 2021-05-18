#include "log.h"
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>

static pthread_t tid[4];
static void (*log_funcs[3])(const char *, ...) = {LogDebug, LogError, LogFatal};

static void *threadFunc(void *args) {
  for (int i = 0; i < 20; ++i) {
    srand(time(NULL) + i);
    log_funcs[rand() % 3]("haha\n");
  }
  return NULL;
}
int main() {
  LogInit("test.log");
  for (int i = 0; i < 4; ++i) {
    pthread_create(&tid[i], NULL, threadFunc, NULL);
  }
  for (int i = 0; i < 4; ++i) {
    pthread_join(tid[i], NULL);
  }
  LogStopAndWait();
}