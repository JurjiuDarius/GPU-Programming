// User-level thread library implementation
#include "ult.h"

static tcb_t threads[MAX_THREADS];
static thread_id_t current_thread = 0;
static sigset_t block_alarm;

int timer_count = 0;

static void block_signals(void) {
    sigprocmask(SIG_BLOCK, &block_alarm, NULL);
}

static void unblock_signals(void) {
    sigprocmask(SIG_UNBLOCK, &block_alarm, NULL);
}

// Round-robin scheduler
static void schedule(void) {
    thread_id_t prev = current_thread;
    thread_id_t next;
    int i;

    if (threads[prev].state == THREAD_RUNNING)
        threads[prev].state = THREAD_READY;

    // find next ready thread
    next = prev;
    for (i = 0; i < MAX_THREADS; i++) {
        next = (next + 1) % MAX_THREADS;
        if (threads[next].state == THREAD_READY)
            break;
    }

    if (threads[next].state != THREAD_READY) {
        for (i = 0; i < MAX_THREADS; i++) {
            if (threads[i].state == THREAD_BLOCKED) {
                fprintf(stderr, "Deadlock detected!\n");
                exit(1);
            }
        }
        exit(0);
    }

    current_thread = next;
    threads[next].state = THREAD_RUNNING;
    swapcontext(&threads[prev].context, &threads[next].context);
}

static void timer_handler(int sig) {
    (void)sig;
    timer_count++;
    schedule();
}

static void thread_wrapper(void *(*func)(void *), void *arg) {
    void *result = func(arg);
    ult_exit(result);
}

void ult_init(void) {
    struct sigaction sa;
    struct itimerval timer;

    sigemptyset(&block_alarm);
    sigaddset(&block_alarm, SIGALRM);

    // main thread = thread 0
    threads[0].id = 0;
    threads[0].state = THREAD_RUNNING;
    threads[0].stack = NULL;
    getcontext(&threads[0].context);
    current_thread = 0;

    // timer handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    // start timer
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TIME_QUANTUM_US;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = TIME_QUANTUM_US;
    setitimer(ITIMER_REAL, &timer, NULL);
}

int ult_create(thread_id_t *thread, void *(*start_func)(void *), void *arg) {
    int i;
    thread_id_t new_id = -1;

    block_signals();

    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_UNUSED) {
            new_id = i;
            break;
        }
    }

    if (new_id < 0) {
        unblock_signals();
        return -1;
    }

    threads[new_id].id = new_id;
    threads[new_id].state = THREAD_READY;
    threads[new_id].retval = NULL;
    threads[new_id].stack = malloc(STACK_SIZE);

    if (!threads[new_id].stack) {
        threads[new_id].state = THREAD_UNUSED;
        unblock_signals();
        return -1;
    }

    getcontext(&threads[new_id].context);
    threads[new_id].context.uc_stack.ss_sp = threads[new_id].stack;
    threads[new_id].context.uc_stack.ss_size = STACK_SIZE;
    sigemptyset(&threads[new_id].context.uc_sigmask); // important! unblock signals for new thread

    makecontext(&threads[new_id].context, (void (*)(void))thread_wrapper, 2, start_func, arg);

    *thread = new_id;
    unblock_signals();
    return 0;
}

int ult_join(thread_id_t thread, void **retval) {
    block_signals();

    if (thread < 0 || thread >= MAX_THREADS || threads[thread].state == THREAD_UNUSED) {
        unblock_signals();
        return -1;
    }

    if (thread == current_thread) {
        unblock_signals();
        return -1;
    }

    while (threads[thread].state != THREAD_TERMINATED) {
        threads[current_thread].state = THREAD_BLOCKED;
        unblock_signals();
        schedule();
        block_signals();
    }

    if (retval)
        *retval = threads[thread].retval;

    if (threads[thread].stack) {
        free(threads[thread].stack);
        threads[thread].stack = NULL;
    }
    threads[thread].state = THREAD_UNUSED;

    unblock_signals();
    return 0;
}

void ult_exit(void *retval) {
    int i;
    block_signals();

    threads[current_thread].retval = retval;
    threads[current_thread].state = THREAD_TERMINATED;

    // wake up blocked threads
    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_BLOCKED)
            threads[i].state = THREAD_READY;
    }

    unblock_signals();
    schedule();
}

thread_id_t ult_self(void) {
    return current_thread;
}

// Mutex stuff

int ult_mutex_init(ult_mutex_t *mutex) {
    mutex->initialized = 1;
    mutex->locked = 0;
    mutex->owner = -1;
    return 0;
}

int ult_mutex_lock(ult_mutex_t *mutex) {
    if (!mutex->initialized) return -1;

    block_signals();
    while (mutex->locked && mutex->owner != current_thread) {
        threads[current_thread].state = THREAD_BLOCKED;
        unblock_signals();
        schedule();
        block_signals();
    }
    mutex->locked = 1;
    mutex->owner = current_thread;
    unblock_signals();
    return 0;
}

int ult_mutex_unlock(ult_mutex_t *mutex) {
    int i;
    if (!mutex->initialized) return -1;

    block_signals();
    if (mutex->owner != current_thread) {
        unblock_signals();
        return -1;
    }

    mutex->locked = 0;
    mutex->owner = -1;

    // wake all blocked threads
    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_BLOCKED)
            threads[i].state = THREAD_READY;
    }

    unblock_signals();
    schedule();  // yield to let other threads run
    return 0;
}

// RWLock stuff

int ult_rwlock_init(ult_rwlock_t *rwlock) {
    int i;
    rwlock->initialized = 1;
    rwlock->readers = 0;
    rwlock->writer = 0;
    rwlock->waiting_writers = 0;
    rwlock->writer_owner = -1;
    for (i = 0; i < MAX_THREADS; i++)
        rwlock->reader_owners[i] = 0;
    return 0;
}

int ult_rwlock_rdlock(ult_rwlock_t *rwlock) {
    if (!rwlock->initialized) return -1;

    block_signals();
    while (rwlock->writer || rwlock->waiting_writers > 0) {
        threads[current_thread].state = THREAD_BLOCKED;
        unblock_signals();
        schedule();
        block_signals();
    }
    rwlock->readers++;
    rwlock->reader_owners[current_thread] = 1;
    unblock_signals();
    return 0;
}

int ult_rwlock_wrlock(ult_rwlock_t *rwlock) {
    if (!rwlock->initialized) return -1;

    block_signals();
    rwlock->waiting_writers++;
    while (rwlock->readers > 0 || rwlock->writer) {
        threads[current_thread].state = THREAD_BLOCKED;
        unblock_signals();
        schedule();
        block_signals();
    }
    rwlock->waiting_writers--;
    rwlock->writer = 1;
    rwlock->writer_owner = current_thread;
    unblock_signals();
    return 0;
}

int ult_rwlock_unlock(ult_rwlock_t *rwlock) {
    int i;
    if (!rwlock->initialized) return -1;

    block_signals();
    if (rwlock->writer && rwlock->writer_owner == current_thread) {
        rwlock->writer = 0;
        rwlock->writer_owner = -1;
    } else if (rwlock->reader_owners[current_thread]) {
        rwlock->reader_owners[current_thread] = 0;
        rwlock->readers--;
    } else {
        unblock_signals();
        return -1;
    }

    // wake all
    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_BLOCKED)
            threads[i].state = THREAD_READY;
    }

    unblock_signals();
    schedule();  // yield to let other threads run
    return 0;
}

int ult_rwlock_destroy(ult_rwlock_t *rwlock) {
    if (!rwlock->initialized) return -1;

    block_signals();
    if (rwlock->readers > 0 || rwlock->writer) {
        unblock_signals();
        return -1;
    }
    rwlock->initialized = 0;
    unblock_signals();
    return 0;
}
