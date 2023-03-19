#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "log.h"

ProxyLog *chipproxy_log_create() {
	ProxyLog *log = malloc(sizeof(ProxyLog));
	if(!log) {
		return NULL;
	}

	log->fp = fopen("/tmp/chipproxy.log", "wb");
	return log;
}

void chipproxy_log_add(ProxyLog *log, const char *format, ...) {
	va_list args;
	va_start(args, format);

	vfprintf(log->fp, format, args);

	fflush(log->fp);
}

void chipproxy_log_free(ProxyLog *log) {
	fclose(log->fp);
	free(log);
}