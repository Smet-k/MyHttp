#include "tpool.h"
#include <stdio.h>
#include <stdlib.h>

void thread_queue_push(TaskQueue* queue, Task task);
static void* thread_do_work(void* arg);

void thread_queue_init(TaskQueue* queue) {
    queue->count = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
}

ThreadPool* threadpool_create(int num_threads){
    ThreadPool* tp = malloc(sizeof(ThreadPool));
    if (!tp) return NULL;

    tp->thread_count = num_threads;

    thread_queue_init(&tp->queue);
    
    tp->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!tp->threads) {
        free(tp);
        return NULL;
    }

    for(int i = 0;i < num_threads;i++){
        if(pthread_create(&tp->threads[i], NULL, &thread_do_work, tp) != 0){
            perror("Failed to create the thread");
            free(tp->threads);
            free(tp);
            return NULL;
        }
    }

    return tp;
}

static void* thread_do_work(void* arg) {
    ThreadPool* tp = (ThreadPool*)arg;

    while (1) {
        Task task;
        pthread_mutex_lock(&tp->queue.lock);

        while (tp->queue.count == 0) {
            pthread_cond_wait(&tp->queue.not_empty, &tp->queue.lock);
        }

        task = tp->queue.tasks[0];
        for (int i = 0; i < tp->queue.count - 1; i++) {
            tp->queue.tasks[i] = tp->queue.tasks[i + 1];
        }
        tp->queue.count--;
        pthread_mutex_unlock(&tp->queue.lock);

        task.function(task.arg);
    }

    return NULL;
}

void thread_queue_push(TaskQueue* queue, Task task) {
    pthread_mutex_lock(&queue->lock);
    queue->tasks[queue->count] = task;
    queue->count++;
    pthread_mutex_unlock(&queue->lock);
    pthread_cond_signal(&queue->not_empty);
}



