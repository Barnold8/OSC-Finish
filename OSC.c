// SIM7

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include "coursework.h"
#include "linkedlist.h"



LinkedList tProcessQueue     = LINKED_LIST_INITIALIZER;
LinkedList rProcessQueue     = LINKED_LIST_INITIALIZER;
LinkedList dProcessQueue     = LINKED_LIST_INITIALIZER;
LinkedList rIOProcessQueue   = LINKED_LIST_INITIALIZER;
LinkedList deviceQueue       = LINKED_LIST_INITIALIZER;
LinkedList SeekQueue         = LINKED_LIST_INITIALIZER;
LinkedList READQueue         = LINKED_LIST_INITIALIZER;
LinkedList WRITEQueue        = LINKED_LIST_INITIALIZER;

int head                     = 0;
int PID_count                = 0;
int rQueueCount              = 0;
int end                      = 0;
int proc_size                = 0;
int term_size                = 0;
int blocked_size             = 0;
int killed                   = 0;
int RIO                      = 0;
int read_count               = 0;
int write_count              = 0;
int seek_count               = 0;
int pool_table               [SIZE_OF_PROCESS_TABLE];
int runningThreads           [MAX_CONCURRENT_PROCESSES] = {0}; // global int array to say what thread indexs are in use

Process* process_table[SIZE_OF_PROCESS_TABLE];

sem_t Generator; // max concurrent threads
sem_t Killer;
sem_t isGen;
sem_t tableACCESS;
sem_t dQueue;
sem_t readers;
sem_t readTry;
sem_t WriteSerial;
sem_t RIOsync;
sem_t ReadWait; // Used by writer to force wait reader 
sem_t Resource; // Only allow one thread to access resource at any given time

char *states[4] = {"READY","RUNNING","BLOCKED","TERMINATED"};
char *read_write[2] = {"READ","WRITE"};


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
        if(ptemp->iTrack == head){
            addLast(ptemp,&LQueue);
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
  
        if(killed >= NUMBER_OF_PROCESSES){    
            printf("Simulator finished\n");
            sem_post(&dQueue);
            break;
        }
     
        sem_wait(&isGen);
        
        Element* head;

        if(RIO >= 1){
            head = getHead(rIOProcessQueue);
        }else{
            head = getHead(rProcessQueue);
        }

        if(head != NULL){   

            Process *p = (Process *)head->pData;
            
            if(RIO >= 1){                       // RIO WORKS
                RIO--;
                sem_post(&dQueue);
                removeFirst(&rIOProcessQueue);
                printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",getListLen(&rIOProcessQueue), p->iPID);
            }else{
                removeFirst(&rProcessQueue);
                printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",term_size, p->iPID);
            }
         
            sem_post(&isGen);

            runPreemptiveProcess(p,1,1); // Itrack due to last bool param is -1 at points, breaking the lookscan algo. This is because it sets head to -1 and the process is -1. 
                                         // So nothing can be done and it deadlocks. Nethertheless, its not what i need to do 
            
            printf("SIMULATOR - CPU %ld: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",CPUID_Long, p->iPID,p->iBurstTime,p->iRemainingBurstTime);

            if(p->iState == 3 && p->iRemainingBurstTime > 0){ 
            
                switch (p->iDeviceType)
                {
                case READERS_WRITERS: // goes to device controller
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = 0 , Type = %s]\n",p->iPID,read_write[p->iRW]);
                    printf("QUEUE - ADDED: [Queue = Device 0 - %s, Size = %d, PID = %d]\n",read_write[p->iRW],getListLen(&deviceQueue),p->iPID);
                    //printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",proc_size, p->iPID);
                    addLast(p,&deviceQueue);
                    break;
                case HARD_DRIVE:     // goes to disk controller
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = Hard Drive, Type = %s]\n",p->iPID,read_write[p->iRW]);
                    printf("QUEUE - ADDED: [Queue = Hard Drive, Size = %d, PID = %d]\n",getListLen(&dProcessQueue),p->iPID);
                    //printf("QUEUE - Removed: [Queue = READY , size = %d, PID = %d]\n",proc_size, p->iPID);
                    addLast(p,&dProcessQueue);
                    break;
                default:
                    break;
                }

               // addLast(p,&dProcessQueue);
                proc_size--;
                blocked_size++;
                
                sem_post(&dQueue); 
                
                printf("QUEUE - ADDED: [Queue = Blocked , size = %d, PID = %d]\n",blocked_size, p->iPID);

            }
            else if(p->iRemainingBurstTime > 0){ // if theres remaining time on the process, add it back to ready queue 
                addLast(p,&rProcessQueue);    
                printf("QUEUE - Added: [Queue = %s, size = %d, PID = %d]\n",states[p->iState-1],proc_size, p->iPID);
            }
            else{ 

                proc_size--;
                term_size++;

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
            sem_post(&dQueue);
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
       
        sem_wait(&Killer);
        Element* e = getHead(tProcessQueue);    


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
       
        if(killed >= NUMBER_OF_PROCESSES){
            printf("%d Tracks crossed (total)\n",seek_count);
            printf("HARD DRIVE: Finished\n");
            break;}
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

void * reader(){

    while(1){
       
        if(killed >= NUMBER_OF_PROCESSES){
            printf("Reader finished\n");
            break;
        }
        sem_wait(&ReadWait);
    
        Element * e = getHead(READQueue);
        
        if(e != NULL){
            sem_wait(&Resource);
            read_count++;
            sem_post(&Resource);
            printf("READER - Device 0: Read request received\n");
            printf("READER - Device 0: Reading (%d parallel reader(s))\n",read_count);
            Process * p = (Process*) e->pData;
            removeFirst(&READQueue);
            sem_post(&ReadWait);
            simulateIO(p);
            
            RIO++;
            proc_size++;
            blocked_size--;
           
            read_count--;
            sem_wait(&Resource);
            addLast(p,&rIOProcessQueue);
            sem_post(&Resource);
            
            
        }else{
            sem_post(&ReadWait);
        }
        

      
    }
}

void * writer(){

    while(1){
        if(killed >= NUMBER_OF_PROCESSES){
            printf("Writer finished\n");
            break;
        }
        
        sem_wait(&WriteSerial);              // Ensures serialisation of writer threads (only one writer thread can access at any given time)
        Element * e = getHead(WRITEQueue);
       
        if(e != NULL){

            write_count++;
            if(write_count == 1){sem_wait(&ReadWait);}      // A lock on readers, stopping them from reading when writing 
            Process * p = (Process*) e->pData;

            removeFirst(&WRITEQueue);
            simulateIO(p);
        
            sem_wait(&Resource);
            RIO++;
            proc_size++;
            blocked_size--;
            sem_post(&Resource);

            addLast(p,&rIOProcessQueue);
            write_count--;
            if(write_count == 0){sem_post(&ReadWait);}
        }
        sem_post(&WriteSerial);
    }
}



void * deviceController(){ // ensure RIO is incremented to avoid deadlocks

    // Let n readers read, when a writer wants to write,stop adding to reader queue, finish reader queue, run waiting writers, go back to readers. 
    // Multiple readers can read at any given time, put a semaphore around critical section queue
    // When a writer is active, all readers sleep. 
    // Have a semaphore for the critical section for the device queue
    // Parallelism, need multiple threads for reading
    // serial, binary semaphore on writer. 
    
    while(1){
        if(killed >= NUMBER_OF_PROCESSES){
            printf("device controller finished\n");
            break;
        }
        Element* e = getHead(deviceQueue);
        
        if(e != NULL){
            Process * p = (Process*) e->pData;
            removeFirst(&deviceQueue);

      
            switch (p->iRW) // This switch delegates IO processes to their respective reader writer queue for processing 
            {
            case WRITE: // Gen thread for writing
                //printf("WRITING....\n");
                addLast(p,&WRITEQueue);

                break;
            case READ: // Gen thread for reading
                //printf("READING....\n");
           
                addLast(p,&READQueue);
     
                break;
            default:
                break;
            }
        }
    }
}

int main(){

   // inits
   pthread_t threads[NUMBER_OF_CPUS];
   pthread_t writer_threads[NUMBER_OF_WRITERS];
   pthread_t reader_threads[NUMBER_OF_READERS];
   pthread_t gen;
   pthread_t kill;
   pthread_t IO_Access;
   pthread_t devController;
   
   void* (*funcs[7])() = {genList,procSim,termList,IOAccess,deviceController,reader,writer};
   
   sem_init(&Generator,0,MAX_CONCURRENT_PROCESSES); // sleeps generator thread when process queue is full of max conc proc
   sem_init(&Killer,0,0);                           // Used to wake up killer semaphore
   sem_init(&isGen,0,1);   
   sem_init(&tableACCESS,0,1);  
   sem_init(&dQueue,0,1);
   sem_init(&WriteSerial,0,1);
   sem_init(&ReadWait,0,1);
   sem_init(&Resource,0,1);
  

   // Create

   for(int i = 0; i < SIZE_OF_PROCESS_TABLE;i++){process_table[i] = NULL;}
   for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++){pool_table[i] = -1;}

   pthread_create(&gen,NULL,funcs[0],NULL);

   for(long int i = 0; i < NUMBER_OF_CPUS; i++){
      
      pthread_create(&threads[i],NULL,funcs[1],(void*) i); 
      
   }   
   
   for(long int i = 0; i < NUMBER_OF_WRITERS; i++){
      
      pthread_create(&writer_threads[i],NULL,funcs[5],NULL); 
      
   }

    for(long int i = 0; i < NUMBER_OF_READERS; i++){
      
      pthread_create(&reader_threads[i],NULL,funcs[6],NULL); 
      
   }


   pthread_create(&kill,NULL,funcs[2],NULL);
   pthread_create(&devController,NULL,funcs[4],NULL);
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
    for(long int i = 0; i < NUMBER_OF_READERS; i++){
     if(pthread_join(reader_threads[i],NULL) != 0){
        perror("Cant run thread :( :(\n");
         exit(1);
     }
   }

   for(long int i = 0; i < NUMBER_OF_WRITERS; i++){
     if(pthread_join(writer_threads[i],NULL) != 0){
        perror("Cant run thread :( :(\n");
         exit(1);
     }
   }

    if(pthread_join(kill,NULL) != 0){
     perror("Cant run thread :( :(\n");
      exit(1);
   }

    if(pthread_join(devController,NULL) != 0){
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

////============================= DEBUG BLOCK

// int a,b,c,d,_e,f;

// sem_getvalue(&Generator, &a);
// sem_getvalue(&Killer, &b);
// sem_getvalue(&isGen, &c);
// sem_getvalue(&tableACCESS, &d);
// sem_getvalue(&dQueue, &_e);
// sem_getvalue(&WriteSerial, &f);

// printf("\nSemaphore values:\n\n\tGenerator:\t%d\n\tKiller:\t%d\n\tisGen:\t%d\n\ttableACCESS:\t%d\n\tdQueue:\t%d\n\tWriteSerial:\t%d\n\n",a,b,c,d,_e,f);


// printf("\nQueue values:\n\n\ttProcessQueue: %d\n\trProcessQueue: %d\n\tdProcessQueue: %d\n\trIOProcessQueue: %d\n\n\n",
//     getListLen(&tProcessQueue),getListLen(&rProcessQueue),getListLen(&dProcessQueue),getListLen(&rIOProcessQueue));

// printf("\nGlobal integer values:\n\n\thead: %d\n\tPID_count: %d\n\trQueueCount: %d\n\tend: %d\n\tproc_size: %d\n\tterm_size: %d\n\tblocked_size: %d\n\tkilled: %d\n\tRIO: %d\n\n",
// head,PID_count,rQueueCount,end,proc_size,term_size,blocked_size,killed,RIO);

//============================= DEBUG BLOCK
