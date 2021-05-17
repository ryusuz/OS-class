#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "ku_cfs.h"

LinkedList *ready;
int count;

void handler(int sig) {
    int i;
    count--;                                //1초가 흘렀으므로 1 감소

    Node *least = ready->head->next;
    
    kill(least->pid, SIGSTOP);
    least->vruntime += least->nicevalue;

    sort(ready);
    kill(ready->head->next->pid, SIGCONT);

    if(count == 0 ) {                   //주어진 타임슬라이스가 모두 지나면 종료
        Node *tmp = ready->head->next;

        while(tmp != NULL) {
            kill(tmp->pid, SIGKILL);     //자식에게 종료시그널 보냄

            Node *freeNode = tmp;
            tmp = tmp->next;
            free(freeNode);
        }
        free(ready->head);
        free(ready);                    //레디큐 프리
        exit(0); 
    }
}

void timer() {
    struct itimerval timer;
    timer.it_value.tv_sec = 1;
    timer.it_interval.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_usec = 0;
    
    kill(ready->head->next->pid, SIGCONT);
    setitimer(ITIMER_REAL, &timer, NULL);

}

void call_handler() {
    struct sigaction sact;
    memset(&sact, 0, sizeof(sact));
    sact.sa_handler = &handler;
    sigaction(SIGALRM, &sact, NULL);
}


int main(int argc, char* argv[]) {
    double nicevalue[5] = { 0.64, 0.8, 1.0, 1.25, 1.5625 };
    int i, j;
    int order = 0;
    char atoz[2];
    int pid;
    count = atoi(argv[6]);

    ready = init(); //레디큐 생성

    for(i = 1; i < 6; i++) {
        for(j = 0; j < atoi(argv[i]); j++) {
            pid = fork();
            atoz[0] = 'A' + order;
            atoz[1] = '\0';

            if(pid == 0) {                       //자식프로세스 생성
                execl("./ku_app", "./ku_app", atoz, NULL);
            }
            else {
                insert(pid, nicevalue[i-1], 0, ready);  
                order++;
            }
        }
    }

    sleep(5);

    call_handler();

    timer(); 

    while(1);
}


