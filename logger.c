#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include "logger.h"
#include "common.h"

#define DATETIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define DATE_FORMAT "%Y-%m-%d"
#define TIME_FORMAT "%H:%M:%S"

char * getTimeString();
char * getLevelString(char level);

FILE * logfile = NULL;
char default_loglevel = L_INFO;
char enabled_levels = L_ERROR | L_WARNING | L_INFO;
char opt_flags = LOG_DEFAULT;

int finit_log_file(FILE * stream, char flags) {
	if (logfile != NULL) close_log();

	opt_flags = flags;
	logfile = stream;
    
	if (opt_flags & LOG_WRITE_INIT_DATETIME) {
		char * datetime = getTimeString(DATETIME_FORMAT);

		fprintf(logfile, "[LOG STARTED AT %s]\n\n", datetime);
		free(datetime);
        
		if(opt_flags & LOG_AUTO_FLUSH) fflush(logfile);
	}
	return 0;
}

int init_log_file(char * filename, char flags) {
	FILE * stream = fopen(filename, (flags & LOG_CLEAR_ON_INIT) ? "w" : "a");
	if (stream == NULL) {
		return 1; /* error */
	}
	finit_log_file(stream, flags);
	return 0;
}

char * getTimeString(char * format) {
	sigset_t old_mask = block_signals(1, SIGALRM);
	time_t dateAndTime;
	char * buff;
	struct tm * tm_info;
	buff = malloc(sizeof(char) * 26);
	time(&dateAndTime);
	tm_info = localtime(&dateAndTime);
	strftime(buff, 26, format, tm_info);
	reset_signals(old_mask);
	return buff;
}

void set_default_log_level(char level) {
	default_loglevel = level;
}

void set_enabled_log_levels(char levels) {
	enabled_levels = levels;
}

void vlog_write(char level, const char * fmt, va_list args) {
	if (((enabled_levels & level) == 0) && (opt_flags & LOG_USE_LEVEL)) return;
	if ((opt_flags & LOG_DATE) || (opt_flags & LOG_TIME) || (opt_flags & LOG_USE_LEVEL)) {
		fprintf(logfile, "[ ");
		if (opt_flags & LOG_DATE) {
			char * datetime = getTimeString(DATE_FORMAT);
			fprintf(logfile, "%s ", datetime);
			free(datetime);
		}
		if (opt_flags & LOG_TIME) {
			char * datetime = getTimeString(TIME_FORMAT);
			fprintf(logfile, "%s ", datetime);
			free(datetime);
		}
		if (opt_flags & LOG_USE_LEVEL) {
			char * lev = getLevelString(level);
			fprintf(logfile, "%s ", lev);
		}
		fprintf(logfile, "]: ");
	}
	vfprintf(logfile, fmt, args);
	fputc('\n', logfile);
	if (opt_flags & LOG_AUTO_FLUSH) fflush(logfile);
}

void log_write(const char * fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vlog_write(default_loglevel, fmt, args);
	va_end(args);
}

void log_lev(char level, const char * fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vlog_write(level, fmt, args);
	va_end(args);
}

char * getLevelString(char level) {
	switch (level) {
        case L_ERROR:
		return "ERROR";
        case L_WARNING:
		return "WARNING";
        case L_INFO:
		return "INFO";
        case L_DEBUG:
		return "DEBUG";
        case L_TRACE:
		return "TRACE";
        default:
		return "";
	}
}

void close_log() {
	if(logfile != NULL) {
		if (opt_flags & LOG_WRITE_CLOSE_DATETIME) {
			char * datetime = getTimeString(DATETIME_FORMAT);

			fprintf(logfile, "\n[LOG ENDED AT %s]\n\n", datetime);
			free(datetime);
		}
		fflush(logfile);
		fclose(logfile);
	}
}
