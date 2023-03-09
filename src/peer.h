#ifndef PEER_H
#define PEER_H

#include "list.h"
#include "bio.h"
#include <stdbool.h>

typedef struct _ProxyPeer {
	ListNode node;
	int fdin;
	int fdout;
	ProxyBIO *inbound;
	ProxyBIO *outbound;
	bool connected;
	int connect_start;
} ProxyPeer;

ProxyPeer   *chipproxy_peer_create(int fdin);
void         chipproxy_peer_free(ProxyPeer *peer);

#endif