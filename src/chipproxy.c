#include <sys/time.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <arpa/inet.h> 
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "peer.h"
#include "bio.h"
#include "host.h"
#include "list.h"
#include "config.h"
#include "chipproxy.h"

bool quit = false;

char *bind_ip[]   = {"0.0.0.0", "0.0.0.0", "0.0.0.0", "0.0.0.0"};
int   bind_port[] = {80, 443, 8080, 10022};

char *pass_ip[]   = {"127.0.0.1", "3.0.7.3", "127.0.0.1", "127.0.0.1"};
int   pass_port[] = {5001, 443, 5001, 22};

uint64_t tx = 0;
uint64_t rx = 0;

List hosts;

int main(int argc, char const *argv[]) {
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, chipproxy_exit);
	signal(SIGQUIT, chipproxy_exit);
	signal(SIGTERM, chipproxy_exit);
	signal(SIGHUP, chipproxy_exit);

	chipproxy_log("ChipProxy %s", CHIPPROXY_VERSION);

	chipproxy_setup();
	chipproxy_loop();
	chipproxy_cleanup();
}

void chipproxy_setup() {
	list_clear(&hosts);

	for(int i = 0; i < sizeof(bind_ip) / sizeof(bind_ip[0]); i++) {
		ProxyHost *host = chipproxy_host_create();
		if(!host) {
			chipproxy_error("device out of memory");
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
	int last_service = 0;

	struct timeval tv;
	fd_set rdset, wdset;

	while(!quit) {
		if(chipproxy_get_time() - last_service >= 1) {
			chipproxy_print_stats();
			last_service = chipproxy_get_time();
		}

		tv.tv_sec = 0;
		tv.tv_usec = 250000;

		FD_ZERO(&rdset);
		FD_ZERO(&wdset);

		int max = 0;

		for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
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
			for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
				ProxyHost *host = (ProxyHost*)h;

				/* client <==> proxy accept */
				if(FD_ISSET(host->fd, &rdset) && max < FD_SETSIZE - 16) {
					ProxyPeer *peer = chipproxy_host_accept(host);
					peer->state = PEER_CONNECTING;
					chipproxy_event_peer_connect(peer);
				}

				for(ListNode *i = list_begin(&host->peers); i != list_end(&host->peers); i = list_next(i)) {
					ProxyPeer *peer = (ProxyPeer*)i;

					if(peer->state == PEER_CONNECTING) {
						/* proxy <==> server connect */
						if(FD_ISSET(peer->fdout, &wdset)) {
							if(connect(peer->fdout, (struct sockaddr*)&host->proxy_pass, sizeof(host->proxy_pass)) != -1) {
								peer->state = PEER_CONNECTED;
							} else if(errno != EINPROGRESS && errno != EAGAIN) {
								chipproxy_peer_disconnect(peer);
								continue;
							}
						}
					}

					if(peer->state == PEER_CONNECTED) {
						/* client <==> proxy read */
						if(FD_ISSET(peer->fdin, &rdset)) {
							chipproxy_peer_read(peer);
						}

						/* client <==> proxy write */
						if(FD_ISSET(peer->fdin, &wdset)) {
							chipproxy_peer_write(peer);
						}

						/* proxy <==> server read */
						if(FD_ISSET(peer->fdout, &rdset)) {
							chipproxy_peer_server_read(peer);
						}

						/* proxy <==> server write */
						if(FD_ISSET(peer->fdout, &wdset)) {
							chipproxy_peer_server_write(peer);
						}
					}
				}
			}
		}

		ListNode *h = list_begin(&hosts);
		while(h != list_end(&hosts)) {
			ProxyHost *host = (ProxyHost*)h;
			h = list_next(h);

			ListNode *i = list_begin(&host->peers);
			while(i != list_end(&host->peers)) {
				ProxyPeer *peer = (ProxyPeer*)i;
				i = list_next(i);

				if(peer->state == PEER_CONNECTING) {
					if(chipproxy_get_time() - peer->connect_start > 10) {
						// connect() timeout
						chipproxy_peer_disconnect(peer);
					}
				}

				if(peer->state == PEER_DISCONNECTING) {
					chipproxy_event_peer_disconnect(peer);
					chipproxy_peer_free(peer);
				}
			}
		}
	}
}

void chipproxy_cleanup() {
	ListNode *h = list_begin(&hosts);
	while(h != list_end(&hosts)) {
		ProxyHost *host = (ProxyHost*)h;
		h = list_next(h);

		chipproxy_host_free(host);
	}
}

void chipproxy_print_stats() {
	int count = 0;
	for(ListNode *h = list_begin(&hosts); h != list_end(&hosts); h = list_next(h)) {
		ProxyHost *host = (ProxyHost*)h;
		count += list_size(&host->peers);
	}
	chipproxy_log("peers: %li", count);
}

void chipproxy_event_peer_connect(ProxyPeer *peer) {
	chipproxy_log("peer %p has connected", peer);
}

void chipproxy_event_peer_disconnect(ProxyPeer *peer) {
	chipproxy_log("peer %p has disconnected", peer);
}

uint32_t chipproxy_get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000) / 1000;
}

char *chipproxy_format_bytes(uint64_t bytes) {
	char *suffix[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"};
	char length = sizeof(suffix) / sizeof(suffix[0]);

	int i = 0;
	double dblBytes = bytes;

	if (bytes > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024) {
			dblBytes = bytes / 1024.0;
		}
	}

	static char output[200];
	sprintf(output, "%.02lf %s", dblBytes, suffix[i]);
	return output;
}

void chipproxy_log(const char *format, ...) {
	va_list args;
	va_start(args, format);

	printf("\033[0;36m[ChipProxy] ");
	vprintf(format, args);
	printf("\033[0m\n");
	
	va_end(args);
}

void chipproxy_warn(const char *format, ...) {
	va_list args;
	va_start(args, format);

	printf("\033[0;31m[ChipProxy] ");
	vprintf(format, args);
	printf("\033[0m\n");
	
	va_end(args);
}

void chipproxy_error(const char *format, ...) {
	va_list args;
	va_start(args, format);

	printf("\033[0;31m[ChipProxy] ");
	vprintf(format, args);
	printf("\033[0m\n");

	exit(1);
}

void chipproxy_exit(int type) {
	quit = true;
	chipproxy_log("terminating...");	
}