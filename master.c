/** 
 * Processo master.
 * 
 * Il processo master genera N processi figli e invia ad essi un SIGALRM ogni S secondi.
 * I processi figli incrementano di uno il campo value_inc e decrementano di uno il campo value_dec dell'cur_day-esimo
 * elemento di un array che si trova in memoria condivisa.
 * Quando ricevono il SIGALRM dal processo master, passano all'elemento cur_day+1 dell'array. Questo avviene per ctx->SO_days volte.
 * Dopodichè il processo master invia un segnale SIGTERM ai figli i quali terminano dopo aver inviato un
 * messaggio al master specificando il proprio pid e usando come tipo di messaggio un numero casuale tra 1 e N.
 * 
 * I tre parametri N, M e S vengono passati da linea di comando (al master).
 * 
 **/


/*
 * TODOs:
 * - Aggiunta di parametri da input in master es. (es. SO_NAVI, ..., SO_MAELSTROM)
 *
 * - ~ Creare due tipi diversi di child (navi, porti)
 * - ? Aggiunta di parametri da linea di comando (argv) in porti es. PORTI(es. posizione, num_banchine < SO_BANCHINE, SO_FILL?, SO_LOADSPEED?)
 * - Definizione costanti temporali (tempo simulato vs tempo reale, max tempo simulato)
 * - Refactoring. es. spostare funzioni come da common a shm_utils, uniformare nomi funzioni, eliminazione codice non usato,lingua, ...
 * - Inizializzazione strutture dati (es. coda di navi, processi figli, richiesta_offerta[tipo merce, quantita merce], ...)
 * - Definizione IPC objects (es. semafori, memoria condivisa, coda di messaggi, ...)
 * - Verifica e test handler per i segnali (es. SIGTERM, SIGINT, SIGALRM, ...)
 * - Aggiunta funzioni IPC mancanti (es. createIPCobj, createChildren, shutdown, ...)

 * - Gestione report (es. stampa su file, stampa su console, ...)
 *      + Quando triggerare (inizio, durante, fine) (handled by signals)
 *      - Definizione e implementazione di cosa stampare (es. array in memoria condivisa, ...) e come stampare (es. formato, ...)
 * - Gestione terminazione
 *      + per timeout simulazione (alarm)
 *      - per terminazione da tastiera (ctrl + c)
 *      - per condizione di terminazione (es. offerta pari a 0 OPPURE richiesta pari a 0)
 *
 *
 *  + Definizione user defined types (es. struct coordinate, struct merce, ...)
 *  + Aggiunta di parametri da linea di comando (argv) in child es. NAVI (es. velocita, capacita, posizione)
 *  + Definizione variabili globali (es. numero navi, numero processi figli, ...)
 *  + Tipo merce : max SO_MERCI. Numero variabile, prototipo enum lasciato
 *  + Definizione prime funzioni IPC e shm
 *  + Definizione IPC objects (es. semafori, memoria condivisa, coda di messaggi, ...)
 *  + Definizione handler per i segnali (es. SIGTERM, SIGINT, SIGALRM, ...)
 *  + Definizione strutture dati (es. coda di navi, scadenze, statistiche, ...)
 *  + Creare due tipi diversi di child (navi, porti)
 */
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
#include "shm_utils.h"
#include "logger.h"

#ifndef ENABLED_LEVELS
#define ENABLED_LEVELS (L_INFO | L_DEBUG | L_ERROR)
#endif

#ifndef LOG_FILE_NAME
#define LOG_FILE_NAME "log.txt"
#endif

#define MAX_ARGS 10

/** Variabili globali **/
int sem_id = -2;
int shm_id = -2;
int msg_id = -2;

sharedElement *shared_array;

pid_t *ships_pids;

pid_t *harbours_pids;

int S;

int cur_day = 0;

/**
 * Carico i parametri di configurazione dal file
 * @param CONFIG
 */
void load_config_file(context * CONFIG){
    FILE *fp;

    if((fp = fopen("opt.conf", "r")) == NULL){
        perror("ERRORE");
        exit(EXIT_FAILURE);
    }

    while(!feof(fp)){
        char name[32];
        int value = 0;
        fscanf(fp, "%s %d\n", name, &value);

        if(strstr(name, "SO_NAVI") != NULL){
            ctx->SO_navi = value;
        }else if(strstr(name, "SO_PORTI") != NULL){
            ctx->SO_porti = value;
        }else if(strstr(name, "SO_MERCI") != NULL){
            ctx->SO_merci = value;
        }else if(strstr(name, "SO_SIZE") != NULL){
            ctx->SO_size = value;
        }else if(strstr(name, "SO_MIN_VITA") != NULL){
            ctx->SO_min_vita = value;
        }else if(strstr(name, "SO_MAX_VITA") != NULL){
            ctx->SO_max_vita = value;
        }else if(strstr(name, "SO_LATO") != NULL){
            ctx->SO_lato = value;
        }else if(strstr(name, "SO_SPEED") != NULL){
            ctx->SO_speed = value;
        }else if(strstr(name, "SO_CAPACITY") != NULL){
            ctx->SO_capacity = value;
        }else if(strstr(name, "SO_BANCHINE") != NULL){
            ctx->SO_banchine = value;
        }else if(strstr(name, "SO_FILL") != NULL){
            ctx->SO_fill = value;
        }else if(strstr(name, "SO_LOADSPEED") != NULL){
            ctx->SO_loadspeed = value;
        }else if(strstr(name, "SO_DAYS") != NULL) {
            ctx->SO_days = value;
        }/*else if(strstr(name, "SO_STORM_DURATION") != NULL) {
            ctx->SO_storm_duration = value;
        }else if(strstr(name, "SO_SWELL_DURATION") != NULL) {
            ctx->SO_swell_duration = value;
        }else if(strstr(name, "SO_MAELSTORM") != NULL) {
            ctx->SO_maelstrom = value;
        }*/
        ctx->stop = 0;
    }
    fclose(fp);

    if(ctx->SO_navi < 1){
        perror("ERRORE: I processi nave devono essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_porti < 4){
        perror("ERRORE: I processi porti devono essere >= 4\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_merci < 1){
        perror("ERRORE: il valore SO_MERCI deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_size < 1){
        perror("ERRORE: il valore SO_SIZE deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_min_vita < 1){
        perror("ERRORE: il valore SO_MIN_VITA deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_max_vita < 1){
        perror("ERRORE: il valore SO_MAX_VITA deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_lato < 1){
        perror("ERRORE: il valore SO_LATO deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_speed < 1){
        perror("ERRORE: il valore SO_SPEED deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_capacity < 1){
        perror("ERRORE: il valore SO_CAPACITY deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_banchine < 1){
        perror("ERRORE: il valore SO_BANCHINE deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_fill < 1){
        perror("ERRORE: il valore SO_FILL deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_days < 1){
        perror("ERRORE: il valore SO_DAYS deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }/*else if(ctx->SO_storm_duration < 1){
        perror("ERRORE: il valore SO_STORM_DURATION deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_swell_duration < 1){
        perror("ERRORE: il valore SO_SWELL_DURATION deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }else if(ctx->SO_maelstrom < 1){
        perror("ERRORE: il valore SO_MAELSTORM deve essere >= 1\n");
        exit(EXIT_FAILURE);
    }*/
}

/* Creo gli IPC objects */
void createIPCobj() {
    int i;

    /* 2 semafori (start e mutex) */
    sem_id = semget(SEM_KEY, 2, 0600 | IPC_CREAT);
    TEST_ERROR;

    /* Semaforo start inizializzato a 0 */
    semctl(sem_id, START_SEM, SETVAL, 0);
    TEST_ERROR;

    /* Semaforo mutex inizializzato a 1 */
    semctl(sem_id, MUTEX_SEM, SETVAL, 1);
    TEST_ERROR;

    /* Memoria condivisa (un array lungo ctx->SO_days di elementi di tipo sharedElement) */
    shm_id = shmget(SHM_KEY, sizeof(sharedElement)*ctx->SO_days, 0600 | IPC_CREAT);
    TEST_ERROR;

    /* Attacco la porzione di memoria condivisa allo spazio di indirizzi del processo */
    shared_array = shmat(shm_id, NULL, 0);
    TEST_ERROR;

    /* Inizializzo l'array in memoria condivisa */
    for(i = 0; i < ctx->SO_days; i++){
        shared_array[i].value_ship = 0;
        shared_array[i].value_harb = 0;
    }

    /* Coda di messaggi */
    msg_id = msgget(MSG_KEY, 0600 | IPC_CREAT);
    TEST_ERROR;
}

/* Disalloca tutto e termina il processo */
void shutdown() {
    int i;
    message msg;
    sigset_t mask;

    /* Blocco tutti i segnali */
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    /* Termino i figli nave */
    for(i = 0; i < ctx->SO_navi; i++) {
        /* L'if serve a evitare di fare la kill con PID = 0 */
        log_lev(L_DEBUG, "Killing %d", ships_pids[i]);
        if (ships_pids[i]) kill(ships_pids[i], SIGTERM);
    }

    /* Termino i figli porto */
    for(i = 0; i < ctx->SO_porti; i++) {
        /* L'if serve a evitare di fare la kill con PID = 0 */
        log_lev(L_DEBUG, "Killing %d", harbours_pids[i]);
        if (harbours_pids[i]) kill(harbours_pids[i], SIGTERM);
    }


    /* Attendo la terminazione dei processi figli per evitare processi zombie. */
    while (wait(NULL) != -1);

    /**
     * Recupero alcuni o tutti i messaggi dalla coda di messaggi (dipende dal
     * campo mtype utilizzato) e, per ogni messaggio ricevuto, stampo il pid
     * del processo figlio che ha inviato il messaggio e il tipo del messaggio ricevuto.
     * 
     * Provare i vari casi per vedere cosa cambia in base al campo mtype.
     **/
    while(msgrcv(msg_id, &msg, MESSAGE_SIZE, 0, IPC_NOWAIT) > 0){
    /* while(num_bytes = msgrcv(msg_id, &msg, MESSAGE_SIZE, -N, IPC_NOWAIT) > 0){ */
    /* while(num_bytes = msgrcv(msg_id, &msg, MESSAGE_SIZE, 1, IPC_NOWAIT) > 0){ */
    /* while(num_bytes = msgrcv(msg_id, &msg, MESSAGE_SIZE, 1, IPC_NOWAIT | MSG_EXCEPT) > 0){ */
        log_write("Ricevuto il messaggio da parte del figlio con PID %d (tipo del messaggio %ld)", msg.pid, msg.mtype);;
    }

    /* Print summary one last time*/
    attach_shared_memory();
    print_summary();
    detach_shared_memory();

    free(ships_pids);
    free(harbours_pids);

    /* Stacco la porzione di memoria condivisa allo spazio di indirizzi del processo */
    shmdt(shared_array);

    /* Elimino la memoria condivisa, i semafori e la coda di messaggi */
    if (shm_id != -2) shmctl(shm_id, IPC_RMID, NULL);
    if (sem_id != -2) semctl(sem_id, 0, IPC_RMID);
    if (msg_id != -2) msgctl(msg_id, IPC_RMID, NULL);

    log_lev(L_DEBUG, "Releasing custom IPC resources");
    free_ipc();

    log_lev(L_DEBUG, "Simulation stopped. Terminating now!");

#ifndef LOG_TO_CONSOLE
    close_log();
#endif

    exit(EXIT_SUCCESS);
}

/* Creo i processi nave */
void create_ships(char* argv[]){
    int i;
    pid_t child_pid;

    char* argv_child[MAX_ARGS]; /* One extra element for the argv0 */
    argv_child[0] = SHIP_PATH; /* argv0 */
    argv_child[1] = malloc(MAX_DIGIT_CHILD_ID* sizeof(char)); /* i.e. 0000, 0001*/

    /*argv_child[1] = "1000"; /* speed */
    argv_child[2] = "10"; /* capacity */
    argv_child[3] = "0"; /* posX */
    argv_child[4] = "0"; /* posY */
    argv_child[5] = NULL;

    /* Creo SO_NAVI figli e salvo i loro pids */
    for(i = 0; i < ctx->SO_navi; i++){
        snprintf( argv_child[1], sizeof(argv_child[1]), "%d", i ); /* id */
        switch(child_pid = create_process()) {
            case -1:
                log_lev(L_ERROR, "Fork error");
                shutdown();
            case 0:
                /* I figli eseguiranno il programma child */
                execve(SHIP_PATH, argv_child, NULL);
                TEST_ERROR;
                exit(EXIT_FAILURE);
            default:
                ships_pids[i] = child_pid;
        }
    }
    free(argv_child[1]);  /* free temp buffer on master process at the end (not needed anymore)*/
}

/* Creo i processi porto */
void create_harbours(char* argv[]){
    int i;
    pid_t child_pid;
    char * buffer, buffer_id;
    char* argv_child[MAX_ARGS]; /* One extra element for the argv0 */
    argv_child[0] = SHIP_PATH; /* argv0 */
    argv_child[1] = malloc(MAX_DIGIT_CHILD_ID* sizeof(char)); /* i.e. 0000, 0001*/
    argv_child[2] = "0000"; 
    /* use temp buff and string copy to replace*/
    buffer = malloc(3* sizeof(char));

    argv_child[3] = "0"; /* posX */
    argv_child[4] = "0"; /* posY */
    argv_child[5] = NULL;

    /* Creo SO_porti figli e salvo i loro pids */
    for(i = 0; i < ctx->SO_porti; i++){
        /* Complete argv initialization */
        snprintf( argv_child[1], sizeof(argv_child[1]), "%d", i ); /* id */
        snprintf( buffer, 3* sizeof(char), "%d", get_random_int(0, ctx->SO_banchine) ); /* docks_num */
        argv_child[2]= buffer;

        /* Create process*/
        switch(child_pid = create_process()) {
            case -1:
                log_lev(L_ERROR, "Fork error");
                shutdown();
            case 0:
                /* I figli eseguiranno il programma child */
                execve(HARBOUR_PATH, argv_child, NULL);
                TEST_ERROR;
                exit(EXIT_FAILURE);
            default:
                harbours_pids[i] = child_pid;
        }
    }
    free(buffer);
    free(argv_child[1]);  /* free temp buffer on master process at the end (not needed anymore)*/
    /* don't need to free in the child since it will be deallocated at the end of the child process (and we assume to have argv available for the whole lifetime of the process)*/
}

/* Handler per i segnali SIGTERM e SIGINT */
void sigterm_handler(int signum) {
    log_write("Ricevuto il segnale %s, arresto la simulazione", strsignal(signum));
    shutdown();
}

/* Handler per il segnale SIGALRM */
void sigalrm_handler(int signum) {
    int i;

    log_write("Ricevuto il segnale %s, invio SIGALRM ai figli", strsignal(signum));
    attach_shared_memory();
    print_summary();
    detach_shared_memory();

    /* Passaggio all'elemento successivo dell'array */
    cur_day++;

    /* Invio un SIGALRM a tutti i processi figli */
    for(i = 0; i < ctx->SO_porti; i++){
        kill(harbours_pids[i], SIGALRM);
    }

    for(i = 0; i < ctx->SO_navi; i++){
        kill(ships_pids[i], SIGALRM);
    }

    /**
     * Setto un nuovo timer di S secondi (stampa giornaliera e cadenza quotidiana per navi) o invio a me stesso (master) un 
     * segnale SIGTERM se sono già passati ctx->SO_days giorni (terminazione e stampa finale). 
     * 1 sec vita reale = 1 giorno simulato
     **/
    if (cur_day < (ctx->SO_days - 1)) /* Need minus one since in this way we will schedule alarm (ctx->SO_days-1) times (notice that one alarm is already sent in the main before the while!)*/
        alarm(S);
    else
        raise(SIGTERM);
}

/**
 * Initialize the harbours array
 * @return 0 on success
 */
int initialize_harbours() {
    int i;
    for (i = 0; i < ctx->SO_porti; i++) {
        /* TODO: first 4 in the corners */
        harbours[i].pos = get_random_position(0, ctx->SO_lato);
        harbours[i].docks = get_random_int(1, ctx->SO_banchine);
        harbours[i].busy_docks = 0;
        /* Assign all the field of the harbour */
        harbours[i].tot_received = 0;
        harbours[i].tot_sent = 0;

        /* Initialize STATS with 0.
        TOREMEMBER: Remember to set it properly when creating goods or interacting with harbours */
        memset(harbours[i].stats.num_goods_expired,0,sizeof(harbours[i].stats.num_goods_expired[0])* MAX_SO_MERCI);
        memset(harbours[i].stats.num_goods_waiting,0,sizeof(harbours[i].stats.num_goods_waiting[0])* MAX_SO_MERCI);
        memset(harbours[i].stats.num_goods_sent,0,sizeof(harbours[i].stats.num_goods_sent[0])* MAX_SO_MERCI);
        memset(harbours[i].stats.num_goods_received,0,sizeof(harbours[i].stats.num_goods_received[0])* MAX_SO_MERCI);
        memset(harbours[i].stats.tot_offer_by_type,0,sizeof(harbours[i].stats.tot_offer_by_type[0])* MAX_SO_MERCI);
        memset(harbours[i].stats.tot_demand_by_type,0,sizeof(harbours[i].stats.tot_demand_by_type[0])* MAX_SO_MERCI);

        memset(harbours[i].num_goods_expiring_by_day_type, 0, sizeof(harbours[i].num_goods_expiring_by_day_type[0][0]) * MAX_SO_DAYS * MAX_SO_MERCI);

    }
    return 0;
}

/**
 * Initialize the ships array
 * @return 0 on success
 */
int initialize_ships() {
    int i;
    for (i = 0; i < ctx->SO_navi; i++) {
        /*ships[i].pos = get_random_position(0, ctx->SO_lato);
        ships[i].cur_speed = ctx->SO_speed;*/
        ships[i].max_capacity = ctx->SO_capacity;
        ships[i].cur_capacity = ctx->SO_capacity;
        /* Initialize STATS with 0.
        TOREMEMBER: Remember to set it properly when creating goods or interacting with ships */
        memset(ships[i].stats.num_goods_expired,0,sizeof(ships[i].stats.num_goods_expired[0])* MAX_SO_MERCI);

        memset(ships[i].num_goods_expiring_by_day_type, 0, sizeof(ships[i].num_goods_expiring_by_day_type[0][0]) * MAX_SO_DAYS * MAX_SO_MERCI);
    }
    return 0;
}

/**
 * Initialize the goods array
 * @return 0 on success
 */
int initialize_goods() {
    /* Define goods generation policy and algorithm*/

    /*TODO: initialize goods */

    int h;
    int tot_tons;
    for (h = 0; h < ctx->SO_porti; h++) {  /* We need to generate SO_FILL ton goods in each harbour (both offer and demand)*/
        tot_tons = 0;
        /* Generate offer up to SO_FILL tons */
        while(tot_tons < ctx->SO_fill){
            goods[h].qty = get_random_int(0,ctx->SO_size);
            goods[h].lifetime = get_random_int(ctx->SO_min_vita,ctx->SO_max_vita);
            goods[h].type = 0;
            tot_tons += goods[h].qty;
        }
        

        /* Generate ask up to SO_FILL tons */
    }


    int i;
    for (i = 0; i < ctx->SO_merci; i++) {
        goods[i].qty = get_random_int(0,ctx->SO_size);
        goods[i].lifetime = get_random_int(ctx->SO_min_vita,ctx->SO_max_vita);
        goods[i].type = 0;
    }

    /* TODO: TOREMEMBER: Remember to update statistics of both harbours and ships*/
    return 0;
}

/**
 * Function to print summary (use inside attach_shared_memory context)
 * @return 0 on success
 */
int print_summary() {
    /* TODO: distinguish between every day print summary and last one that is more detailed.
       TODO: Access propert datastructure and print data
    */
    log_write("[SUMMARY] SUMMARY print DAY %d\n", cur_day);

    int i;
    /*Do NOT use attach_shared_memory() at this level, since the caller will do that ;*/
    /* Stampo il contenuto del vettore in memoria condivisa */
    for(i = 0; i < ctx->SO_days; i++){
        log_write("array[%d].value_ship = %d", i, shared_array[i].value_ship);
        log_write("array[%d].value_harb = %d", i, shared_array[i].value_harb);
    }

    return 0;
}

void initialize_data() {
    printf("[IPC] Inizializzazione oggetti condivisi simulazione\n");

    attach_shared_memory();
    {
        if (initialize_harbours() == -1) TEST_ERRORM;
        if (initialize_ships () == -1) TEST_ERRORM;
        if (initialize_goods() == -1) TEST_ERRORM;

        if (print_summary() == -1) TEST_ERRORM;
    }
    detach_shared_memory();
}
/**
 * argv[0]: ./master
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

    /* Initialize context */
    ctx = (context *) malloc(sizeof(context));
    /* Assign values to context fields */
    
    load_config_file(ctx);

    S = 1;            /* Seconds in real life corresponding to one day of simulation */

    /* Alloco il vettore che conterrà i pids dei processi figli */
    ships_pids = (int *) malloc(sizeof(int)*ctx->SO_navi);
    harbours_pids = (int *) malloc(sizeof(int)*ctx->SO_porti);
	/* Imposto gli handler per la disallocazione delle risorse */
    set_handler(SIGTERM, &sigterm_handler);
    set_handler(SIGINT, &sigterm_handler);

    /* Creo gli oggetti IPC tutorato */
    createIPCobj();

    /* Creo gli oggetti IPC custom (navi)*/
    initialize_ipc(ctx);
    if(sem_set(sem_shm, 0, 1) == -1) TEST_ERRORM;
    initialize_data();

    /* Creo i processi figli */
    log_write("Creazione navi");
    create_ships(argv);
    log_write("Navi create");

    log_write("Creazione porti");
    create_harbours(argv);
    log_write("Porti creati");


    /* Imposto l'handler per il SIGALRM */
    set_handler(SIGALRM, &sigalrm_handler);

    /* Sblocco i processi individuo */
    sem_cmd(sem_id, START_SEM, (ctx->SO_navi+ctx->SO_porti), 0);

    /*sem_set(sem_id, START_SEM, (ctx->SO_navi+ctx->SO_porti));*/
    /* Setto il primo timer di S secondi */
    alarm(S);

    while(1) pause();
    
    return EXIT_SUCCESS;
}



