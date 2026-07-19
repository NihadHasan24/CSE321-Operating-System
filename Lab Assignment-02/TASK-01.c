#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    long long *fib_sequence;
    int n;
    int search_count;
    int *search_indices;
    int *search_results;
    int sequence_ready;
} SharedData;

SharedData shared_data;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* fibonacci_generator(void* arg) {
    printf("Thread 1: Generating Fibonacci sequence...\n");
    
    shared_data.fib_sequence = (long long*)malloc((shared_data.n + 1) * sizeof(long long));
    
    if (shared_data.n >= 0) shared_data.fib_sequence[0] = 0;
    if (shared_data.n >= 1) shared_data.fib_sequence[1] = 1;
    
    for (int i = 2; i <= shared_data.n; i++) {
        shared_data.fib_sequence[i] = shared_data.fib_sequence[i-1] + shared_data.fib_sequence[i-2];
    }
    
    for (int i = 0; i <= shared_data.n; i++) {
        printf("a[%d] = %lld\n", i, shared_data.fib_sequence[i]);
    }
    
    pthread_mutex_lock(&mutex);
    shared_data.sequence_ready = 1;
    pthread_mutex_unlock(&mutex);
    
    return NULL;
}

void* fibonacci_searcher(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        if (shared_data.sequence_ready) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
        usleep(1000);
    }
    
    printf("Thread 2: Searching for indices...\n");

    for (int i = 0; i < shared_data.search_count; i++) {
        int search_index = shared_data.search_indices[i];

        if (search_index >= 0 && search_index <= shared_data.n) {
            shared_data.search_results[i] = shared_data.fib_sequence[search_index];
        } else {
            shared_data.search_results[i] = -1;
        }
    }
    
    return NULL;
}

int main() {
    pthread_t thread1, thread2;
    
    shared_data.sequence_ready = 0;
    
    printf("Enter the term of fibonacci sequence:\n");
    scanf("%d", &shared_data.n);
    
    printf("How many numbers you are willing to search?:\n");
    scanf("%d", &shared_data.search_count);

    shared_data.search_indices = (int*)malloc(shared_data.search_count * sizeof(int));
    shared_data.search_results = (int*)malloc(shared_data.search_count * sizeof(int));

    for (int i = 0; i < shared_data.search_count; i++) {
        printf("Enter search %d:\n", i + 1);
        scanf("%d", &shared_data.search_indices[i]);
    }
    

    pthread_create(&thread1, NULL, fibonacci_generator, NULL);
    pthread_create(&thread2, NULL, fibonacci_searcher, NULL);
    
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    for (int i = 0; i < shared_data.search_count; i++) {
        printf("result of search #%d = %d\n", i + 1, shared_data.search_results[i]);
    }
    
    free(shared_data.fib_sequence);
    free(shared_data.search_indices);
    free(shared_data.search_results);
    pthread_mutex_destroy(&mutex);
    
    return 0;
}