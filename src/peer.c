#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "peer.h"
#include "chipproxy.h"

ProxyPeer *chipproxy_peer_create(int fdin, int fdout) {
	ProxyPeer *peer = malloc(sizeof(ProxyPeer));
	if(!peer) {
		return NULL;
	}
	peer->state = PEER_CONNECTING;
	peer->fdin = fdin;
	peer->fdout = fdout;
	peer->inbound = chipproxy_bio_create(16384 * 2);
	peer->outbound = chipproxy_bio_create(16384 * 2);
	peer->connect_start = chipproxy_get_time();
	return peer;
}

void chipproxy_peer_read(ProxyPeer *peer) {
	char buf[4096];
	int r = read(peer->fdin, (char*)&buf, sizeof(buf));
	if(r <= 0) {
		if(errno != EAGAIN && errno != EWOULDBLOCK) {
			chipproxy_peer_disconnect(peer);
			return;
		}
	}

	chipproxy_bio_write(peer->inbound, buf, r);
}

void chipproxy_peer_write(ProxyPeer *peer) {
	int size = chipproxy_bio_read_available(peer->outbound);

	if(size > 0) {
		int w = write(peer->fdin, chipproxy_bio_get_buffer(peer->outbound), size);
		if(w <= 0) {
			if(errno != EAGAIN && errno != EWOULDBLOCK) {
				chipproxy_peer_disconnect(peer);
				return;
			}
		}

		chipproxy_bio_read(peer->outbound, NULL, w);
	}
}

void chipproxy_peer_server_read(ProxyPeer *peer) {
	char buf[4096];
	int r = read(peer->fdout, (char*)&buf, sizeof(buf));
	if(r <= 0) {
		if(errno != EAGAIN && errno != EWOULDBLOCK) {
			chipproxy_peer_disconnect(peer);
			return;
		}
	}

	chipproxy_bio_write(peer->outbound, buf, r);
}

void chipproxy_peer_server_write(ProxyPeer *peer) {
	int size = chipproxy_bio_read_available(peer->inbound);

	if(size > 0) {
		int w = write(peer->fdout, chipproxy_bio_get_buffer(peer->inbound), size);
		if(w <= 0) {
			if(errno != EAGAIN && errno != EWOULDBLOCK) {
				chipproxy_peer_disconnect(peer);
				return;
			}
		}

		chipproxy_bio_read(peer->inbound, NULL, w);
	}
}

void chipproxy_peer_disconnect(ProxyPeer *peer) {
	peer->state = PEER_DISCONNECTING;
	close(peer->fdin);
	close(peer->fdout);
}

void chipproxy_peer_free(ProxyPeer *peer) {
	chipproxy_bio_free(peer->inbound);
	chipproxy_bio_free(peer->outbound);
	list_remove(&peer->node);
	free(peer);
}