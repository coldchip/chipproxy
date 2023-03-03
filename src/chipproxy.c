#include <sys/time.h>
#include <stddef.h>
#include "chipproxy.h"

uint32_t chipproxy_get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000) / 1000;
}