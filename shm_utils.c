#include "shm_utils.h"

/*
 * Shared data structure
 * */

/* Shared memory data init */
int shm_goods;
batch_of_goods* goods;

int shm_ship;
ship* ships;

int shm_harb;
harbour* harbours;

int shm_context;
context * ctx;

/* Semaphore data */

int sem_shm; /* Semaphore for shared memory (1 for now) but one for each structure later */

int sem_ship; /* Semaphore for ships */
int sem_harb; /* Semaphore for harbours */
int sem_context; /* Semaphore for context */
int sem_good; /* Semaphore for goods */

void initialize_ipc(context *ctxx) {
    key_t shm_context_key, shm_harb_key, shm_ship_key, shm_goods_key, sem_shm_key;

    /* Create the keys for shared objects by leveraging ftok  */
    shm_context_key = ftok(FTOK_PATH, 'c');
    shm_harb_key = ftok(FTOK_PATH, 'h');
    shm_ship_key = ftok(FTOK_PATH, 's');
    shm_goods_key = ftok(FTOK_PATH, 'g');
    sem_shm_key = ftok(FTOK_PATH, 'm');

    printf("[IPC] Initializing IPCs resources \n");
    if ((shm_context  = shmget(shm_context_key, sizeof(context), IPC_CREAT | IPC_EXCL | SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((ctx     = shmat(shm_context, NULL, 0)) == (void *) -1)
        TEST_ERRORM

    /*Copy ctxx into ctx*/
    memcpy(ctx, ctxx, sizeof(context));
    /*print ctx->SO_porti*/
    printf("[IPC] Creating %d harbours in shared memory \n", ctx->SO_porti);
    if ((shm_harb       = shmget(shm_harb_key, sizeof(harbour) * ctx->SO_porti, IPC_CREAT | IPC_EXCL | SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((shm_ship       = shmget(shm_ship_key , sizeof(ship)  * ctx->SO_navi, IPC_CREAT | IPC_EXCL | SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((shm_goods      = shmget(shm_goods_key, sizeof(batch_of_goods) * ctx->SO_merci, IPC_CREAT | IPC_EXCL | SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((sem_shm        = semget(sem_shm_key, 1, IPC_CREAT | IPC_EXCL | SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;

    return;
}

void free_ipc() {
    sem_wait(sem_shm, 0);

    printf("[IPC] Releasing IPCs resources \n");
    if (shmctl(shm_context, IPC_RMID, 0) == -1) { TEST_ERRORM; }

    if (shmctl(shm_harb, IPC_RMID, 0) == -1) { TEST_ERRORM; }
    if (shmctl(shm_ship, IPC_RMID, 0) == -1) { TEST_ERRORM; }
    if (shmctl(shm_goods, IPC_RMID, 0) == -1) { TEST_ERRORM; }

    if (semctl(sem_shm, IPC_RMID, 0) == -1) { TEST_ERRORM; }

    /*if (msgctl(test_msg       , IPC_RMID, 0) == -1) { TEST_ERRORM; } */
    printf("[IPC] Released IPCs resources !\n");
    return;
}

void get_ipcs() {
    if ((shm_context    = shmget(ftok(FTOK_PATH, 'c'), sizeof(context), SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((ctx            = shmat(shm_context  , NULL, 0)) == (void*) -1)
        TEST_ERRORM;
    if ((shm_harb       = shmget(ftok(FTOK_PATH, 'h'), sizeof(harbours) * ctx -> SO_porti, SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((shm_ship       = shmget(ftok(FTOK_PATH, 's'), sizeof(ship)  * ctx -> SO_navi, SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((shm_goods      = shmget(ftok(FTOK_PATH, 'g'), sizeof(batch_of_goods) * ctx->SO_merci, SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    if ((sem_shm        = semget(ftok(FTOK_PATH, 'm'), 1, SHM_PERM_FLAG)) == -1)
        TEST_ERRORM;
    /*if ((test_msg       = msgget(ftok(FTOK_PATH, 'm'), SHM_PERM_FLAG)) == -1) ERROR; */

    return;
}

void detach_context() {
    if((shmdt(ctx)) == -1)
        TEST_ERRORM;
    return;
}

pid_t create_process() {
    /*sem_signal(sem_proc_count, 0);*/
    return fork();
}

void terminate_process() {
    /*detach_context();
    free_ipc();*/
    /*sem_wait(sem_proc_count, 0);*/
    exit(EXIT_SUCCESS);
}

/* Access (in mutual exclusion) shared memory */
void attach_shared_memory() {
    sem_wait(sem_shm, 0);
    if(ctx -> stop == 0) {
        if((goods = shmat(shm_goods, NULL, 0)) == ( batch_of_goods * ) -1) TEST_ERRORM;
        if((harbours = shmat(shm_harb, NULL, 0)) == ( harbour * ) -1) TEST_ERRORM;
        if((ships  = shmat(shm_ship,  NULL, 0)) == ( ship *  ) -1) TEST_ERRORM;
    } else
        terminate_process();
    return;
}


/* Detach shared memory
 * */
void detach_shared_memory() {
    if(ctx -> stop == 0) {
        if((shmdt(goods)) == -1)
            TEST_ERRORM;
        if((shmdt(harbours)) == -1)
            TEST_ERRORM;
        if((shmdt(ships )) == -1)
            TEST_ERRORM;
    }
    sem_signal(sem_shm, 0);
    /*sem_signal(sem_proc_count, 0);*/
    return;
}