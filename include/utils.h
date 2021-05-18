#ifndef LS_UTILS_H
#define LS_UTILS_H

#include <pthread.h>
#include <stddef.h>

#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })

typedef struct {
  void **array;
  int size;
  int capacity;
} DArray;
void DArrayInit(DArray *arr, int initialSize);
void DArrayPushBack(DArray *arr, void *value);
void DArraySwap(DArray *arr1, DArray *arr2);

void *DArrayGet(DArray *arr1, int index);
void DArrayClear(DArray *arr1);
void **DArrayStart(DArray *arr);
int DArraySize(DArray *arr);
void DArrayFree(DArray *arr);

typedef struct {
  DArray data;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} BlockQueue;
/**
 * @brief 初始化一个阻塞队列
 *
 * @param queue
 * @param size 队列初始大小
 */
void BlockQueueInit(BlockQueue *queue, int size);
void BlockQueuePushBack(BlockQueue *queue, void *value);
void BlockQueueWaitAndSwap(BlockQueue *queue, DArray *array);
void BlockQueueFree(BlockQueue *queue);
#endif