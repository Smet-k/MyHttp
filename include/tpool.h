#ifndef TPOOL_H
#define TPOOL_H
#include <pthread.h>

#define MAX_TASKS 1024

typedef struct {
    void (*function)(int);
    int arg;
} Task;

typedef struct {
    Task tasks[MAX_TASKS];
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} TaskQueue;

typedef struct {
    pthread_t *threads;
    int thread_count;
    TaskQueue queue;
} ThreadPool;

ThreadPool* threadpool_create(const int num_threads);
void thread_queue_init(TaskQueue *queue); 
void thread_queue_push(TaskQueue *queue, const Task task);

#endif