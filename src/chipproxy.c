#include <sys/time.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "chipproxy.h"

uint32_t chipproxy_get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000) / 1000;
}

char *chipproxy_format_bytes(uint64_t bytes) {
	char *suffix[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"};
	char length = sizeof(suffix) / sizeof(suffix[0]);

	int i = 0;
	double dblBytes = bytes;

	if (bytes > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024) {
			dblBytes = bytes / 1024.0;
		}
	}

	static char output[200];
	sprintf(output, "%.02lf %s", dblBytes, suffix[i]);
	return output;
}

void chipproxy_log(const char *format, ...) {
	va_list args;
	va_start(args, format);

	printf("\033[0;36m[ChipProxy] ");
	vprintf(format, args);
	printf("\033[0m\n");
	
	va_end(args);
}

void chipproxy_warn(const char *format, ...) {
	va_list args;
	va_start(args, format);

	printf("\033[0;31m[ChipProxy] ");
	vprintf(format, args);
	printf("\033[0m\n");
	
	va_end(args);
}

void chipproxy_error(const char *format, ...) {
	va_list args;
	va_start(args, format);

	printf("\033[0;31m[ChipProxy] ");
	vprintf(format, args);
	printf("\033[0m\n");

	exit(1);
}