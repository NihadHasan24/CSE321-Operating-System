#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {

    int arr[] = {15, 8, 23, 4, 42, 7, 19, 36, 11, 28};
    int n = sizeof(arr) / sizeof(arr[0]);
    
    char *args[n + 2];
    args[0] = "./sort";
    
    for (int i = 0; i < n; i++) {
        args[i + 1] = malloc(20);
        sprintf(args[i + 1], "%d", arr[i]);
    }
    args[n + 1] = NULL;
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork failed");
        return 1;
    }
    else if (pid == 0) {

        execv("./sort", args);
        perror("execv failed");
        exit(1);
    }
    else {
        int status;
        wait(&status);

        args[0] = "./oddeven";
        execv("./oddeven", args);
        perror("execv failed");
    }

    return 0;
}
