#ifndef CHIPPROXY_H
#define CHIPPROXY_H

#include <stdint.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

uint32_t chipproxy_get_time();

#endif