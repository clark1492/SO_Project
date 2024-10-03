#ifndef NAVI22_SHM_UTILS_H
#define NAVI22_SHM_UTILS_H

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "common.h"
#include "logger.h"

#define SHM_PERM_FLAG 0744


void initialize_ipc(context *ctx);
void free_ipc();
void get_ipcs();


void detach_context();

pid_t create_process();
void terminate_process();

void attach_shared_memory();
void detach_shared_memory();


#endif
