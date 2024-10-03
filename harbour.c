/**
 * Processo porto.
 * 
 * 
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include "common.h"
#include "logger.h"
#include "shm_utils.h"

#ifndef ENABLED_LEVELS
#define ENABLED_LEVELS (L_INFO | L_DEBUG | L_ERROR)
#endif

#ifndef LOG_TO_CONSOLE
#define LOG_TO_CONSOLE 1
#endif

#ifndef LOG_FILE_NAME
#define LOG_FILE_NAME "ship_harb.txt"
#endif

/* Variabili globali */
int sem_id = -2;
int shm_id = -2;
int msg_id = -2;

sharedElement *shared_array;

int cur_day = 0;
int id;

/* Disalloca tutto e termina il processo */
void shutdown() {
    sigset_t mask;
    message msg;

    /* Blocco tutti i segnali */
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    
    /**
     * Invio un messaggio al master usando come mtype
     * un numero casuale compreso tra 1 ed ctx->SO_merci.
     **/
    msg.mtype = (rand() % ctx->SO_merci) + 1;
    msg.pid = getpid();
    msgsnd(msg_id, &msg, MESSAGE_SIZE, 0);

    /* Stacco la porzione di memoria condivisa allo spazio di indirizzi del processo */
    shmdt(shared_array);

    detach_context();  /* detach shared memory */

    exit(EXIT_SUCCESS);
}

/**
 * Incremento l'elemento in posizione cur_day dell'array in memoria
 * condivisa. Questo lo faccio accedendo ad una sezione critica
 * in mutua esclusione per evitare inconsistenze.
 * 
 * Provare a togliere sem_wait e sem_signal per vedere che cosa può
 * accadere se non si fa attenzione ad accedere in mutua esclusione
 * alla sezione critica di un programma.
 **/
void incrementElement(){
    sem_wait(sem_id, MUTEX_SEM);

    /*log_write("[DAY DEBUG] Harbour[%d] print day: %d\n", id, cur_day);
    log_write("[DAY DEBUG]Harbour print value_harb %d\n",shared_array[cur_day].value_harb);*/
    shared_array[cur_day].value_harb++;

    sem_signal(sem_id, MUTEX_SEM);
}

/* Handler per i segnali SIGTERM e SIGINT */
void sigterm_handler(int signum) {
    shutdown();
}

/* Handler per il segnale SIGALRM */
void sigalrm_handler(int signum) {
    /* Passaggio al giorno successivo */
    cur_day++;
}

void get_harbour_ipcs(){
    /* Load common ipcs*/
    get_ipcs();

    /*Load sample (old) child ipcs*/
    /* 2 semafori (start e mutex) */
    sem_id = semget(SEM_KEY, 2, 0600); /* 2 semafori */
    TEST_ERROR;

    /* Memoria condivisa (un array lungo ctx->SO_days di elementi di tipo sharedArrayElement) */
    shm_id = shmget(SHM_KEY, sizeof(sharedElement)*ctx->SO_days, 0600);
    TEST_ERROR;
    /* Attacco la porzione di memoria condivisa allo spazio di indirizzi del processo */
    shared_array = shmat(shm_id, NULL, 0);
    TEST_ERROR;

    /* Coda di messaggi */
    msg_id = msgget(MSG_KEY, 0600);
    TEST_ERROR;
}

/**
 * argv[0]: ./harbour
 * argv[1]: int velocita (km/giorno)  = SO_SPEED
 * argv[2]: int capacita (tonnellate) = SO_CAPACITY
 * argv[3]: double posizioneX (X coordinate)
 * argv[4]: double posizioneY (Y coordinate)
 **/
int main(int argc, char* argv[]){

    /* Inizializzo il logger */
#ifdef LOG_TO_CONSOLE
    finit_log_file(stdout, LOG_DEBUG);
#else
    init_log_file(LOG_FILE_NAME, LOG_DEBUG);
#endif
    set_default_log_level(L_INFO);
    set_enabled_log_levels(ENABLED_LEVELS);

    /* Sanity check and conversione argomenti */
    if(argc < 5){
        fprintf(stderr, "Usage: %s <speed> <capacity> <pos_x> <pos_y>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Inizializzo variabili chiave */
    id = atoi(argv[1]);
    int docks_num = atoi(argv[2]);
    double pos_x = atof(argv[3]);
    double pos_y = atof(argv[4]);
    

    log_lev(L_DEBUG, "[IPC DEBUG] + New harbour created");
    log_lev(L_DEBUG, "[IPC DEBUG]   - ID : %d", id);
    log_lev(L_DEBUG, "[IPC DEBUG]   - docks_num : %d", docks_num);
    log_lev(L_DEBUG, "[IPC DEBUG]   - posX : %4.2f", pos_x);
    log_lev(L_DEBUG, "[IPC DEBUG]   - posY : %4.2f", pos_y);


    /* Imposto l'handler per la terminazione */
    set_handler(SIGTERM, &sigterm_handler);
    set_handler(SIGINT, &sigterm_handler);
    /* Imposto l'handler per il SIGALRM */
    set_handler(SIGALRM, &sigalrm_handler);
    
    /* Recupero le strutture condivise */
    get_harbour_ipcs();

    log_lev(L_DEBUG, "[IPC DEBUG] + Loaded shared memory data");
    log_lev(L_DEBUG, "[IPC DEBUG]    - Harbour number max read from ctx: %d", ctx->SO_banchine);

    /* Inizializzo il processo */
    srand((time(NULL)+id) ^ getpid());  /* naive random seed */

    /* Access to shared memory to read docks number from harbour (test)  */
    attach_shared_memory();
        harbours[id].docks = get_random_int(0, ctx->SO_banchine);
        log_lev(L_DEBUG, "[IPC DEBUG]    - Number of docks read from harbour[0]: %d", harbours[0].docks);
    detach_shared_memory();

    /* Attendo che il master dia il 'via' sul semaforo start */
    sem_wait(sem_id, START_SEM);
    log_lev(L_DEBUG, "[IPC DEBUG]   - Starting harbour with id %d", id);

    while(1){
        /* Decido cosa fare in base allo stato attuale delle banchine */
        /*if(harbours[id].busy_docks >= harbours[id].docks){
            /* Se ci sono banchine libere, posso decidere se far attraccare una nave o meno 
            if(get_random_int(0, 100) < 50){
                /* Decido di far attraccare una nave 
                log_lev(L_DEBUG, "[IPC DEBUG]   - Harbour[%d] decided to let a ship dock", id);
                incrementElement();
                harbours[id].docks--;
            }
        }*/

        incrementElement();
        pause(); /* Sospendo il processo finche' non arriva un segnale */
    }

    return EXIT_SUCCESS;
}

/* add implementation for ship movement
 add implementation for ship docking
 add implementation for ship loading/unloading
 add implementation for ship negotiation
 improve ship communication with master*/

