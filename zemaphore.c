#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "common_threads.h"
#include "zemaphore.h"

typedef struct _rwlock_t {
    Zem_t writelock;
    Zem_t readlock;
    Zem_t mutex;
    int AR;       // number of Active Readers
    int AW;       // number of Active Writers 
    int WR;       // number of Waiting Readers
    int WW;       // number of Waiting Writers
} rwlock_t;

void rwlock_init(rwlock_t *rw) {
    rw->AR = 0;
    rw->AW = 0;
    rw->WR = 0;
    rw->WW = 0;
    Zem_init(&rw->mutex, 1); 
    Zem_init(&rw->writelock, 0); 
    Zem_init(&rw->readlock, 0); 
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    Zem_wait(&rw->mutex);
    while((rw->AW + rw->WW) > 0){
        rw->WR++;
        Zem_post(&rw->mutex);
        Zem_wait(&rw->readlock);
        Zem_wait(&rw->mutex);
    }
    rw->AR++;
    Zem_post(&rw->mutex);
}

void rwlock_release_readlock(rwlock_t *rw) {
    Zem_wait(&rw->mutex);
    rw->AR--;
    if (rw->AR == 0 && rw->WW > 0) {
        Zem_post(&rw->writelock);
        rw->WW--;
    }
    Zem_post(&rw->mutex);
}

void rwlock_acquire_writelock(rwlock_t *rw) {
    Zem_wait(&rw->mutex);
    while((rw->AW + rw->AR) > 0){
        rw->WW++;
        Zem_post(&rw->mutex);
        Zem_wait(&rw->writelock);
        Zem_wait(&rw->mutex);
    }
    rw->AW++;
    Zem_post(&rw->mutex);
}

void rwlock_release_writelock(rwlock_t *rw) {
    Zem_wait(&rw->mutex);
    rw->AW--;
    if (rw->WW > 0) {
        Zem_post(&rw->writelock);
        rw->WW--;
    }
    else if(rw->WR >0) { 
        while(rw->WR > 0) { // broadcasting
            Zem_post(&rw->readlock);
            rw->WR--;
        }
    }
    Zem_post(&rw->mutex);
}


//
// Don't change the code below (just use it!) But fix it if bugs are found!
// 

int loops;
int DB = 0;

typedef struct {
    int thread_id;
    int job_type;         // 0: reader, 1: writer
    int arrival_delay;
    int running_time;
} arg_t;

sem_t print_lock;

#define TAB 25
void space(int s) {
    Sem_wait(&print_lock);
    int i;
    for (i = 0; i < s * TAB; i++)
        printf(" ");
}

void space_end() {
    Sem_post(&print_lock);
}

#define TICK usleep(100000)   // 1/100초 단위로 하고 싶으면 usleep(10000)
rwlock_t rwlock;

void *reader(void *arg) {
    arg_t *args = (arg_t *)arg;
    
    TICK;
    rwlock_acquire_readlock(&rwlock);
   // start reading
    int i;
    for (i = 0; i < args->running_time-1; i++) {
        TICK;
        space(args->thread_id);printf("reading %d of %d\n", i, args->running_time); space_end();
    }
    TICK;
    space(args->thread_id); printf("reading %d of %d, DB is %d\n", i, args->running_time, DB); space_end();
    // end reading
    TICK;
    rwlock_release_readlock(&rwlock);
    return NULL;
}

void *writer(void *arg) {
    arg_t *args = (arg_t *)arg;

    TICK;
    rwlock_acquire_writelock(&rwlock);
   // start writing
    int i;
    for (i = 0; i < args->running_time-1; i++) {
        TICK;
        space(args->thread_id); printf("writing %d of %d\n", i, args->running_time); space_end();
    }
    TICK;
    DB++;
    space(args->thread_id); printf("writing %d of %d, DB is %d\n", i, args->running_time, DB); space_end();
    // end writing 
    TICK;
    rwlock_release_writelock(&rwlock);

    return NULL;
}

void *worker(void *arg) {
    arg_t *args = (arg_t *)arg;
    int i;
    for (i = 0; i < args->arrival_delay; i++) {
        TICK;
        space(args->thread_id); printf("arrival delay %d of %d\n", i, args->arrival_delay); space_end();
    }
    if (args->job_type == 0) reader(arg);
    else if (args->job_type == 1) writer(arg);
    else {
        space(args->thread_id); printf("Unknown job %d\n",args->thread_id); space_end();
    }
    return NULL;
}

#define MAX_WORKERS 10

int main(int argc, char *argv[]) {
    
    // command line argument로 공급 받거나  
    // 예: -n 6 -a 0:0:5,0:1:8,1:3:4,0:5:7,1:6:2,0:7:4    또는   -n 6 -a r:0:5,r:1:8,w:3:4,r:5:7,w:6:2,r:7:4
    // 아래 코드에서 for-loop을 풀고 배열 a에 직접 쓰는 방법으로 worker 세트를 구성한다.
    
    int num_workers;
    pthread_t p[MAX_WORKERS];
    arg_t a[MAX_WORKERS];   
    
    rwlock_init(&rwlock);
    Sem_init(&print_lock, 1);

    char is_valid_cmd = 0;

    if(strcmp(argv[1], "-n") == 0) {
        num_workers = atoi(argv[2]);
        is_valid_cmd = 1;
    }
    if(is_valid_cmd) {
        if(strcmp(argv[3], "-a") != 0) { printf("invalid input\n"); return 0; }
        char *token;
        int MAXARG = sizeof(a[0]) / sizeof(a[0].arrival_delay) - 1;
        int t_id = 0; 
        int arg_cnt = 0; // argument 개수만큼 채우다가 최대치에 도달하면 초기화하고 쓰레드 아이디 증가  

        token = strtok(argv[4], ",:");  
        while(token != NULL) {
            a[t_id].thread_id = t_id;
            if(arg_cnt == 0) a[t_id].job_type = atoi(token);
            else if(arg_cnt == 1) a[t_id].arrival_delay = atoi(token);
            else if(arg_cnt == 2) a[t_id].running_time = atoi(token);
            // else if(arg_cnt == 3) ...
            arg_cnt++;
            if(arg_cnt == MAXARG) { arg_cnt=0; t_id++; }
            
            token = strtok(NULL, ",:");
        }
    }
    else { printf("invalid input\n"); return 0; }

    printf("begin\n");   
    printf(" ... heading  ...  \n");   // a[]의 정보를 반영해서 헤딩 라인을 출력
    
    for (int i = 0; i < num_workers; i++)
        Pthread_create(&p[i], NULL, worker, &a[i]);
    
    for (int i = 0; i < num_workers; i++)
        Pthread_join(p[i], NULL);

    printf("end: DB %d\n", DB);

    return 0;
}