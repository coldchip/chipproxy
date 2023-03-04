#ifndef SOCKET_H
#define SOCKET_H

#include "list.h"
#include "peer.h"
#include <stdbool.h>

typedef struct _ProxyHost {
	int fd;
	List peers;
} ProxyHost;

ProxyHost         *chipproxy_host_create();
bool               chipproxy_host_setopt_buffer(ProxyHost *socket, int send, int recv);
bool               chipproxy_host_set_non_block(int fd);
bool               chipproxy_host_bind(ProxyHost *host, const char *ip, int port);
ProxyPeer         *chipproxy_host_accept(ProxyHost *host);
void               chipproxy_host_free(ProxyHost *host);

#endif