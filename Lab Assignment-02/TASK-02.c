#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

int waiting_students = 0;         
int served_students = 0;        
int st_sleeping = 1;              
int students_created = 0;       


sem_t waiting_chairs;              
sem_t st_semaphore;               
pthread_mutex_t mutex;            
pthread_cond_t st_condition;      


void* student_thread(void* arg) {
    int student_id = *(int*)arg;
    free(arg);
    

    if (sem_trywait(&waiting_chairs) == 0) {
        pthread_mutex_lock(&mutex);
        waiting_students++;
        printf("Student %d started waiting for consultation\n", student_id);
        
        if (st_sleeping) {
            st_sleeping = 0;
            pthread_cond_signal(&st_condition);
        }
        pthread_mutex_unlock(&mutex);
        
        sem_wait(&st_semaphore);
        
        pthread_mutex_lock(&mutex);
        waiting_students--;
        printf("A waiting student started getting consultation\n");
        printf("Number of students now waiting: %d\n", waiting_students);
        printf("ST giving consultation\n");
        printf("Student %d is getting consultation\n", student_id);
        pthread_mutex_unlock(&mutex);
        
        usleep(rand() % 500000 + 300000); 
        
        pthread_mutex_lock(&mutex);
        served_students++;
        printf("Student %d finished getting consultation and left\n", student_id);
        printf("Number of served students: %d\n", served_students);
        pthread_mutex_unlock(&mutex);
        
        sem_post(&waiting_chairs);
        sem_post(&st_semaphore);
        
    } else {
        pthread_mutex_lock(&mutex);
        printf("No chairs remaining in lobby. Student %d Leaving.....\n", student_id);
        served_students++;
        printf("Student %d finished getting consultation and left\n", student_id);
        printf("Number of served students: %d\n", served_students);
        pthread_mutex_unlock(&mutex);
    }
    
    return NULL;
}

void* st_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        
        if (served_students >= 10) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        while (waiting_students == 0 && served_students < 10) {
            st_sleeping = 1;
            pthread_cond_wait(&st_condition, &mutex);
        }
        
        if (served_students >= 10) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        st_sleeping = 0;
        pthread_mutex_unlock(&mutex);
        

        usleep(50000);
    }
    
    return NULL;
}

int main() {
    srand(time(NULL));

    sem_init(&waiting_chairs, 0, 3);      
    sem_init(&st_semaphore, 0, 1);        
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&st_condition, NULL);

    pthread_t st_tid;
    pthread_create(&st_tid, NULL, st_thread, NULL);
    

    pthread_t student_threads[10];
    for (int i = 0; i < 10; i++) {
        int* student_id = malloc(sizeof(int));
        *student_id = i;
        
        pthread_create(&student_threads[i], NULL, student_thread, student_id);
        

        usleep(rand() % 200000 + 50000);
        
        pthread_mutex_lock(&mutex);
        students_created++;
        pthread_mutex_unlock(&mutex);
    }
    

    for (int i = 0; i < 10; i++) {
        pthread_join(student_threads[i], NULL);
    }
    
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&st_condition);
    pthread_mutex_unlock(&mutex);
    
    pthread_join(st_tid, NULL);
    
    sem_destroy(&waiting_chairs);
    sem_destroy(&st_semaphore);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&st_condition);
    
    return 0;
}
