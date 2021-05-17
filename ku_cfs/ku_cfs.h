#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct _Node {
    pid_t pid;
    double nicevalue;
    double vruntime;
    struct _Node* next;
}Node;

typedef struct _LinkedList {
    Node *head;
    Node *tail;
}LinkedList;

LinkedList* init() {
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->next = NULL;
    list->head = newNode;
    list->tail = newNode;
    return list;
}

void insert(int pid, double nicevalue, double vruntime, LinkedList *list) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->next = NULL;
    newNode->pid = pid;
    newNode->nicevalue = nicevalue;
    newNode->vruntime = vruntime;

    list->tail->next = newNode;
    list->tail = newNode;
}

void sort(LinkedList *list) {
    Node *tmp = list->head->next;
    
    while(tmp != NULL) {
        Node *tmp2 = list->head->next;
        while(tmp2 != NULL) {
            if(tmp->vruntime < tmp2->vruntime || (tmp->vruntime == tmp2->vruntime && tmp->pid < tmp2->pid)) {
                int pid = tmp->pid;
                double nicevalue = tmp->nicevalue;
                double vruntime = tmp->vruntime;

                tmp->pid = tmp2->pid;
                tmp->nicevalue = tmp2->nicevalue;
                tmp->vruntime = tmp2->vruntime;

                tmp2->pid = pid;
                tmp2->nicevalue = nicevalue;
                tmp2->vruntime = vruntime;
            }
            tmp2 = tmp2->next;
        }
        tmp = tmp->next;
    }
}