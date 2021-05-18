#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void DArrayInit(DArray *arr, int initialSize) {
  assert(initialSize > 0);
  arr->array = (void **)malloc(sizeof(void *) * initialSize);
  arr->size = 0;
  arr->capacity = initialSize;
}

static void resize(DArray *arr) {
  assert(arr->size == arr->capacity);
  void **oldArr = arr->array;
  arr->array = (void **)malloc(sizeof(void *) * 2 * arr->capacity);
  arr->capacity *= 2;
  memcpy(arr->array, oldArr, sizeof(void *) * arr->size);
  free(oldArr);
}

void DArrayPushBack(DArray *arr, void *value) {
  assert(arr->capacity >= arr->size);
  if (arr->capacity == arr->size) {
    resize(arr);
  }
  arr->array[arr->size++] = value;
}

void DArraySwap(DArray *arr1, DArray *arr2) {
  DArray tmp = *arr1;
  arr1->array = arr2->array;
  arr1->capacity = arr2->capacity;
  arr1->size = arr2->size;

  arr2->array = tmp.array;
  arr2->capacity = tmp.capacity;
  arr2->size = tmp.size;
}

void *DArrayGet(DArray *arr, int index) {
  assert(index >= 0 && index < arr->size);
  return arr->array[index];
}
void DArrayClear(DArray *arr) { arr->size = 0; }
void **DArrayStart(DArray *arr) { return arr->array; }
int DArraySize(DArray *arr) { return arr->size; }
void DArrayFree(DArray *arr) {
  free(arr->array);
  arr->size = 0;
}

void BlockQueueInit(BlockQueue *queue, int size) {
  DArrayInit(&queue->data, size);
  pthread_cond_init(&queue->cond, NULL);
  pthread_mutex_init(&queue->mutex, NULL);
}
void BlockQueuePushBack(BlockQueue *queue, void *value) {
  pthread_mutex_lock(&queue->mutex);
  DArrayPushBack(&queue->data, value);
  pthread_mutex_unlock(&queue->mutex);
  pthread_cond_signal(&queue->cond);
}
void BlockQueueWaitAndSwap(BlockQueue *queue, DArray *array) {
  assert(DArraySize(array) == 0);
  pthread_mutex_lock(&queue->mutex);
  while (DArraySize(&queue->data) == 0) {
    pthread_cond_wait(&queue->cond, &queue->mutex);
  }
  DArraySwap(array, &queue->data);
  pthread_mutex_unlock(&queue->mutex);
}
void BlockQueueFree(BlockQueue *queue) {
  DArrayFree(&queue->data);
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
}
