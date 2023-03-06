#ifndef CHIPPROXY_H
#define CHIPPROXY_H

#include <stdint.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

uint32_t      chipproxy_get_time();
char         *chipproxy_format_bytes(uint64_t bytes);
void          chipproxy_log(const char *format, ...);
void          chipproxy_warn(const char *format, ...);
void          chipproxy_error(const char *format, ...);

#endif