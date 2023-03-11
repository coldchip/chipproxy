#include "host.h"
#include "peer.h"
#include "chipproxy.h"
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <netinet/tcp.h>
#include <signal.h>
#include <errno.h>

ProxyHost *chipproxy_host_create() {
	ProxyHost *host = malloc(sizeof(ProxyHost));

	list_clear(&host->peers);

	int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	if(!chipproxy_host_set_non_block(fd)) {
		return NULL;
	}

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(char){1}, sizeof(int)) < 0){
		return NULL;
	}
	if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &(char){1}, sizeof(int)) < 0){
		return NULL;
	}

	host->fd = fd;
	host->last_service = 0;

	return host;
}

bool chipproxy_host_setopt_buffer(ProxyHost *socket, int send, int recv) {
	if(setsockopt(socket->fd, SOL_SOCKET, SO_SNDBUF, (char*)&send, (int)sizeof(send)) < 0){
		return false;
	}
	if(setsockopt(socket->fd, SOL_SOCKET, SO_RCVBUF, (char*)&recv, (int)sizeof(recv)) < 0){
		return false;
	}
	return true;
}

bool chipproxy_host_set_non_block(int fd) {
	int flags = fcntl(fd, F_GETFL);
	if(flags == -1) {
		return false;
	}

	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0) {
		return true;
	}

	return false;
}

bool chipproxy_host_bind(ProxyHost *host, const char *ip, int port) {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip); 
	addr.sin_port        = htons(port);

	if(bind(host->fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) { 
		return false;
	}

	if(listen(host->fd, 511) != 0) { 
		return false;
	}
	return true;
}

ProxyPeer *chipproxy_host_accept(ProxyHost *host) {
	struct sockaddr_in addr;

	int fdin = accept(host->fd, (struct sockaddr*)&addr, &(socklen_t){sizeof(addr)});
	if(fdin >= 0) {
		if(!chipproxy_host_set_non_block(fdin)) {
			chipproxy_log("unable to set socket to non blocking mode");
		}

		int fdout = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if(fdout == -1) {
			close(fdin);
			return NULL;
		}
		if(!chipproxy_host_set_non_block(fdout)) {
			chipproxy_error("unable to set to non blocking");
		}

		ProxyPeer *peer = chipproxy_peer_create(fdin, fdout);
		peer->state = PEER_CONNECTED;
		list_insert(list_end(&host->peers), peer);

		return peer;
	}
	return NULL;
}

int chipproxy_host_service(List *hosts, ProxyEvent *event) {
	ListNode *h = list_begin(hosts);
	while(h != list_end(hosts)) {
		ProxyHost *host = (ProxyHost*)h;
		h = list_next(h);

		ListNode *i = list_begin(&host->peers);
		while(i != list_end(&host->peers)) {
			ProxyPeer *peer = (ProxyPeer*)i;
			i = list_next(i);

			if(peer->state == PEER_CONNECTING) {
				if(chipproxy_get_time() - peer->connect_start > 15) {
					// connect() timeout
					chipproxy_peer_disconnect(peer);
				}
			}

			if(peer->state == PEER_DISCONNECTING) {
				peer->state = PEER_DISCONNECTED;

				event->type = PROXY_EVENT_DISCONNECT;
				event->peer = peer;
				return 1;
			}

			if(peer->state == PEER_DISCONNECTED) {
				chipproxy_peer_free(peer);
			}
		}
	}

	struct timeval tv = {
		.tv_sec = 0,
		.tv_usec = 500000
	};

	fd_set rdset, wdset;
	FD_ZERO(&rdset);
	FD_ZERO(&wdset);

	int max = 0;

	for(ListNode *h = list_begin(hosts); h != list_end(hosts); h = list_next(h)) {
		ProxyHost *host = (ProxyHost*)h;

		FD_SET(host->fd, &rdset);
		max = MAX(max, host->fd);

		for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
			ProxyPeer *peer = (ProxyPeer*)i;

			if(peer->state == PEER_CONNECTING) {
				/* proxy <==> server */
				FD_SET(peer->fdout, &wdset);
				max = MAX(max, peer->fdout);
			}

			if(peer->state == PEER_CONNECTED) {
				/* client <==> proxy */
				if(chipproxy_bio_write_available(peer->inbound) > 0) {
					FD_SET(peer->fdin, &rdset);
					max = MAX(max, peer->fdin);
				}

				if(chipproxy_bio_read_available(peer->outbound) > 0) {
					FD_SET(peer->fdin, &wdset);
					max = MAX(max, peer->fdin);
				}

				/* connected */
				if(chipproxy_bio_read_available(peer->inbound) > 0) {
					FD_SET(peer->fdout, &wdset);
					max = MAX(max, peer->fdout);
				}

				if(chipproxy_bio_write_available(peer->outbound) > 0) {
					FD_SET(peer->fdout, &rdset);
					max = MAX(max, peer->fdout);
				}
			}
		}
	}

	if(select(max + 1, &rdset, &wdset, NULL, &tv) >= 0) {
		for(ListNode *h = list_begin(hosts); h != list_end(hosts); h = list_next(h)) {
			ProxyHost *host = (ProxyHost*)h;

			if(chipproxy_get_time() - host->last_service > 1) {
				chipproxy_log("--------------------");
				chipproxy_log("host %p has %li peer(s)", host, list_size(&host->peers));
				chipproxy_log("--------------------");
				host->last_service = chipproxy_get_time();
			}

			/* client <==> proxy accept */
			if(FD_ISSET(host->fd, &rdset) && max < FD_SETSIZE - 16) {
				ProxyPeer *peer = chipproxy_host_accept(host);
				peer->state = PEER_CONNECTING;
				return 0;
			}

			for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
				ProxyPeer *peer = (ProxyPeer*)i;

				if(peer->state == PEER_CONNECTING) {
					/* proxy <==> server connect */
					if(FD_ISSET(peer->fdout, &wdset)) {
						if(connect(peer->fdout, (struct sockaddr*)&host->proxy_pass, sizeof(host->proxy_pass)) != -1) {
							peer->state = PEER_CONNECTED;

							event->type = PROXY_EVENT_CONNECT;
							event->peer = peer;
							return 1;
						}

						if(errno != EINPROGRESS && errno != EAGAIN) {
							chipproxy_peer_disconnect(peer);
							continue;
						}
					}
				}

				if(peer->state == PEER_CONNECTED) {
					/* client <==> proxy read */
					if(FD_ISSET(peer->fdin, &rdset)) {
						event->type = PROXY_EVENT_CLIENT_READ;
						event->peer = peer;
						return 1;
					}

					/* client <==> proxy write */
					if(FD_ISSET(peer->fdin, &wdset)) {
						event->type = PROXY_EVENT_CLIENT_WRITE;
						event->peer = peer;
						return 1;
					}

					/* proxy <==> server read */
					if(FD_ISSET(peer->fdout, &rdset)) {
						event->type = PROXY_EVENT_SERVER_READ;
						event->peer = peer;
						return 1;
					}

					/* proxy <==> server write */
					if(FD_ISSET(peer->fdout, &wdset)) {
						event->type = PROXY_EVENT_SERVER_WRITE;
						event->peer = peer;
						return 1;
					}
				}
			}
		}
	}

	return 0;
}

void chipproxy_host_free(ProxyHost *host) {
	ListNode *i = list_begin(&host->peers);
	while(i != list_end(&host->peers)) {
		ProxyPeer *peer = (ProxyPeer*)i;
		i = list_next(i);
		chipproxy_peer_free(peer);
	}

	close(host->fd);
	free(host);
}