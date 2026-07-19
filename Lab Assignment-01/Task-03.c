#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/mman.h>

int main() {
    pid_t a, b, c, new_child;
    pid_t original_parent = getpid();
    

    int *process_count = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *process_count = 1;
    

    
    a = fork();
    if (a > 0){
        
        (*process_count)++;
    } 
    b = fork();
    if (b > 0){
        (*process_count)++;
    } 
    c = fork();
    if (c > 0){
        (*process_count)++;
    } 
    if (getpid() % 2 == 1) {
        new_child = fork();
        if (new_child == 0) {

            exit(0);
        } else if (new_child > 0) {
            (*process_count)++;
            wait(NULL);
        }
    }
    if (getpid() == original_parent) {
        while (wait(NULL) > 0);
        printf("Total processes created: %d\n", *process_count);
        munmap(process_count, sizeof(int));
    } else {
        exit(0);
    }
    
    return 0;
}
