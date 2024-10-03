/**
 * Processo nave.
 * 
 * Ogni nave ha
 * una velocita SO_SPEED misurata in kilometri al giorno (identica per tutte le navi),
• una posizione, ovvero una coppia di coordinate di tipo (double) all’interno della mappa,
• una capacit`a SO_CAPACITY (in ton) che misura il carico totale trasportabile (identica per tutte le navi). Una
nave non pu`o trasportare una quantit`a di merci che ecceda la propria capacit`a.
Le navi nascono da posizioni casuali e senza merce. Non c’`e limite al numero di navi che possono occupare una
certa posizione sulla mappa, mentre il numero di navi che possono svolgere contemporaneamente operazioni di
carico/scarico in un porto `e limitato dal numero di banchine.
Lo spostamento della nave da un punto ad un altro `e realizzato tramite una nanosleep che simuli un tempo
simulato di
distanza fra posizione di partenza e di arrivo
velocit`a di navigazione .
Nel calcolare questo tempo, si tenga conto anche della parte frazionaria (per esempio, se si deve dormire per 1.41
secondi non va bene dormire per 1 o 2 secondi). Si ignorino le eventuali collisioni durante il movimento sulla mappa.
Le navi possono sapere tutte le informazioni sui porti: posizione, merci offerte/richieste, etc. Ogni nave, in
maniera autonoma:
• si sposta sulla mappa
• quando si trova nella posizione coincidente con il porto, pu`o decidere se accedere ad una banchina e, se questo
avviene, pu`o decidere lo scarico o il carico della merce ancora non scaduta.
La negoziazione fra nave e porto su tipologia e quantit`a di merci da caricare o scaricare avviene in un momento a
discrezione del progettista (prima di partire per la destinazione, quando arrivata nel porto, altro, etc.)
Le navi non possono comunicare fra loro, n´e sapere il contenuto di quanto trasportato dalle altre navi
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
#define LOG_FILE_NAME "ship_logs.txt"
#endif

/* Variabili globali */
int sem_id = -2;
int shm_id = -2;
int msg_id = -2;

sharedElement *shared_array;

int cur_day = 0;
int id;
ship_status cur_status;

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

    /*log_write("Ship print DAY:%d ID:%d\n", cur_day, id);
    log_write("Ship print value_ship %d\n",shared_array[cur_day].value_ship);*/

    shared_array[cur_day].value_ship++;
    sem_signal(sem_id, MUTEX_SEM);
}

/* Handler per i segnali SIGTERM e SIGINT */
void sigterm_handler(int signum) {
    shutdown();
}

/* Handler per il segnale SIGALRM */
void sigalrm_handler(int signum) {
    /* Passaggio all'elemento successivo dell'array */
    cur_day++;
}

void get_ship_ipcs(){
    
    /* Recupero le strutture condivise */
    get_ipcs();

    /* Carico 2 semafori (start e mutex) */
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


/* Implement strategy to choose an harbour*/
int choose_destination_harbour_id(){
    int dest_harbour = -1;

    do
    {
        dest_harbour = get_random_int(0, ctx->SO_porti - 1);
    }
    while (dest_harbour == -1 && (cur_status.harbour_id != dest_harbour));

    log_lev(L_DEBUG, "[SHIP DEBUG] Ship %d: destination harbour is %d\n", id, dest_harbour);
    return dest_harbour;
}

/* Simulate navigation compute distance and sleep (nano sleep) */
void navigate_to(position pos){
    double distance = get_distance(pos, cur_status.pos);
    double time = distance / cur_status.cur_speed;
    log_lev(L_DEBUG, "[SHIP DEBUG] Ship %d is navigating to %f,%f", id, pos.x, pos.y);
    log_lev(L_DEBUG, "[SHIP DEBUG] Ship %d will navigate (sleep) for %f seconds", id, time);
    wait_for_action(time);
}

int goto_harbour() {

    int new_destination_harbour_id = choose_destination_harbour_id();
    cur_status.harbour_id = new_destination_harbour_id;
    cur_status.harbour_dock_id = -1;
    cur_status.state = NAVIGATION;

    position dest_harb_pos;

    /* Access shared memory and perform r/w operations*/
    attach_shared_memory();
        dest_harb_pos = harbours[cur_status.harbour_id].pos; /* get destination harbour position */
        ships[id].state = cur_status.state; /* Update ship status in ships array*/
    detach_shared_memory();
    navigate_to(dest_harb_pos);

    return 0;
}

/* Update ship status and position. Wait for a dock to be available*/
int arrivedin_harbour() {

    position cur_harb_pos;

    cur_status.state = ARRIVED;
    /* Access shared memory and perform r/w operations*/
    attach_shared_memory();
        cur_harb_pos = harbours[cur_status.harbour_id].pos; /* get destination harbour position */ /*Could have saved the destination (on cur_status struct) when choosing the harbour instead*/
        ships[id].state = cur_status.state; /* Update ship status in ships array*/
    detach_shared_memory();
    cur_status.pos = cur_harb_pos;

    /* TODO: Wait for dock to be free */
    /*cur_status.harbour_dock_id = dock_harbour();*/
}

void exchange_goods() {
    cur_status.state = EXCHANGE;
    /* Access shared memory and perform r/w operations*/
    attach_shared_memory();
        ships[id].state = cur_status.state; /* Update ship status in ships array*/
    detach_shared_memory();

    /* TODO: IPC with harbour to define what goods to load and to unload (and qty) */
    /* TODO: Compute real loading time*/
    double loading_time = get_random_double(0, 3);
    double unloading_time = get_random_double(0, 3);
    log_lev(L_DEBUG, "[SHIP DEBUG] Ship %d will load for %f seconds and unload for %fseconds", id, loading_time, unloading_time);
    wait_for_action(loading_time+unloading_time);
    return 0;
}

/*
int dock_harbour() {
    int dock_id = choose_dock_id();
    cur_status.harbour_dock_id = dock_id;
    cur_status.state = DOCKED;

    /* Set the status to docked *
    setStatus(boat->state == In_Sea ? In_Sea_Docked : In_Sea_Empty_Docked);

    return dock_id;
}*/


void complete_exchange(){
    cur_status.state = WAITING_DEPARTURE;
    /* Access shared memory and perform r/w operations*/
    attach_shared_memory();
        ships[id].state = cur_status.state; /* Update ship status in ships array*/
    detach_shared_memory();
}


/**
 * argv[0]: ./ship
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

    /* Recupero i dati passati da linea di comando */
    id = atoi(argv[1]);
    int capacity = atoi(argv[2]);
    double pos_x = atof(argv[3]);
    double pos_y = atof(argv[4]);

    /* Inizializzo il generatore di numeri casuali usando il pid del processo */
    srand((time(NULL)+id) ^ getpid());  /* naive random seed */

    /* Imposto l'handler per la terminazione */
    set_handler(SIGTERM, &sigterm_handler);
    set_handler(SIGINT, &sigterm_handler);

    /* Imposto l'handler per il SIGALRM */
    set_handler(SIGALRM, &sigalrm_handler);

    /* Recupero le strutture condivise */
    get_ship_ipcs();

    log_lev(L_DEBUG, "[IPC DEBUG] + Loaded shared memory data");
    log_lev(L_DEBUG, "[IPC DEBUG]    - Harbour number read from SHIP: %d", ctx->SO_porti);

    /* Access to shared memory to read docks number from harbour (test)  */
    attach_shared_memory();
        log_lev(L_DEBUG, "[IPC DEBUG]    - Cur capacity read from ships[%d]: %d",id, ships[id].cur_capacity);
    detach_shared_memory();

    /* Init ship status */
    cur_status.state = WAITING_DEPARTURE;
    cur_status.pos = (position){pos_x, pos_y};
    cur_status.harbour_id = -1;
    cur_status.harbour_dock_id = -1;
    /*cur_status.pos = get_random_position(0, ctx->SO_lato);*/
    cur_status.cur_speed = ctx->SO_speed;

    log_lev(L_DEBUG, "[IPC DEBUG] + New ship created");
    log_lev(L_DEBUG, "[IPC DEBUG]   - id : %d", id);
    log_lev(L_DEBUG, "[IPC DEBUG]   - capacity : %d", capacity);
    log_lev(L_DEBUG, "[IPC DEBUG]   - posX : %4.2f", pos_x);
    log_lev(L_DEBUG, "[IPC DEBUG]   - posY : %4.2f", pos_y);

    /* Attendo che il master dia il 'via' sul semaforo start */
    sem_wait(sem_id, START_SEM);
    log_lev(L_DEBUG, "[IPC DEBUG]   - Starting ship with id %d", id);
    
    while(1){
        /* Decido cosa fare in base allo stato attuale (navigazione senza meta, a un porto appena arrivato, carico, scarico) */
        /* Se sono in navigazione senza meta */
        /* Scelgo un porto (il primo libero che incontro iterando ? il piu vicino (libero) che trovo? il primo libero che incontro iterando che ha ancora merce da offrire? */
        /* Calcolo la distanza tra la mia posizione e quella del porto e dormo con nanosleep(distanza/velocita) */

        /* Se sono arrivato al porto */
            /* Se il porto e' libero (banchine disponibili)*/
                /* Se il porto ha merce da offrire e la nave non e' piena*/
                    /* Carico tutta la merce a disposizione */
                /* Se il porto ha merce da richiedere e la nave ha merci */
                    /* Scarico tutta la merce che ho */
                /* Se il porto non ha NE merce da richiedere o la nave non ha merci OPPURE se Se il porto ha merce da offrire e la nave non e' piena*/    
                    /* Cambio porto */
            /* Altrimenti */
                /* Attendo sul semaforo */
        /* Se il porto non e' libero */ /* Alternativa sarebbe chiedere prima al porto il permesso e "riservarsi" il posto. Cosi ci sara sicuramente posto all'arrivo */
            /* Attendo sul semaforo */

        switch (cur_status.state)
        {
            case WAITING_DEPARTURE:
                /* Find harbour */
                goto_harbour();
                /*break;*/
            case NAVIGATION:
                /* Navigate */
                /*break;*/
            case ARRIVED:
                /* Arrived: wait for dock */
                log_lev(L_DEBUG, "[SHIP DEBUG] Ship %d: Arrived to harbour %d (day %d)", id, cur_status.harbour_id, cur_day);
                arrivedin_harbour();
                /*break;*/
            case EXCHANGE:
                /* Loading */
                log_lev(L_DEBUG, "[SHIP DEBUG] Ship %d: Exchanging goods in harbour %d (day %d)", id, cur_status.harbour_id, cur_day);
                exchange_goods();
                complete_exchange();
                log_lev(L_DEBUG, "[SHIP DEBUG] Ship %d: Completed exchanging of goods in harbour %d (day %d)", id, cur_status.harbour_id, cur_day);
                /* Unloading */
                /*break;*/
            default:
                break;
        }
        incrementElement();
        pause();
    }

    return EXIT_SUCCESS;
}

/* add implementation for ship movement
 add implementation for ship docking
 add implementation for ship loading/unloading
 add implementation for ship negotiation
 improve ship communication with master*/

