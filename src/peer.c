#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "peer.h"
#include "chipproxy.h"

ProxyPeer *chipproxy_peer_create(int fdin) {
	ProxyPeer *peer = malloc(sizeof(ProxyPeer));
	peer->fdin = fdin;
	peer->inbound = chipproxy_bucket_create(16384 * 2);
	peer->outbound = chipproxy_bucket_create(16384 * 2);
	peer->connected = false;
	peer->connect_start = chipproxy_get_time();
	return peer;
}

void chipproxy_peer_free(ProxyPeer *peer) {
	close(peer->fdin);
	close(peer->fdout);
	chipproxy_bucket_free(peer->inbound);
	chipproxy_bucket_free(peer->outbound);
	list_remove(&peer->node);
	free(peer);
}