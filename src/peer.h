#ifndef PEER_H
#define PEER_H

#include "list.h"
#include "bio.h"
#include <stdbool.h>

typedef enum {
	PEER_CONNECTED,
	PEER_CONNECTING,
	PEER_DISCONNECTING,
	PEER_DISCONNECTED
} ProxyPeerState;

typedef struct _ProxyPeer {
	ListNode node;
	ProxyPeerState state;
	int fdin;
	int fdout;
	ProxyBIO *inbound;
	ProxyBIO *outbound;
	int connect_start;
	int test;
} ProxyPeer;

ProxyPeer   *chipproxy_peer_create(int fdin, int fdout);
void         chipproxy_peer_disconnect(ProxyPeer *peer);
void         chipproxy_peer_free(ProxyPeer *peer);

#endif