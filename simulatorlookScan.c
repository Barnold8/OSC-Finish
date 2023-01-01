#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include "coursework.h"
#include "linkedlist.h"

int head = 0;
LinkedList LQueue = LINKED_LIST_INITIALIZER;
LinkedList RQueue = LINKED_LIST_INITIALIZER;

int getListLen(LinkedList *l){

    Element *e = getHead(*l);
    int x = 0;

    while(e != NULL){
        x++;
        e = getNext(e);
    }
    return x;
}

void printLL(LinkedList L){

    Element* e = getHead(L);
    while(e != NULL){
        Process *p = (Process *)e->pData;
        printf("TRACK: %d \n",p->iTrack);
        e = getNext(e);
    }

}


void queue_sort(LinkedList* L, int reverse){ // based on ITRACK
    Element* head = getHead(*L);
    
    while(head != NULL){
        Element* subHead = getNext(head);
        
        while(subHead != NULL){
            Process * headP    = (Process* )head->pData;
            Process * subHeadP = (Process* )subHead->pData;
            Process * pTemp    = (Process* )subHead->pData;
            if(reverse == 1){
                    if(headP->iTrack < subHeadP->iTrack){
                    pTemp = head->pData;
                    head->pData = subHead->pData;
                    subHead->pData = pTemp;
                }
            }else{
                    if(headP->iTrack > subHeadP->iTrack){
                    pTemp = head->pData;
                    head->pData = subHead->pData;
                    subHead->pData = pTemp;
                }
            }

            subHead = getNext(subHead);
        }
        head = getNext(head);
    }
}



Element * look_scan(LinkedList* L, char* direction){ // sorts incoming queue accordingly 

    head = 50; 
    int seek_count = 0;
    int running = 2;
    int distance;
    int currTrack;
    LinkedList SeekQueue = LINKED_LIST_INITIALIZER;

    Element* e = getHead(*L);
    if(e == NULL){
        return NULL;
    }
    // Process *phead = (Process *)e->pData;

    while(e != NULL){
        Process *ptemp = (Process *)e->pData;
        
        if(ptemp->iTrack < head){
            addLast(ptemp,&LQueue);
        }
        if(ptemp->iTrack > head){
            addLast(ptemp,&RQueue);
        }
       
        e = getNext(e);
        removeFirst(L);
    }



    queue_sort(&LQueue,1);
    queue_sort(&RQueue,0);
    
    while(running--){ // directions need fixing i think 
        
        if(strcmp(direction, "left") == 0){ 
            
            Element * lHead = getHead(LQueue);
            
            while(lHead != NULL){
                
                Process * p = (Process*)lHead->pData;

                //printf("Left queue adding %d\n",p->iTrack);

                addLast(p,&SeekQueue);
                

                distance = abs(p->iTrack - head);

                seek_count += distance;

                lHead = getNext(lHead);
            
                head = p->iTrack;
                removeFirst(&LQueue);
                
            }
            direction = "right";

        }else if(strcmp(direction, "right") == 0){
                    
            Element * rHead = getHead(RQueue);
          
            while(rHead != NULL){
                
                Process * p = (Process*)rHead->pData;

              //  printf("Right queue adding %d\n",p->iTrack);

                distance = abs(p->iTrack - head);

                addLast(p,&SeekQueue);
            
                seek_count += distance;

                rHead = getNext(rHead);
                

                head = p->iTrack;
                removeFirst(&RQueue);
            }
            direction = "left";
        }

    }
    // printf("Seek %d\n",seek_count);
    // printf("Head is %d\n",head);
    // printf("LL %d | RR %d\n",getListLen(&LQueue),getListLen(&RQueue));
    // printf("LL + RR = %d | Seek %d\n",getListLen(&LQueue)+getListLen(&RQueue),getListLen(&SeekQueue));
    // printf("Seek sequence:\n");
    // Element* h = getHead(SeekQueue);
    // Process* p;
    // while(h != NULL){
    //     p = (Process *) h->pData;

    //     printf("%d\n",p->iTrack);

    //     h = getNext(h);
    // }
    return getHead(SeekQueue); // return the head of the new queue
}


//===================== DEBUG CODE 

int main(){


    LinkedList L = LINKED_LIST_INITIALIZER;
    int testArray[] = {16, 60, 149, 153, 52, 157, 184, 193, 141, 56, 127, 123, 174, 88, 20, 86, 93, 55, 175, 13, 150, 63, 143, 71, 98, 121, 92, 70, 55, 57, 81, 179};
    int otherTest[] = { 176, 79, 34, 60, 92, 11, 41, 114 };
    for(int i = 0; i < 8; i++){
        Process *p = generateProcess(i);
        p->iTrack = otherTest[i];
        addLast(p,&L);
    }

    // printf("\n===== SWAP TEST =====\n");
    // printf("Track: %d\n",p1->iTrack);
    // printf("Track: %d\n",p2->iTrack);
    // printf("Swapping...\n");
    // swapPdat(&p1, &p2);
    // printf("Track: %d\n",p1->iTrack);
    // printf("Track: %d\n",p2->iTrack);
    // printf("\n===== SWAP TEST =====\n");


    // printf("\n===== Sort TEST =====\n");
    printf("\nBefore:\n");
    printLL(L);
    L.pHead = look_scan(&L,"right");
    printf("\n\nAfter:\n");
    printLL(L);
    // printf("\n===== Sort TEST =====\n");

    
    Element * e = getHead(L);
    while(e != NULL){

        Process * p = (Process *) e->pData;
        e = getNext(e);
        removeFirst(&L);
        
        free(p);
    }

    return 0;
}

//===================== DEBUG CODE 

    // // ==================== DEBUG CODE
    // printf("ALL QUEUE....\n");
    // printLL(*L);
    // printf("L: BEFORE...\n");
    // printf("LL %d\n",getListLen(&LQueue));
    // printLL(LQueue);
    // printf("L: AFTER...\n");
    // queue_sort(&LQueue,1);
    // printLL(LQueue);
    // printf("R: BEFORE...\n");
    // printf("RR %d\n",getListLen(&RQueue));
    // printLL(RQueue);
    // printf("R: AFTER...\n");
    // queue_sort(&RQueue,0);
    //printLL(RQueue);    
    // // ==================== DEBUG CODE