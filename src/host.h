#ifndef SOCKET_H
#define SOCKET_H

#include "list.h"
#include "peer.h"
#include <stdbool.h>

typedef struct _ProxySocket {
	int fd;
	List peers;
} ProxySocket;

ProxySocket       *chipproxy_host_create();
bool               chipproxy_host_setopt_buffer(ProxySocket *socket, int send, int recv);
bool               chipproxy_host_set_non_block(int fd);
bool               chipproxy_host_bind(ProxySocket *host, const char *ip, int port);
ProxyPeer         *chipproxy_host_accept(ProxySocket *host);
void               chipproxy_host_free(ProxySocket *host);

#endif