#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

struct msg {
    long int type;
    char txt[6];
};

int main() {
    key_t key;
    int msgid;
    struct msg message;
    pid_t pid_otp, pid_mail;

    key = ftok("progfile", 65);

    msgid = msgget(key, 0666 | IPC_CREAT);
    if(msgid == -1) {
        perror("msgget");
        exit(1);
    }

    char workspace[10];
    printf("Please enter the workspace name:\n");
    scanf("%s", workspace);

    if(strcmp(workspace, "cse321") != 0) {
        printf("Invalid workspace name\n");
        // Remove queue before exit
        msgctl(msgid, IPC_RMID, NULL);
        exit(0);
    }

    message.type = 1;
    strncpy(message.txt, workspace, sizeof(message.txt));
    message.txt[sizeof(message.txt)] = '\0';

    if(msgsnd(msgid, &message, sizeof(message.txt), 0) == -1) {
        perror("msgsnd login workspace");
        exit(1);
    }
    printf("Workspace name sent to otp generator from log in: %s\n\n", message.txt);

    pid_otp = fork();
    if(pid_otp < 0) {
        perror("fork otp generator");
        exit(1);
    }

    if(pid_otp == 0) {
        if(msgrcv(msgid, &message, sizeof(message.txt), 1, 0) == -1) {
            perror("msgrcv otp generator");
            exit(1);
        }
        printf("OTP generator received workspace name from log in: %s\n\n", message.txt);

        pid_t otp = getpid();
        snprintf(message.txt, sizeof(message.txt), "%d", otp);

        message.type = 2;
        if(msgsnd(msgid, &message, sizeof(message.txt), 0) == -1) {
            perror("msgsnd otp to login");
            exit(1);
        }
        printf("OTP sent to log in from OTP generator: %s\n", message.txt);

        message.type = 3;
        if(msgsnd(msgid, &message, sizeof(message.txt), 0) == -1) {
            perror("msgsnd otp to mail");
            exit(1);
        }
        printf("OTP sent to mail from OTP generator: %s\n", message.txt);

        pid_mail = fork();
        if(pid_mail < 0) {
            perror("fork mail");
            exit(1);
        }
        if(pid_mail == 0) {
            if(msgrcv(msgid, &message, sizeof(message.txt), 3, 0) == -1) {
                perror("msgrcv mail");
                exit(1);
            }
            printf("Mail received OTP from OTP generator: %s\n", message.txt);

            message.type = 4;
            if(msgsnd(msgid, &message, sizeof(message.txt), 0) == -1) {
                perror("msgsnd mail to login");
                exit(1);
            }
            printf("OTP sent to log in from mail: %s\n", message.txt);
            exit(0);
        }

        wait(NULL);
        exit(0);
    }

    wait(NULL);
    
    if(msgrcv(msgid, &message, sizeof(message.txt), 2, 0) == -1) {
        perror("msgrcv login from otp");
        exit(1);
    }
    printf("Log in received OTP from OTP generator: %s\n", message.txt);

    char otp1[6];
    strncpy(otp1, message.txt, sizeof(otp1));
    otp1[sizeof(otp1)-1] = '\0';

    if(msgrcv(msgid, &message, sizeof(message.txt), 4, 0) == -1) {
        perror("msgrcv login from mail");
        exit(1);
    }
    printf("Log in received OTP from mail: %s\n", message.txt);

    char otp2[6];
    strncpy(otp2, message.txt, sizeof(otp2));
    otp2[sizeof(otp2)-1] = '\0';

    if(strcmp(otp1, otp2) == 0) {
        printf("OTP Verified\n");
    } else {
        printf("OTP Incorrect\n");
    }

    msgctl(msgid, IPC_RMID, NULL);

    return 0;
}
