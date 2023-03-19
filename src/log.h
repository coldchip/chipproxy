#ifndef LOG_H
#define LOG_H

typedef struct _ProxyLog {
	FILE *fp;
} ProxyLog;

ProxyLog     *chipproxy_log_create();
void          chipproxy_log_add(ProxyLog *log, const char *format, ...);
void          chipproxy_log_free(ProxyLog *log);

#endif