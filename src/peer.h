#ifndef PEER_H
#define PEER_H

#include "list.h"
#include "bio.h"
#include <stdbool.h>
#include <arpa/inet.h> 

typedef enum {
	PEER_CONNECTING,
	PEER_CONNECTED,
	PEER_DISCONNECTING
} ProxyPeerState;

typedef struct _ProxyPeer {
	ListNode node;
	struct sockaddr_in addr;
	ProxyPeerState state;
	int fdin;
	int fdout;
	ProxyBIO *inbound;
	ProxyBIO *outbound;
	int connect_start;
	int test;
} ProxyPeer;

ProxyPeer   *chipproxy_peer_create(int fdin, int fdout);
void         chipproxy_peer_read(ProxyPeer *peer);
void         chipproxy_peer_write(ProxyPeer *peer);
void         chipproxy_peer_server_read(ProxyPeer *peer);
void         chipproxy_peer_server_write(ProxyPeer *peer);
void         chipproxy_peer_disconnect(ProxyPeer *peer);
void         chipproxy_peer_free(ProxyPeer *peer);

#endif