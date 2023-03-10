#ifndef EVENT_H
#define EVENT_H

#include <sys/socket.h>
#include "peer.h"

typedef enum {
	EVENT_OK,
	EVENT_DISCONNECT
} EventResult;

void chipproxy_init();
void chipproxy_setup();
void chipproxy_loop();

EventResult chipproxy_event_peer_connect(ProxyPeer *peer);
EventResult chipproxy_event_peer_read(ProxyPeer *peer);
EventResult chipproxy_event_peer_write(ProxyPeer *peer);
EventResult chipproxy_event_server_read(ProxyPeer *peer);
EventResult chipproxy_event_server_write(ProxyPeer *peer);
void chipproxy_event_peer_disconnect(ProxyPeer *peer);

void chipproxy_exit(int type);

#endif