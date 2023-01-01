#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include "coursework.h"
#include "linkedlist.h"



LinkedList tProcessQueue = LINKED_LIST_INITIALIZER;
LinkedList rProcessQueue = LINKED_LIST_INITIALIZER;
LinkedList dProcessQueue = LINKED_LIST_INITIALIZER;
LinkedList rIOProcessQueue = LINKED_LIST_INITIALIZER;

LinkedList SeekQueue = LINKED_LIST_INITIALIZER;

int head = 0;
int PID_count = 0;
int rQueueCount = 0;
int end = 0;
int proc_size = 0;
int term_size = 0;
int blocked_size = 0;
int killed = 0;
int RIO = 0;
int pool_table[SIZE_OF_PROCESS_TABLE];

Process* process_table[SIZE_OF_PROCESS_TABLE];

sem_t Generator; // max concurrent threads
sem_t Killer;
sem_t isGen;
sem_t tableACCESS;
sem_t dQueue;

char *states[4] = {"READY","RUNNING","BLOCKED","TERMINATED"};

// PROBLEMS:

    // READYIO messes EVERYTHING UP. Invalid read, invalid write, invalid free. No segfault or aborts though???????
    // Might be fine to continue with and fix up later

    //gets stuck near the end. All queues are empty and ends up deadlocking due to that fact

void printPOOL(){

    for(int i = 0; i < SIZE_OF_PROCESS_TABLE;i++){
        printf("POOL[%d]: %d\n",i,pool_table[i]);
    }

}

void printLL(LinkedList L){

    Element* e = getHead(L);
    while(e != NULL){
        Process *p = (Process *)e->pData;
        printf("TRACK: %d \n",p->iTrack);
        e = getNext(e);
    }

}

int getListLen(LinkedList *l){

    Element *e = getHead(*l);
    int x = 0;

    while(e != NULL){
        x++;
        e = getNext(e);
    }
    return x;
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
    LinkedList LQueue = LINKED_LIST_INITIALIZER;
    LinkedList RQueue = LINKED_LIST_INITIALIZER;
    int seek_count = 0;
    int running = 2;
    int distance;
    int currTrack;


    sem_wait(&dQueue);
    Element* e = getHead(*L);
   

    if(e == NULL){
        sem_post(&dQueue);
        return NULL;
    }

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
        sem_post(&dQueue);
    }

    queue_sort(&LQueue,1);
    queue_sort(&RQueue,0);
    //printf("Left Queue %d | Right queue %d\n",getListLen(&LQueue),getListLen(&RQueue));
    while(running--){ // directions need fixing i think 
       
        if(strcmp(direction, "left") == 0){ // need to reverse this one
            
            Element * lHead = getHead(LQueue);
            
            while(lHead != NULL && getListLen(&LQueue) > 0){
                
                Process * p = (Process*)lHead->pData;

                //printf("Left queue adding %d\n",p->iTrack);
                
                sem_wait(&dQueue);
                addLast(p,&SeekQueue);
                sem_post(&dQueue);
                

                distance = abs(p->iTrack - head);

                seek_count += distance;

                lHead = getNext(lHead);
            
                head = p->iTrack;
                removeFirst(&LQueue);
                
            }
            direction = "right";

        }else if(strcmp(direction, "right") == 0){
                    
            Element * rHead = getHead(RQueue);
          
            while(rHead != NULL && getListLen(&RQueue) > 0){
                
                Process * p = (Process*)rHead->pData;

              //  printf("Right queue adding %d\n",p->iTrack);

                distance = abs(p->iTrack - head);
                sem_wait(&dQueue);
                addLast(p,&SeekQueue);
                sem_post(&dQueue);
            
                seek_count += distance;

                rHead = getNext(rHead);
                

                head = p->iTrack;
                removeFirst(&RQueue);
            }
            direction = "left";
        }

    }
    return getHead(SeekQueue); // return the head of the new queue
}

void* genList(){ // generate process and decrement semaphore 
    
    while(killed != NUMBER_OF_PROCESSES){ // Stops making new processes after predefined maxiumum process count (not same as max councurrent processes)
        if(PID_count >= NUMBER_OF_PROCESSES+5){break;}
        int ID_INDEX;
        sem_wait(&Generator); 
        sem_wait(&isGen);
        sem_wait(&tableACCESS);
        
        int processIndex = -1;

        for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){
            if(pool_table[i] == -1){
                pool_table[i] = 1;
                processIndex = i;
                ID_INDEX = i;
                break;
            }
        }

        Process *p = generateProcess(ID_INDEX);
        addLast(p,&rProcessQueue);
        process_table[ID_INDEX] = p;

        for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){
            if(process_table[i] == NULL){
                process_table[i] = p;
                break;
            }
        }
        sem_post(&tableACCESS);

        PID_count++;
        proc_size++;

        printf("QUEUE - Added: [Queue = %s, size = %d, PID = %d]\n",states[p->iState-1],proc_size,ID_INDEX);
        printf("GENERATOR - ADMITTED: [PID = %d, Initial BurstTime = %d, Remaining BurstTime = %d]\n",ID_INDEX,p->iBurstTime,p->iRemainingBurstTime);
        sem_post(&isGen);
    }
    sem_post(&dQueue);
    printf("Generator finished.\n");
    
}

void * procSim(void * CPUID){ // multiple processes are be picking up the same process. 

    long int rTime, tTime; // response | turnaround
    rTime = tTime = -1;
    long int CPUID_Long = (long int) CPUID;
    while(1){ // runs thread infintely //head != NULL
        //printf("EEEE\n");
  
        if(killed >= NUMBER_OF_PROCESSES){     //end && getListLen(&rProcessQueue) == 0     //proc_size == NUMBER_OF_PROCESSES  <- this is what is needed i think // deadlocks because all the processes arent being simulated...
            printf("Simulator finished\n");
            sem_post(&dQueue);
            break;
        }
     
        sem_wait(&isGen);
        
        Element* head;
        // Element* head = getHead(rProcessQueue);

        if(RIO >= 1){
            head = getHead(rIOProcessQueue);
            //removeFirst(&rIOProcessQueue);
        }else{
            head = getHead(rProcessQueue);
            //removeFirst(&rProcessQueue);
        }

        if(head != NULL){   

            Process *p = (Process *)head->pData;
            
            if(RIO >= 1){                       // RIO WORKS
                RIO--;
                sem_post(&dQueue);
                removeFirst(&rIOProcessQueue);
            }else{
                removeFirst(&rProcessQueue);
            }
            
            //removeFirst(&rProcessQueue);
            sem_post(&isGen);

            runPreemptiveProcess(p,1,0); 
            printf("SIMULATOR - CPU %ld: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",CPUID_Long, p->iPID,p->iBurstTime,p->iRemainingBurstTime);

            if(p->iState == 3 && p->iRemainingBurstTime > 0){
                
                addLast(p,&dProcessQueue);
                proc_size--;
                blocked_size++;
                //printf("============================================================================= ITRACK  BEFORE %d\n",p->iTrack);
                sem_post(&dQueue); 
                printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",term_size, p->iPID);
                printf("QUEUE - ADDED: [Queue = Blocked , size = %d, PID = %d]\n",blocked_size, p->iPID);
                if(p->iDeviceType == 0){
                //SIMULATOR - I/O BLOCKED: [PID = 1, Device = 0, Type = READ]
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = %d, Type = READ]\n",p->iPID,p->iDeviceID);
                }else{
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = %d, Type = WRITE]\n",p->iPID,p->iDeviceID);
                }

            }
            else if(p->iRemainingBurstTime > 0){ // if theres remaining time on the process, add it back to ready queue 
                addLast(p,&rProcessQueue);    
                printf("QUEUE - Added: [Queue = %s, size = %d, PID = %d]\n",states[p->iState-1],proc_size, p->iPID);
            }
            else{ 

                proc_size--;
                term_size++;
                //printf("Removing %d\n",p->iPID); // uncomment for debugging
                rTime = getDifferenceInMilliSeconds(p->oTimeCreated,p->oLastTimeRunning);
                tTime = getDifferenceInMilliSeconds(p->oFirstTimeRunning,p->oLastTimeRunning); 
            
                printf("SIMULATOR - TERMINATED: [PID = %d, ResponseTime = %ld, TurnAroundTime = %ld]\n",
                   p->iPID,rTime,tTime);
                printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",proc_size, p->iPID);
                printf("QUEUE - Added: [Queue = TERMINATED , size = %d, PID = %d]\n",term_size, p->iPID);
              
                addLast(p,&tProcessQueue);
                sem_post(&Killer);
            }
        }else{
            sem_post(&isGen);
        }
          // sem_post(&isGen);
    }
    
}

void * termList(){ // Finishes up but somehow doesnt free every process. 
    
    while(1){

        if(killed >= NUMBER_OF_PROCESSES){

            sem_post(&isGen);
            sem_post(&dQueue); 
            printf("Terminator finished\n");

            break; // ends the last thread
        }
       
        printf("Before simulator killer sem wait\n");
        sem_wait(&Killer);
        Element* e = getHead(tProcessQueue);    
        printf("After simulator killer sem wait\n");

        if(e != NULL){
   
            Process *p = (Process *)e->pData;
        
            printf("QUEUE - Removed: [Queue = TERMINATED , size = %d, PID = %d]\n",term_size, p->iPID);
            printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d]\n",killed+1, p->iPID); // +1 so the elements actually killed aligns with real numbers
            
            killed++;
            term_size--;
            removeFirst(&tProcessQueue);
            
            sem_wait(&tableACCESS);
            pool_table[p->iPID] = -1;
            process_table[p->iPID] = NULL;
            sem_post(&tableACCESS);
            free(p);
            

        }   

        sem_post(&Generator);
    
    }

}

void * IOAccess(){ 

    while(1){
       
        if(killed >= NUMBER_OF_PROCESSES){printf("HARD DRIVE: Finished\n");break;}
        sem_wait(&dQueue); // if no processes, go to sleep
        
        look_scan(&dProcessQueue,"right"); // run look scan algorithm
        Element* e = getHead(SeekQueue);

        while(e != NULL){
          
            Process * p = (Process *)e->pData;
            e = getNext(e);
            printf("READER - Device 0: Read request received\n"); //printf("READER - Device %d: Read request received\n",p->iDeviceID);

            simulateIO(p);

            printf("HARD DRIVE: reading track %d\n",p->iTrack);
            
            proc_size++;
            blocked_size--;
            printf("QUEUE - Removed: [Queue = BLOCKED , size = %d, PID = %d]\n",blocked_size, p->iPID);
            printf("QUEUE - ADDED: [Queue = READY , size = %d, PID = %d]\n",term_size, p->iPID);
            
            addLast(p,&rIOProcessQueue);
            removeFirst(&SeekQueue);
            RIO++;
        }

    }

}


int main(){

   // inits
   pthread_t threads[NUMBER_OF_CPUS];
   pthread_t gen;
   pthread_t kill;
   pthread_t IO_Access;
   void* (*funcs[4])() = {genList,procSim,termList,IOAccess};
   
   sem_init(&Generator,0,MAX_CONCURRENT_PROCESSES); // sleeps generator thread when process queue is full of max conc proc
   sem_init(&Killer,0,0);                           // Used to wake up killer semaphore
   sem_init(&isGen,0,1);   
   sem_init(&tableACCESS,0,1);  
   sem_init(&dQueue,0,1); 
   // Create

   for(int i = 0; i < SIZE_OF_PROCESS_TABLE;i++){process_table[i] = NULL;}
   for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){pool_table[i] = -1;}

   pthread_create(&gen,NULL,funcs[0],NULL);

   for(long int i = 0; i < NUMBER_OF_CPUS; i++){
      
      pthread_create(&threads[i],NULL,funcs[1],(void*) i); 
      
   }

   pthread_create(&kill,NULL,funcs[2],NULL);
   pthread_create(&IO_Access,NULL,funcs[3],NULL);

   //  join
   if(pthread_join(gen,NULL) != 0){
     perror("Cant run thread :( :(\n");
      exit(1);
   }

   for(long int i = 0; i < NUMBER_OF_CPUS; i++){
     if(pthread_join(threads[i],NULL) != 0){
        perror("Cant run thread :( :(\n");
         exit(1);
     }
   }

    if(pthread_join(kill,NULL) != 0){
     perror("Cant run thread :( :(\n");
      exit(1);
   }

    if(pthread_join(IO_Access,NULL) != 0){
     perror("Cant run thread :( :(\n");
     exit(1);
   }

    // printf("End of main\n"); // uncomment for debugging
    // printf("Ready queue %d | termination queue %d\n",getListLen(&rProcessQueue),getListLen(&tProcessQueue)); // uncomment for debugging

}

//============================= DEBUG BLOCK

// int a,b,c,d,_e;

// sem_getvalue(&Generator, &a);
// sem_getvalue(&Killer, &b);
// sem_getvalue(&isGen, &c);
// sem_getvalue(&tableACCESS, &d);
// sem_getvalue(&dQueue, &_e);

// printf("\nSemaphore values:\n\n\tGenerator:\t%d\n\tKiller:\t%d\n\tisGen:\t%d\n\ttableACCESS:\t%d\n\tdQueue:\t%d\n",a,b,c,d,_e);


// printf("\nQueue values:\n\n\ttProcessQueue: %d\n\trProcessQueue: %d\n\tdProcessQueue: %d\n\trIOProcessQueue: %d\n\tLQueue: %d\n\tRQueue: %d\n\tSeekQueue: %d\n\n",
//     getListLen(&tProcessQueue),getListLen(&rProcessQueue),getListLen(&dProcessQueue),getListLen(&rIOProcessQueue),getListLen(&LQueue),getListLen(&RQueue),getListLen(&SeekQueue));

// printf("\nGlobal integer values:\n\n\thead: %d\n\tPID_count: %d\n\trQueueCount: %d\n\tend: %d\n\tproc_size: %d\n\tterm_size: %d\n\tblocked_size: %d\n\tkilled: %d\n\tRIO: %d\n\n",
// head,PID_count,rQueueCount,end,proc_size,term_size,blocked_size,killed,RIO);

//============================= DEBUG BLOCK



// #include <pthread.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <string.h>
// #include <semaphore.h>
// #include "coursework.h"
// #include "linkedlist.h"



// LinkedList tProcessQueue = LINKED_LIST_INITIALIZER;
// LinkedList rProcessQueue = LINKED_LIST_INITIALIZER;
// LinkedList dProcessQueue = LINKED_LIST_INITIALIZER;
// LinkedList rIOProcessQueue = LINKED_LIST_INITIALIZER;

// int PID_count = 0;
// int rQueueCount = 0;
// int end = 0;
// int proc_size = 0;
// int term_size = 0;
// int blocked_size = 0;
// int killed = 0;
// int RIO = 0;
// int head = 0;
// int direction = 1; // start reading right
// int distance = 0;
// int pool_table[SIZE_OF_PROCESS_TABLE];

// Process* process_table[SIZE_OF_PROCESS_TABLE];
// sem_t Generator; // max concurrent threads
// sem_t Killer;
// sem_t isGen;
// sem_t tableACCESS;
// sem_t dQueue;
// sem_t RIOsem;
// sem_t test;

// char *states[4] = {"READY","RUNNING","BLOCKED","TERMINATED"};

// // PROBLEMS:

//     // READYIO messes EVERYTHING UP. Invalid read, invalid write, invalid free. No segfault or aborts though???????
//     // Might be fine to continue with and fix up later

// //TODO

//     // make a IO ready queue                                                                                      -Done
//     // simulate blocked process via simulateIO                                                                    -Done
//     // add disk process to ready io queue                                                                         -Done
//     // in simulator check for ready io processes, if they exist, take from that queue rather than the ready queue -Done



// void printPOOL(){

//     for(int i = 0; i < SIZE_OF_PROCESS_TABLE;i++){
//         printf("POOL[%d]: %d\n",i,pool_table[i]);
//     }

// }

// int getListLen(LinkedList *l){

//     Element *e = getHead(*l);
//     int x = 0;

//     while(e != NULL){
//         x++;
//         e = getNext(e);
//     }
//     return x;
// }

// void* genList(){ // generate process and decrement semaphore 
    
//     while(PID_count != NUMBER_OF_PROCESSES){ // Stops making new processes after predefined maxiumum process count (not same as max councurrent processes)
//         int ID_INDEX;
//         sem_wait(&Generator); 
//         sem_wait(&isGen);
//         sem_wait(&tableACCESS);
        
//         int processIndex = -1;

//         for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){
//             if(pool_table[i] == -1){
//                 pool_table[i] = 1;
//                 processIndex = i;
//                 ID_INDEX = i;
//                 break;
//             }
//         }

//         Process *p = generateProcess(ID_INDEX);
//         addLast(p,&rProcessQueue);
//         process_table[ID_INDEX] = p;


//         // printf("==================== ADDING PROCESS  %d ====================\n",ID_INDEX); // uncomment for debugging
        
//         for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){
//             if(process_table[i] == NULL){
//                 process_table[i] = p;
//                 break;
//             }
//         }
//         sem_post(&tableACCESS);

//         PID_count++;
//         proc_size++;

//         printf("QUEUE - Added: [Queue = %s, size = %d, PID = %d]\n",states[p->iState-1],proc_size,ID_INDEX);
//         printf("GENERATOR - ADMITTED: [PID = %d, Initial BurstTime = %d, Remaining BurstTime = %d]\n",ID_INDEX,p->iBurstTime,p->iRemainingBurstTime);
//         sem_post(&isGen);
//     }
//     printf("Generator finished.\n");
    
// }

// void * procSim(void * CPUID){ // multiple processes are be picking up the same process. 

//     long int rTime, tTime; // response | turnaround
//     rTime = tTime = -1;

//     long int CPUID_Long = (long int) CPUID;
//     while(1){ // runs thread infintely //head != NULL

//         if(killed >= NUMBER_OF_PROCESSES){     //end && getListLen(&rProcessQueue) == 0     //proc_size == NUMBER_OF_PROCESSES  <- this is what is needed i think // deadlocks because all the processes arent being simulated...
//             printf("Simulator finished\n");
//             break;
//         }

//         sem_wait(&isGen);
//         Element* head;
//         // Element* head = getHead(rProcessQueue);

//         if(RIO >= 1){
//             head = getHead(rIOProcessQueue);
//             //removeFirst(&rIOProcessQueue);
//         }else{
//             head = getHead(rProcessQueue);
//             //removeFirst(&rProcessQueue);
//         }

//         if(head != NULL){   

//             Process *p = (Process *)head->pData;
            
//             if(RIO >= 1){                       // RIO WORKS
//                 removeFirst(&rIOProcessQueue);
//                 RIO--;
//             }else{
//                 removeFirst(&rProcessQueue);
//             }
            
//             //removeFirst(&rProcessQueue);
//             sem_post(&isGen);

//             runPreemptiveProcess(p,1,0); 
//             printf("SIMULATOR - CPU %ld: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",CPUID_Long, p->iPID,p->iBurstTime,p->iRemainingBurstTime);

//             if(p->iState == 3 && p->iRemainingBurstTime > 0){
                
//                 addLast(p,&dProcessQueue);
//                 proc_size--;
//                 blocked_size++;
//                 //printf("============================================================================= ITRACK  BEFORE %d\n",p->iTrack);
//                 sem_post(&dQueue); 
//                 printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",term_size, p->iPID);
//                 printf("QUEUE - ADDED: [Queue = Blocked , size = %d, PID = %d]\n",blocked_size, p->iPID);
//                 if(p->iDeviceType == 0){
//                 //SIMULATOR - I/O BLOCKED: [PID = 1, Device = 0, Type = READ]
//                     printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = %d, Type = READ]\n",p->iPID,p->iDeviceID);
//                 }else{
//                     printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = %d, Type = WRITE]\n",p->iPID,p->iDeviceID);
//                 }

//             }
//             else if(p->iRemainingBurstTime > 0){ // if theres remaining time on the process, add it back to ready queue 
//                 addLast(p,&rProcessQueue);    
//                 printf("QUEUE - Added: [Queue = %s, size = %d, PID = %d]\n",states[p->iState-1],proc_size, p->iPID);
//             }
//             else{ 

//                 proc_size--;
//                 term_size++;
//                 //printf("Removing %d\n",p->iPID); // uncomment for debugging
//                 rTime = getDifferenceInMilliSeconds(p->oTimeCreated,p->oLastTimeRunning);
//                 tTime = getDifferenceInMilliSeconds(p->oFirstTimeRunning,p->oLastTimeRunning); 
            
//                 printf("SIMULATOR - TERMINATED: [PID = %d, ResponseTime = %ld, TurnAroundTime = %ld]\n",
//                    p->iPID,rTime,tTime);
//                 printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",proc_size, p->iPID);
//                 printf("QUEUE - Added: [Queue = TERMINATED , size = %d, PID = %d]\n",term_size, p->iPID);
              
//                 addLast(p,&tProcessQueue);
//                 sem_post(&Killer);
//             }
//         }else{

//             sem_post(&isGen);
//             sem_post(&dQueue);
//         }
//           // sem_post(&isGen);
//     }
    
// }

// void * termList(){ // Finishes up but somehow doesnt free every process. 
    
//     while(1){
       
//         sem_wait(&Killer);
//         Element* e = getHead(tProcessQueue);    
        
//         if(e != NULL){
   
//             Process *p = (Process *)e->pData;
        
//             printf("QUEUE - Removed: [Queue = TERMINATED , size = %d, PID = %d]\n",term_size, p->iPID);
//             printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d]\n",killed+1, p->iPID); // +1 so the elements actually killed aligns with real numbers
            
//             killed++;
//             term_size--;
//             removeFirst(&tProcessQueue);
            
//             sem_wait(&tableACCESS);
//             pool_table[p->iPID] = -1;
//             process_table[p->iPID] = NULL;
//             sem_post(&tableACCESS);
//             free(p);

//         }   
//             sem_post(&Generator);
//            // sem_post(&isGen);

//         if(killed >= NUMBER_OF_PROCESSES){

//             sem_post(&isGen);
//             sem_post(&dQueue); 
//             printf("Terminator finished\n");
//             end = 1;
//             break; // ends the last thread
//         }
        
//     }

// }


// void queue_sort(LinkedList* L, int reverse){ // based on ITRACK
//     Element* head = getHead(*L);
    
//     while(head != NULL){
//         Element* subHead = getNext(head);
        
//         while(subHead != NULL){
//             Process * headP    = (Process* )head->pData;
//             Process * subHeadP = (Process* )subHead->pData;
//             Process * pTemp    = (Process* )subHead->pData;
//             if(reverse == 1){
//                     if(headP->iTrack < subHeadP->iTrack){
//                     pTemp = head->pData;
//                     head->pData = subHead->pData;
//                     subHead->pData = pTemp;
//                 }
//             }else{
//                     if(headP->iTrack > subHeadP->iTrack){
//                     pTemp = head->pData;
//                     head->pData = subHead->pData;
//                     subHead->pData = pTemp;
//                 }
//             }

//             subHead = getNext(subHead);
//         }
//         head = getNext(head);
//     }
// }

// void * IOAccess(){ 
    
//     LinkedList LQueue = LINKED_LIST_INITIALIZER; // Left queue of the look scan algo
//     LinkedList RQueue = LINKED_LIST_INITIALIZER; // Right queue of the look scan algo

//     while(1){

//         sem_wait(&dQueue); // if no processes, go to sleep
//         Element* e = getHead(dProcessQueue);
        
//         if(e != NULL){
            
//             Process * p = (Process *)e->pData;
//             removeFirst(&dProcessQueue);
//             sem_post(&dQueue); 

//             if(p->iTrack < head){
//                 Element * lhead = getHead(LQueue);
//                 addLast(p,&LQueue);
//             }
//             if(p->iTrack > head){
//                 Element * lhead = getHead(LQueue);
//                 addLast(p,&RQueue);
//             }
        
//             e = getNext(e);
//             //removeFirst(&dProcessQueue);
           
//         }else{sem_post(&dQueue);}

//             queue_sort(&LQueue,1);
//             queue_sort(&RQueue,0);
        
//         if(getListLen(&RQueue) + getListLen(&LQueue) > 0){ // if elements exist in either queue

//             switch(direction){
                
//                 case 0: // head left
//                     if(getListLen(&LQueue) > 0){ // read left
//                         Element* Lhead = getHead(LQueue);
//                         Process* LP = (Process*) Lhead->pData;
//                         simulateIO(LP);
//                         removeFirst(&LQueue);
//                         distance += abs(LP->iTrack-head);
//                         sem_wait(&RIOsem);
//                         addLast(LP, &rIOProcessQueue);
//                         RIO++;
//                         sem_post(&RIOsem);
                        
//                     }
//                     break;
//                 case 1: // head right
//                     if(getListLen(&LQueue) > 0){ // read right
//                         Element* Lhead = getHead(LQueue);
//                         Process* RP = (Process*) Lhead->pData;
//                         simulateIO(RP);
//                         removeFirst(&RQueue);
//                         distance += abs(RP->iTrack-head);
//                         sem_wait(&RIOsem);
//                         addLast(RP, &rIOProcessQueue);
//                         RIO++;
//                         sem_post(&RIOsem);
//                     }
//                     break;
//                 default:
//                     printf("UNEXPECTED INTEGER DIRECTION %d\n",direction);
//                     exit(1);
//                     break;
//             }

//             if(getListLen(&LQueue) == 0 && direction == 0){
//                 direction = 1;
//             }else if(getListLen(&RQueue) == 0 && direction == 1){
//                 direction = 0;
//             }

//         }

//         if(killed >= NUMBER_OF_PROCESSES){printf("HARD DRIVE: Finished\n");break;}
//     }

// }


// int main(){

//    // inits
//    pthread_t threads[NUMBER_OF_CPUS];
//    pthread_t gen;
//    pthread_t kill;
//    pthread_t IO_Access;
//    void* (*funcs[4])() = {genList,procSim,termList,IOAccess};
   
//    sem_init(&Generator,0,MAX_CONCURRENT_PROCESSES); // sleeps generator thread when process queue is full of max conc proc
//    sem_init(&Killer,0,0);                           // Used to wake up killer semaphore
//    sem_init(&isGen,0,1);   
//    sem_init(&tableACCESS,0,1);  
//    sem_init(&dQueue,0,1); 
//    sem_init(&RIOsem,0,1); 
//    // Create

//    for(int i = 0; i < SIZE_OF_PROCESS_TABLE;i++){process_table[i] = NULL;}
//    for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){pool_table[i] = -1;}

//    pthread_create(&gen,NULL,funcs[0],NULL);

//    for(long int i = 0; i < NUMBER_OF_CPUS; i++){
      
//       pthread_create(&threads[i],NULL,funcs[1],(void*) i); 
      
//    }

//    pthread_create(&kill,NULL,funcs[2],NULL);
//    pthread_create(&IO_Access,NULL,funcs[3],NULL);

//    //  join
//    if(pthread_join(gen,NULL) != 0){
//      perror("Cant run thread :( :(\n");
//       exit(1);
//    }

//    for(long int i = 0; i < NUMBER_OF_CPUS; i++){
//      if(pthread_join(threads[i],NULL) != 0){
//         perror("Cant run thread :( :(\n");
//          exit(1);
//      }
//    }

//     if(pthread_join(kill,NULL) != 0){
//      perror("Cant run thread :( :(\n");
//       exit(1);
//    }

//     if(pthread_join(IO_Access,NULL) != 0){
//      perror("Cant run thread :( :(\n");
//      exit(1);
//    }

// }