#ifndef __LOGGER_H
#define __LOGGER_H

/* Flag di base */
#define LOG_TIME 0x01
#define LOG_DATE 0x02
#define LOG_USE_LEVEL 0x04
#define LOG_WRITE_INIT_DATETIME 0x08
#define LOG_WRITE_CLOSE_DATETIME 0x10
#define LOG_CLEAR_ON_INIT 0x20
#define LOG_AUTO_FLUSH 0x40

/* Flag composte */
#define LOG_DEFAULT (LOG_CLEAR_ON_INIT | LOG_AUTO_FLUSH)
#define LOG_PERSISTENT (LOG_WRITE_CLOSE_DATETIME | LOG_WRITE_INIT_DATETIME | LOG_AUTO_FLUSH | LOG_TIME | LOG_DATE)
#define LOG_DEBUG (LOG_AUTO_FLUSH | LOG_USE_LEVEL | LOG_CLEAR_ON_INIT)
#define LOG_DATETIME (LOG_DATE | LOG_TIME)

/* Livelli di log */
#define L_INFO 0x01
#define L_DEBUG 0x02
#define L_ERROR 0x04
#define L_WARNING 0x08
#define L_TRACE 0x10

/* Usare prima di qualsiasi altra chiamata */
/* Inizializza il logger creando un file */
int init_log_file(char * filename, char flags);

/*
 * Alternativamente usare questo prima di qualsiasi altra chiamata
 * Inizializza il logger partendo da uno stream già creato Utile per
 * usare stdout
 */
int finit_log_file(FILE * stream, char flags);

/*
 * Imposta il livello di log di default usato nella funzione log_write
 * Assicurarsi di abilitare la flag LOG_USE_LEVEL
 */
void set_default_log_level(char level);

/*
 * levels è una flag, usare l'or bitwise per scegliere i livelli abilitati
 * i livelli non abilitati saranno ignorati automaticamente
 */
void set_enabled_log_levels(char levels);

/*
 * Stampa sul file di log usando il livello di log di default Questa
 * funzione si usa come una printf
 */
void log_write(const char * fmt, ...);

/*
 * Stampa sul file di log usando il livello specificato Questa
 * funzione si usa come una printf (eccetto per il parametro level)
 */
void log_lev(char level, const char * fmt, ...);

/*
 * Chiude il file di log, meglio non usarlo se si usa lo stream stdout
 */
void close_log();

#endif /* __LOGGER_H */
