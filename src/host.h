#ifndef SOCKET_H
#define SOCKET_H

#include "list.h"
#include "peer.h"
#include <stdbool.h>
#include <arpa/inet.h> 

typedef struct _ProxyHost {
	ListNode node;
	int fd;
	List peers;
	struct sockaddr_in proxy_pass;
	int last_service;
} ProxyHost;

typedef enum {
	PROXY_EVENT_CONNECT,
	PROXY_EVENT_CLIENT_READ,
	PROXY_EVENT_CLIENT_WRITE,
	PROXY_EVENT_SERVER_READ,
	PROXY_EVENT_SERVER_WRITE,
	PROXY_EVENT_DISCONNECT
} ProxyEventType;

typedef struct _ProxyEvent {
	ProxyPeer *peer;
	ProxyEventType type;
} ProxyEvent;

ProxyHost         *chipproxy_host_create();
bool               chipproxy_host_setopt_buffer(ProxyHost *socket, int send, int recv);
bool               chipproxy_host_set_non_block(int fd);
bool               chipproxy_host_bind(ProxyHost *host, const char *ip, int port);
ProxyPeer         *chipproxy_host_accept(ProxyHost *host);
int                chipproxy_host_service(List *hosts, ProxyEvent *event);
void               chipproxy_host_free(ProxyHost *host);

#endif