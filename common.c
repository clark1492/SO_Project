#include <math.h>
#include "common.h"
#include <errno.h>
#include "logger.h"



/* Move this to shm_utils.c ? */
int sem_wait(int s_id, unsigned short sem_num) {
	struct sembuf sops;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	sops.sem_num = sem_num;
	return semop(s_id, &sops, 1);
}

int sem_signal(int s_id, unsigned short sem_num) {
	struct sembuf sops;
	sops.sem_flg = 0;
	sops.sem_op = 1;
	sops.sem_num = sem_num;
	return semop(s_id, &sops, 1);
}

int sem_cmd(int s_id, unsigned short sem_num, short sem_op, short sem_flg) {
	struct sembuf sops;
	sops.sem_flg = sem_flg;
	sops.sem_op = sem_op;
	sops.sem_num = sem_num;
	return semop(s_id, &sops, 1);
}

int sem_set(int s_id, int sem_num, int value) {
    union semun arg;
    arg.val= value;
    return semctl(s_id, sem_num, SETVAL, arg);
}

sigset_t block_signals(int count, ...) {
	sigset_t mask, old_mask;
	va_list argptr;
	int i;

	sigemptyset(&mask);

	va_start(argptr, count);

	for (i = 0; i < count; i++) {
		sigaddset(&mask, va_arg(argptr, int));
	}

	va_end(argptr);

	sigprocmask(SIG_BLOCK, &mask, &old_mask);
	return old_mask;
}

sigset_t unblock_signals(int count, ...) {
	sigset_t mask, old_mask;
	va_list argptr;
	int i;

	sigemptyset(&mask);

	va_start(argptr, count);

	for (i = 0; i < count; i++) {
		sigaddset(&mask, va_arg(argptr, int));
	}

	va_end(argptr);

	sigprocmask(SIG_UNBLOCK, &mask, &old_mask);
	return old_mask;
}

void reset_signals(sigset_t old_mask) {
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

struct sigaction set_handler(int sig, void (*func)(int)) {
    struct sigaction sa, sa_old;
    sigset_t mask;
    sigemptyset(&mask);
    sa.sa_handler = func;
    sa.sa_mask = mask;
    sa.sa_flags = 0;
    sigaction(sig, &sa, &sa_old);
    return sa_old;
}

/**
 * Genera un numero casuale intero tra min e max
 **/
int get_random_int(int min, int max) {
    return rand() % (max - min + 1) + min;
}

/**
 * Genera un numero casuale double tra min e max
 **/
double get_random_double(double min, double max) {
    return (double)rand() / RAND_MAX * (max - min) + min;
}

/**
 * Genera una posizione casuale
 **/
position get_random_position(double min, double max) {
    position p;
    p.x = get_random_double(min, max);
    p.y = get_random_double(min, max);
    return p;
}

/**
 * Calcola la distanza tra due posizioni
 **/
double get_distance(position p1, position p2) {
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

/* This function will be used to wait for a simulated action (travel, load, unload, ...)
 * The function will resume automatically if the nanosleep is interrupted by a signal (by recalling nanosleep with the remaining time)

 * @param wait_time: the time to wait in seconds
*/
int wait_for_action(double wait_time) {

    struct timespec remaining_time;
	int nanosleep_res;

    if (ctx->stop == 1) {
        return 0;
    }

    do {
        /* Reset errno error */
        errno = 0; 
        
		/* Separate seconds and nanoseconds (because of how nanosleep works)*/
		long seconds = (long) wait_time;
        long nanoseconds = (long) ((wait_time - seconds) * 1000000000L);

		nanosleep_res = nanosleep((const struct timespec[]){{seconds, nanoseconds}}, &remaining_time);

		/* Check if nanosleep was completed (we already waited for wait_time seconds )*/
        if (nanosleep_res == 0) {
            break;
        }

		/* Otherwise compute the remaining time */
        wait_time = remaining_time.tv_sec + (double)remaining_time.tv_nsec/1000000000L;
    } while (nanosleep_res == -1 && errno == EINTR && ctx->stop == 0);
    
    if (errno != EINTR && errno != 0) {
        log_lev(L_ERROR, "nanosleep failed with errno %d", errno);
        return -1;
    }

    return 0;
}

