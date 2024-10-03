#ifndef __COMMON_H
#define __COMMON_H
/*#define _GNU_SOURCE*/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>


#define TEST_ERROR    if (errno) {dprintf(2, \
					   "%s:%d: PID=%5d: Error %d (%s)\n",\
					   __FILE__,\
					   __LINE__,\
					   getpid(),\
					   errno,\
					   strerror(errno));}

#define TEST_ERRORM    if (errno) {fprintf(stderr, \
					   "%s:%d: PID=%5d: Error %d (%s)\n",\
					   __FILE__,\
					   __LINE__,\
					   getpid(),\
					   errno,\
					   strerror(errno));}

#define SEM_KEY 117
#define SHM_KEY 43972
#define MSG_KEY 23477

#define START_SEM 0
#define MUTEX_SEM 1

#define CHILD_PATH "./child"
#define SHIP_PATH "./ship"
#define HARBOUR_PATH "./harbour"

/* Define max values to over allocate arrays  (Requirements #7, table) */
#define MAX_SO_MERCI 100
#define MAX_SO_DAYS 10

#define MAX_DIGIT_CHILD_ID 4 /* Allowed range for childid*/


#define FTOK_PATH "common.c"

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                (Linux specific) */
};

/* User defined types */
/*enum good_type {PASTA = 1, ORTAGGI, METALLO, LEGNO, FRUTTA};*/


typedef struct {
    pid_t pid;             /* PID */
    char type;             /* P o N */
    /* Eventually add pointer or id to ship/harbours list */
} child_info;


typedef struct {
    double x ;        /* X */
    double y ;        /* Y */
} position;


typedef struct {
    unsigned int qty ;       /* Quantity of the given good (0 < qty < SO_SIZE) */
    unsigned int lifetime ;  /* The lifetime of the given batch (SO_MIN_VITA < lifetime < SO_MAX_VITA) */
    unsigned int type ;      /* The type of the good (0 <= type < 5) */
    /*
    /status is the status of the given good (0 < status < SO_NUM_STATI) ? or derive it from other structure*/
} batch_of_goods;
/* Map status related structures*/
typedef struct {
    unsigned int num_goods; /* Numero di merci presenti sulla mappa */
    batch_of_goods *goods;  /* Array di merci presenti sulla mappa */
} map_status;


/*
 * Harbour data structure
 * */

/* Enum with harbour status:
 - 0: Available
 - 1: Busy
*/
enum harbour_status {
    AVAILABLE,
    BUSY
};


typedef struct { /* See 5.6 [Per ogni tipo di merce.. ] on requirements*/
    unsigned int num_goods_expired[MAX_SO_MERCI];           /* Qta di merce scaduta in un porto */
    unsigned int num_goods_waiting[MAX_SO_MERCI];           /* Qta di merce presente in un porto */
    unsigned int num_goods_sent[MAX_SO_MERCI];              /* Qta di merce caricata (spedita)  */
    unsigned int num_goods_received[MAX_SO_MERCI];          /* Qta di merce scaricata in un porto dalle navi */
    /*Note:
     * To compute how much remained in the same place we can do (SO_FILL - num_goods_sent) */

    unsigned int tot_offer_by_type[MAX_SO_MERCI];           /* Qta di merce offerta dal porto per tipo  */
    unsigned int tot_demand_by_type[MAX_SO_MERCI];          /* Qta di merce richiesta dal porto per tipo  */
} harbour_statistics;

typedef struct {
    position pos;
    int docks;  /* Number of docks: 1 <= docks <= SO_BANCHINE*/
    int busy_docks;

    unsigned int tot_received;
    unsigned int tot_sent;

    unsigned int cur_day; /* TODO: remove later?*/

    harbour_statistics stats;
    /* expiration of goods. TOREMEMBER: Set this when creating offer demand on the harbour!
     * Probably need to be reset when a good is delivered! (or can expire also after being delviered?) */
    int num_goods_expiring_by_day_type[MAX_SO_DAYS][MAX_SO_MERCI];

} harbour;


/*
 * Ship data structure
 * */


/* Enum with ship status:
 - 0: Waiting Departure
 - 1: Navigation
 - 2: Arrived
 - 3: Exchange
*/
enum ship_state {
    WAITING_DEPARTURE,
    NAVIGATION,
    ARRIVED,
    EXCHANGE
};


typedef struct { /* Used by ship process to mantain the current state*/
    enum ship_state state;
    position pos;
    int harbour_id;
    int harbour_dock_id;
    int cur_speed;
} ship_status;

typedef struct { /* See 5.6 [Per ogni tipo di merce.. ] on requirements*/
    unsigned int num_goods_expired[MAX_SO_MERCI];           /* Qta di merce scaduta su una nave */

} ship_statistics;


typedef struct {
    enum ship_state state; /* TODO: Remember to copy the value from the process ship_status.state property every day (sigalrm handler)*/
    unsigned int max_capacity;
    unsigned int cur_capacity;
    unsigned int cur_speed;
    unsigned int cur_day; /* TODO: remove later?*/

    ship_statistics stats;

    /* expiration of goods. TOREMEMBER: Set this only when loading ship.*/
    int num_goods_expiring_by_day_type[MAX_SO_DAYS][MAX_SO_MERCI];
} ship;

/*
 * Context (configuration) data structure
 * */
typedef struct {
    unsigned int SO_navi;
    unsigned int SO_porti;
    unsigned int SO_merci;
    unsigned int SO_size;
    unsigned int SO_min_vita;
    unsigned int SO_max_vita;
    unsigned int SO_lato;
    unsigned int SO_speed;
    unsigned int SO_capacity;
    unsigned int SO_banchine;
    unsigned int SO_fill;
    unsigned int SO_loadspeed;
    unsigned int SO_days;

    short stop; /* boolean to stop the simulation */

    /* eventually add the other parameters here (for extended version) **/
} context;


/*
 * Shared data structure
 * */

/* Shared memory data */
extern int shm_goods;
extern batch_of_goods* goods;

extern int shm_ship;
extern ship* ships;

extern int shm_harb;
extern harbour* harbours;

extern int shm_context;
extern context * ctx;

/* Semaphore data */

extern int sem_shm; /* Semaphore for shared memory (1 for now) but one for each structure later */

extern int sem_ship; /* Semaphore for ships */
extern int sem_harb; /* Semaphore for harbours */
extern int sem_context; /* Semaphore for context */
extern int sem_good; /* Semaphore for goods */

typedef struct {
	int value_ship;
	int value_harb;
} sharedElement;

typedef struct {
	long mtype;
	pid_t pid;
} message;

#define MESSAGE_SIZE (sizeof(pid_t))

/* Riduce di 1 il valore del semaforo (acquisisce risorsa) */
int sem_wait(int s_id, unsigned short sem_num) ;

/* Aumenta di 1 il valore del semaforo (rilascia risorsa) */
int sem_signal(int s_id, unsigned short sem_num);

/* Esegue una semop */
int sem_cmd(int s_id, unsigned short sem_num, short sem_op, short sem_flg);

int sem_set(int s_id, int sem_num, int value);

/* Blocca i segnali elencati tra gli argomenti */
/* Restituisce la vecchia maschera */
sigset_t block_signals(int count, ...);

/* Sblocca i segnali elencati tra gli argomenti */
/* Restituisce la vecchia maschera */
sigset_t unblock_signals(int count, ...);

/* 
 * Imposta una maschera per i segnali, usata per reimpostare una
 * vecchia maschera ritornata da block_signals
 */
void reset_signals(sigset_t old_mask);

/* Imposta un nuovo handler per il segnale sig */
/* Ritorna il vecchio struct sigaction */
struct sigaction set_handler(int sig, void (*func)(int));


int get_random_int(int min, int max);
double get_random_double(double min, double max);
position get_random_position(double min, double max);
double get_distance(position p1, position p2);
int wait_for_action(double wait_time);

#endif /* __COMMON_H */ 

