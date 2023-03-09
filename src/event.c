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
int   bind_port[] = {4655, 4656, 4657, 4658};

char *pass_ip[]   = {"127.0.0.1", "127.0.0.1", "127.0.0.1", "3.0.7.3"};
int   pass_port[] = {6060, 80, 80, 443};

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
	struct timeval tv;
	fd_set rdset, wdset;
	int last_display = 0;

	while(1) {
		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		FD_ZERO(&rdset);
		FD_ZERO(&wdset);

		int max = 0;

		for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
			ProxyHost *host = (ProxyHost*)h;

			FD_SET(host->fd, &rdset);
			max = MAX(max, host->fd);
		}

		for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
			ProxyHost *host = (ProxyHost*)h;
			for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
				ProxyPeer *peer = (ProxyPeer*)i;

				if(chipproxy_bio_write_available(peer->inbound) > 0) {
					FD_SET(peer->fdin, &rdset);
					max = MAX(max, peer->fdin);
				}

				if(chipproxy_bio_read_available(peer->inbound) > 0) {
					FD_SET(peer->fdout, &wdset);
					max = MAX(max, peer->fdout);
				}

				if(chipproxy_bio_write_available(peer->outbound) > 0) {
					FD_SET(peer->fdout, &rdset);
					max = MAX(max, peer->fdout);
				}

				if(chipproxy_bio_read_available(peer->outbound) > 0) {
					FD_SET(peer->fdin, &wdset);
					max = MAX(max, peer->fdin);
				}

				if(!peer->connected) {
					FD_SET(peer->fdout, &rdset);
					FD_SET(peer->fdout, &wdset);
					max = MAX(max, peer->fdout);
				}
			}
		}

		if(select(max + 1, &rdset, &wdset, NULL, &tv) >= 0) {
			if(chipproxy_get_time() - last_display > 1) {
				char tx_c[64];
				char rx_c[64];

				strcpy(tx_c, chipproxy_format_bytes(tx));
				strcpy(rx_c, chipproxy_format_bytes(rx));

				int count = 0;
				for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
					ProxyHost *host = (ProxyHost*)h;
					count += (int)list_size(&host->peers);
				}

				chipproxy_log("tx: %s rx: %s peers: %i", tx_c, rx_c, count);

				last_display = chipproxy_get_time();
			}

			for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
				ProxyHost *host = (ProxyHost*)h;

				if(FD_ISSET(host->fd, &rdset) && max < FD_SETSIZE - 16) {
					ProxyPeer *peer = chipproxy_host_accept(host);
					if(peer) {
						peer->fdout = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
						if(peer->fdout == -1) {
							chipproxy_peer_free(peer);
							break;
						}
						if(!chipproxy_host_set_non_block(peer->fdout)) {
							chipproxy_error("unable to set to non blocking %i", peer->fdout);
						}
					}
				}
			}

			for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
				ProxyHost *host = (ProxyHost*)h;
			
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

						rx += r;

						chipproxy_bio_write(peer->inbound, buf, r);
					}

					if(FD_ISSET(peer->fdin, &wdset)) {
						int size = chipproxy_bio_read_available(peer->outbound);

						if(size > 0) {
							int w = write(peer->fdin, chipproxy_bio_get_buffer(peer->outbound), size);
							if(w <= 0) {
								if(errno != EAGAIN && errno != EWOULDBLOCK) {
									chipproxy_peer_free(peer);
								}
								break;
							}

							chipproxy_bio_read(peer->outbound, NULL, w);
						}
					}

				}
			}

			for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
				ProxyHost *host = (ProxyHost*)h;

				for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
					ProxyPeer *peer = (ProxyPeer*)i;

					if(!peer->connected) {
						if(connect(peer->fdout, (struct sockaddr*)&host->proxy_pass, sizeof(host->proxy_pass)) != -1) {
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

							tx += r;

							chipproxy_bio_write(peer->outbound, buf, r);
						}

						if(FD_ISSET(peer->fdout, &wdset)) {
							int size = chipproxy_bio_read_available(peer->inbound);

							if(size > 0) {
								int w = write(peer->fdout, chipproxy_bio_get_buffer(peer->inbound), size);
								if(w <= 0) {
									if(errno != EAGAIN && errno != EWOULDBLOCK) {
										chipproxy_peer_free(peer);
									}
									break;
								}

								chipproxy_bio_read(peer->inbound, NULL, w);
							}
						}
					}
				}
			}
		}
	}
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