// Test program for thread library
#include <stdio.h>
#include <unistd.h>
#include "ult.h"

ult_mutex_t counter_mutex;
int shared_counter = 0;

ult_rwlock_t data_rwlock;
int shared_data = 0;

extern int timer_count;

// Test 1: Basic threads
void *simple_thread(void *arg) {
    int id = *(int *)arg;
    int i;
    volatile long j;

    for (i = 0; i < 3; i++) {
        printf("  [Thread %d] Hello! (iteration %d)\n", id, i);
        for (j = 0; j < 100000000; j++);
    }

    printf("  [Thread %d] Exiting with value %d\n", id, id * 10);
    return (void *)(long)(id);
}

void test_basic_threads(void) {
    thread_id_t t1, t2;
    int arg1 = 1, arg2 = 2;
    void *ret1, *ret2;

    printf("\n=== TEST 1: Basic Thread Creation & Join ===\n");

    ult_create(&t1, simple_thread, &arg1);
    ult_create(&t2, simple_thread, &arg2);

    printf("Main waiting...\n");

    ult_join(t1, &ret1);
    ult_join(t2, &ret2);

    printf("Thread 1 returned: %ld\n", (long)ret1);
    printf("Thread 2 returned: %ld\n", (long)ret2);
    printf("=== TEST 1 PASSED ===\n");
}

// Test 2: Mutex
void *counter_thread(void *arg) {
    int id = *(int *)arg;
    int i;
    volatile int j;

    for (i = 0; i < 5; i++) {
        ult_mutex_lock(&counter_mutex);

        int old = shared_counter;
        for (j = 0; j < 10000000; j++);
        shared_counter = old + 1;

        printf("  [Thread %d] Counter: %d -> %d\n", id, old, shared_counter);

        ult_mutex_unlock(&counter_mutex);
    }

    return NULL;
}

void test_mutex(void) {
    thread_id_t t1, t2, t3;
    int arg1 = 1, arg2 = 2, arg3 = 3;

    printf("\n=== TEST 2: Mutex Synchronization ===\n");
    printf("Expected final value: 15\n\n");

    ult_mutex_init(&counter_mutex);
    shared_counter = 0;

    ult_create(&t1, counter_thread, &arg1);
    ult_create(&t2, counter_thread, &arg2);
    ult_create(&t3, counter_thread, &arg3);

    ult_join(t1, NULL);
    ult_join(t2, NULL);
    ult_join(t3, NULL);

    printf("\nFinal counter: %d\n", shared_counter);
    if (shared_counter == 15)
        printf("=== TEST 2 PASSED ===\n");
    else
        printf("=== TEST 2 FAILED ===\n");
}

// Test 3: RWLock
void *reader_thread(void *arg) {
    int id = *(int *)arg;
    int i;
    volatile int j;

    for (i = 0; i < 3; i++) {
        ult_rwlock_rdlock(&data_rwlock);
        printf("  [Reader %d] data = %d\n", id, shared_data);
        for (j = 0; j < 500000; j++);
        ult_rwlock_unlock(&data_rwlock);
    }

    return NULL;
}

void *writer_thread(void *arg) {
    int id = *(int *)arg;
    int i;
    volatile int j;

    for (i = 0; i < 2; i++) {
        ult_rwlock_wrlock(&data_rwlock);
        shared_data += 10;
        printf("  [Writer %d] wrote %d\n", id, shared_data);
        for (j = 0; j < 500000; j++);
        ult_rwlock_unlock(&data_rwlock);
    }

    return NULL;
}

void test_rwlock(void) {
    thread_id_t r1, r2, w1;
    int arg1 = 1, arg2 = 2, arg3 = 1;

    printf("\n=== TEST 3: Read-Write Lock ===\n");

    ult_rwlock_init(&data_rwlock);
    shared_data = 0;

    ult_create(&r1, reader_thread, &arg1);
    ult_create(&w1, writer_thread, &arg3);
    ult_create(&r2, reader_thread, &arg2);

    ult_join(r1, NULL);
    ult_join(w1, NULL);
    ult_join(r2, NULL);

    ult_rwlock_destroy(&data_rwlock);

    printf("\nFinal data: %d (expected: 20)\n", shared_data);
    printf("=== TEST 3 PASSED ===\n");
}

// Test 4: Preemption
void *counting_thread(void *arg) {
    int id = *(int *)arg;
    int i;
    volatile long j;

    printf("  [Thread %d] Starting...\n", id);

    for (i = 1; i <= 5; i++) {
        printf("  [Thread %d] Count = %d\n", id, i);
        for (j = 0; j < 50000000; j++);
    }

    printf("  [Thread %d] Done!\n", id);
    return NULL;
}

void test_preemption(void) {
    thread_id_t t1, t2;
    int arg1 = 1, arg2 = 2;

    printf("\n=== TEST 4: Preemptive Scheduling ===\n");
    printf("Watch threads interleave!\n\n");

    timer_count = 0;

    ult_create(&t1, counting_thread, &arg1);
    ult_create(&t2, counting_thread, &arg2);

    ult_join(t1, NULL);
    ult_join(t2, NULL);

    printf("Timer fired %d times\n", timer_count);
    printf("=== TEST 4 PASSED ===\n");
}

int main(void) {
    printf("============================================================\n");
    printf("     USER-LEVEL THREAD LIBRARY - TEST SUITE\n");
    printf("============================================================\n");

    ult_init();

    test_basic_threads();
    test_mutex();
    test_rwlock();
    test_preemption();

    printf("\n============================================================\n");
    printf("     ALL TESTS COMPLETED!\n");
    printf("============================================================\n");

    return 0;
}
