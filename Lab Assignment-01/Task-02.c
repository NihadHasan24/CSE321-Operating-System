#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t child_pid, grandchild_pid;
    
    child_pid = fork();
    
    if (child_pid == 0) {

        grandchild_pid = fork();
        
        if (grandchild_pid == 0) {
            printf("I am grandchild\n");
            exit(0);
        }
        else if (grandchild_pid > 0) {
            wait(NULL);
            printf("I am child\n");
            exit(0);
        }
        else {
            perror("Fork failed for grandchild");
            exit(1);
        }
    }
    else if (child_pid > 0) {
        wait(NULL);
        printf("I am parent\n");
    }
    else {
        perror("Fork failed for child");
        exit(1);
    }
    
    return 0;
}
