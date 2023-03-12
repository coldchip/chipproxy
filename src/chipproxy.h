#ifndef CHIPPROXY_H
#define CHIPPROXY_H

#include <stdint.h>
#include "peer.h"

#define CHIPPROXY_VERSION "v1.0.1"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

void          chipproxy_setup();
void          chipproxy_loop();
void          chipproxy_cleanup();

void          chipproxy_print_stats();

void          chipproxy_event_peer_connect(ProxyPeer *peer);
void          chipproxy_event_peer_disconnect(ProxyPeer *peer);

uint32_t      chipproxy_get_time();
char         *chipproxy_format_bytes(uint64_t bytes);
void          chipproxy_log(const char *format, ...);
void          chipproxy_warn(const char *format, ...);
void          chipproxy_error(const char *format, ...);

void          chipproxy_exit(int type);

#endif