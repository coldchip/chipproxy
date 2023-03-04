#include <stdio.h>
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
#include "bucket.h"
#include "host.h"
#include "list.h"
#include "chipproxy.h"

ProxyHost *host = NULL;

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
	host = chipproxy_host_create();
	if(!host) {
		printf("unable to set socket to non blocking mode\n");
		exit(1);
	}

	if(!chipproxy_host_bind(host, "0.0.0.0", 80)) {
		printf("unable to bind\n");
		exit(1);
	}

}

void chipproxy_loop() {
	struct timeval tv;
	fd_set rdset, wdset;

	while(1) {
		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		FD_ZERO(&rdset);
		FD_ZERO(&wdset);

		int max = host->fd;
		FD_SET(max, &rdset);

		for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
			ProxyPeer *peer = (ProxyPeer*)i;

			if(chipproxy_bucket_write_available(peer->inbound) > 0) {
				FD_SET(peer->fdin, &rdset);
			}

			if(chipproxy_bucket_read_available(peer->inbound) > 0) {
				FD_SET(peer->fdout, &wdset);
			}

			if(chipproxy_bucket_write_available(peer->outbound) > 0) {
				FD_SET(peer->fdout, &rdset);
			}

			if(chipproxy_bucket_read_available(peer->outbound) > 0) {
				FD_SET(peer->fdin, &wdset);
			}

			if(!peer->connected) {
				FD_SET(peer->fdout, &rdset);
				FD_SET(peer->fdout, &wdset);
			}

			max = MAX(max, peer->fdin);
			max = MAX(max, peer->fdout);
		}

		if(select(max + 1, &rdset, &wdset, NULL, &tv) >= 0) {
			if(FD_ISSET(host->fd, &rdset)) {
				ProxyPeer *peer = chipproxy_host_accept(host);
				if(peer) {
					peer->fdout = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
					if(!chipproxy_host_set_non_block(peer->fdout)) {
						printf("unable to set to non blocking\n");
						exit(1);
					}
					continue;
				}
			}

			for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
				ProxyPeer *peer = (ProxyPeer*)i;

				if(FD_ISSET(peer->fdin, &rdset)) {
					char buf[4096];
					int r = read(peer->fdin, (char*)&buf, sizeof(buf));
					if(r <= 0) {
						if(errno != EAGAIN && errno != EWOULDBLOCK) {
							chipproxy_peer_free(peer);
						}
						break;
					}

					chipproxy_bucket_write(peer->inbound, buf, r);
				}

				if(FD_ISSET(peer->fdin, &wdset)) {
					int size = chipproxy_bucket_read_available(peer->outbound);

					if(size > 0) {
						int w = write(peer->fdin, chipproxy_bucket_get_buffer(peer->outbound), size);
						if(w <= 0) {
							if(errno != EAGAIN && errno != EWOULDBLOCK) {
								chipproxy_peer_free(peer);
							}
							break;
						}

						chipproxy_bucket_read(peer->outbound, NULL, w);
					}
				}

			}

			for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
				ProxyPeer *peer = (ProxyPeer*)i;

				if(!peer->connected) {
					struct sockaddr_in servaddr;
					servaddr.sin_family = AF_INET;
					servaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
					servaddr.sin_port = htons(5001);

					if(connect(peer->fdout, (struct sockaddr *)&servaddr, sizeof(servaddr)) != -1) {
						peer->connected = true;
					}

					if(chipproxy_get_time() - peer->connect_start > 15) {
						chipproxy_peer_free(peer);
						break;
					}
				} else {
					if(FD_ISSET(peer->fdout, &rdset)) {
						char buf[4096];
						int r = read(peer->fdout, (char*)&buf, sizeof(buf));
						if(r <= 0) {
							if(errno != EAGAIN && errno != EWOULDBLOCK) {
								chipproxy_peer_free(peer);
							}
							break;
						}

						chipproxy_bucket_write(peer->outbound, buf, r);
					}

					if(FD_ISSET(peer->fdout, &wdset)) {
						int size = chipproxy_bucket_read_available(peer->inbound);

						if(size > 0) {
							int w = write(peer->fdout, chipproxy_bucket_get_buffer(peer->inbound), size);
							if(w <= 0) {
								if(errno != EAGAIN && errno != EWOULDBLOCK) {
									chipproxy_peer_free(peer);
								}
								break;
							}

							chipproxy_bucket_read(peer->inbound, NULL, w);
						}
					}
				}
			}
		}
	}
}

void chipproxy_exit(int type) {
	if(type == 0) {}
	
	chipproxy_host_free(host);

	printf("terminating... \n");
	exit(0);
}