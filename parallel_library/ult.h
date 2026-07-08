// User-level thread library header
#ifndef ULT_H
#define ULT_H

#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_THREADS     64
#define STACK_SIZE      (64 * 1024)
#define TIME_QUANTUM_US 5000

typedef int thread_id_t;

typedef enum {
    THREAD_UNUSED,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct {
    thread_id_t     id;
    ucontext_t      context;
    char           *stack;
    thread_state_t  state;
    void           *retval;
} tcb_t;

typedef struct {
    int         initialized;
    int         locked;
    thread_id_t owner;
} ult_mutex_t;

typedef struct {
    int         initialized;
    int         readers;
    int         writer;
    int         waiting_writers;
    thread_id_t writer_owner;
    int         reader_owners[MAX_THREADS];
} ult_rwlock_t;

// Thread API
void ult_init(void);
int ult_create(thread_id_t *thread, void *(*start_func)(void *), void *arg);
int ult_join(thread_id_t thread, void **retval);
void ult_exit(void *retval);
thread_id_t ult_self(void);

// Mutex API
int ult_mutex_init(ult_mutex_t *mutex);
int ult_mutex_lock(ult_mutex_t *mutex);
int ult_mutex_unlock(ult_mutex_t *mutex);

// RWLock API
int ult_rwlock_init(ult_rwlock_t *rwlock);
int ult_rwlock_rdlock(ult_rwlock_t *rwlock);
int ult_rwlock_wrlock(ult_rwlock_t *rwlock);
int ult_rwlock_unlock(ult_rwlock_t *rwlock);
int ult_rwlock_destroy(ult_rwlock_t *rwlock);

#endif
