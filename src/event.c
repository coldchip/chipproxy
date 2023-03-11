#include <stdbool.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <arpa/inet.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "event.h"
#include "peer.h"
#include "bio.h"
#include "host.h"
#include "list.h"
#include "config.h"
#include "chipproxy.h"

char *bind_ip[]   = {"0.0.0.0", "0.0.0.0", "0.0.0.0", "0.0.0.0"};
int   bind_port[] = {80, 443, 8080, 10022};

char *pass_ip[]   = {"127.0.0.1", "3.0.7.3", "127.0.0.1", "127.0.0.1"};
int   pass_port[] = {5001, 443, 5001, 22};

uint64_t tx = 0;
uint64_t rx = 0;

List hosts;

void chipproxy_init() {
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, chipproxy_exit);
	signal(SIGQUIT, chipproxy_exit);
	signal(SIGTERM, chipproxy_exit);
	signal(SIGHUP, chipproxy_exit);

	chipproxy_setup();
	chipproxy_loop();
}

void chipproxy_setup() {
	list_clear(&hosts);

	for(int i = 0; i < sizeof(bind_ip) / sizeof(bind_ip[0]); i++) {
		ProxyHost *host = chipproxy_host_create();
		if(!host) {
			chipproxy_error("unable to set socket to non blocking mode\n");
		}

		if(!chipproxy_host_setopt_buffer(host, 512000, 512000)) {
			chipproxy_error("unable to set socket snd/rcv buffers");
		}

		if(!chipproxy_host_bind(host, bind_ip[i], bind_port[i])) {
			chipproxy_error("unable to bind");
		}

		host->proxy_pass.sin_family = AF_INET;
		host->proxy_pass.sin_addr.s_addr = inet_addr(pass_ip[i]);
		host->proxy_pass.sin_port = htons(pass_port[i]);

		list_insert(list_end(&hosts), host);
	}
}

void chipproxy_loop() {
	while(true) {
		ProxyEvent event;
		if(chipproxy_host_service(&hosts, &event) > 0) {
			ProxyPeer *peer = event.peer;

			switch(event.type) {
				case PROXY_EVENT_CONNECT: {
					chipproxy_event_peer_connect(peer);
				}
				break;

				case PROXY_EVENT_CLIENT_READ: {
					chipproxy_event_peer_read(peer);
				}
				break;

				case PROXY_EVENT_CLIENT_WRITE: {
					chipproxy_event_peer_write(peer);
				}
				break;

				case PROXY_EVENT_SERVER_READ: {
					chipproxy_event_server_read(peer);
				}
				break;

				case PROXY_EVENT_SERVER_WRITE: {
					chipproxy_event_server_write(peer);
				}
				break;

				case PROXY_EVENT_DISCONNECT: {
					chipproxy_event_peer_disconnect(peer);
				}
				break;
			}
		}
	}
}

void chipproxy_event_peer_connect(ProxyPeer *peer) {
	chipproxy_log("peer %p has connected", peer);
}

void chipproxy_event_peer_read(ProxyPeer *peer) {
	char buf[4096];
	int r = read(peer->fdin, (char*)&buf, sizeof(buf));
	if(r <= 0) {
		if(errno != EAGAIN && errno != EWOULDBLOCK) {
			chipproxy_peer_disconnect(peer);
			return;
		}
	}

	rx += r;

	chipproxy_bio_write(peer->inbound, buf, r);
}

void chipproxy_event_peer_write(ProxyPeer *peer) {
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

void chipproxy_event_server_read(ProxyPeer *peer) {
	char buf[4096];
	int r = read(peer->fdout, (char*)&buf, sizeof(buf));
	if(r <= 0) {
		if(errno != EAGAIN && errno != EWOULDBLOCK) {
			chipproxy_peer_disconnect(peer);
			return;
		}
	}

	tx += r;

	chipproxy_bio_write(peer->outbound, buf, r);
}

void chipproxy_event_server_write(ProxyPeer *peer) {
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

void chipproxy_event_peer_disconnect(ProxyPeer *peer) {
	chipproxy_log("peer %p has disconnected", peer);
}

void chipproxy_exit(int type) {
	if(type == 0) {}
	
	ListNode *h = list_begin(&hosts);
	while(h != list_end(&hosts)) {
		ProxyHost *host = (ProxyHost*)h;
		h = list_next(h);

		chipproxy_host_free(host);
	}

	chipproxy_log("terminating...");
	exit(0);
}