#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>

struct shared {
    char sel[100];
    int b;
};

int main() {
    int shmid;
    struct shared *shared_mem;
    int pipefd[2];
    pid_t pid;
    key_t key = 1234; 
    
    shmid = shmget(key, sizeof(struct shared), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    
    shared_mem = (struct shared *)shmat(shmid, NULL, 0);
    if (shared_mem == (struct shared *)-1) {
        perror("shmat failed");
        exit(1);
    }
    
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    printf("Provide Your Input From Given Options:\n");
    printf("1. Type a to Add Money\n");
    printf("2. Type w to Withdraw Money\n");
    printf("3. Type c to Check Balance\n");
    
    scanf("%s", shared_mem->sel);

    shared_mem->b = 1000;
    
    printf("Your selection: %s\n", shared_mem->sel);
    
    pid = fork();
    
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }
    
    if (pid == 0) {
        close(pipefd[0]); 
        
        char selection = shared_mem->sel[0];
        
        if (selection == 'a') {
            int amount;
            printf("Enter amount to be added:\n");
            scanf("%d", &amount);
            
            if (amount > 0) {
                shared_mem->b += amount;
                printf("Balance added successfully\nUpdated balance after addition:\n%d\n", shared_mem->b);
            } else {
                printf("Adding failed, Invalid amount\n");
            }
        }
        else if (selection == 'w') {
            int amount;
            printf("Enter amount to be withdrawn:\n");
            scanf("%d", &amount);
            
            if (amount > 0 && amount <= shared_mem->b) {
                shared_mem->b -= amount;
                printf("Balance withdrawn successfully\nUpdated balance after withdrawal:\n%d\n", shared_mem->b);
            } else {
                printf("Withdrawal failed, Invalid amount\n");
            }
        }
        else if (selection == 'c') {
            printf("Your current balance is:\n%d\n", shared_mem->b);
        }
        else {
            printf("Invalid selection\n");
        }
        
        char message[] = "Thank you for using";
        write(pipefd[1], message, strlen(message) + 1);
        close(pipefd[1]); 
        
        shmdt(shared_mem);
        exit(0);
    }
    else {
        close(pipefd[1]); 
        
        wait(NULL);
        char buffer[100];
        read(pipefd[0], buffer, sizeof(buffer));
        printf("%s\n", buffer);
        close(pipefd[0]); 
        
        shmdt(shared_mem);
        shmctl(shmid, IPC_RMID, NULL);
    }
    
    return 0;
}