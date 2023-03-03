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
#include "bucket.h"
#include "list.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

typedef struct _Peer {
	ListNode node;
	int fdin;
	int fdout;
	ProxyBucket *inbound;
	ProxyBucket *outbound;
	bool connected;
} Peer;

List peers;

bool chipproxy_socket_set_non_block(int fd) {
	int flags = fcntl(fd, F_GETFL);
	if(flags == -1) {
		return false;
	}

	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0) {
		return true;
	}

	return false;
}

void peer_free(Peer *peer) {
	close(peer->fdin);
	close(peer->fdout);
	chipproxy_bucket_free(peer->inbound);
	chipproxy_bucket_free(peer->outbound);
	list_remove(&peer->node);
	free(peer);
	printf("end\n");
}

int main(int argc, char const *argv[]) {
	/* code */
	signal(SIGPIPE, SIG_IGN);

	list_clear(&peers);

	int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	chipproxy_socket_set_non_block(fd);

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(char){1}, sizeof(int)) < 0){
		return 1;
	}

	if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &(char){1}, sizeof(int)) < 0){
		return 1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = inet_addr("0.0.0.0"); 
	addr.sin_port        = htons(4655);

	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
	servaddr.sin_port = htons(5001);

	if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) { 
		return 1;
	}

	if(listen(fd, 32) != 0) { 
		return 1;
	}

	struct timeval tv;
	fd_set rdset, wdset;

	while(1) {
		tv.tv_sec = 500;
		tv.tv_usec = 0;

		FD_ZERO(&rdset);
		FD_ZERO(&wdset);

		int max = fd;
		FD_SET(fd, &rdset);

		for(ListNode *i = list_begin(&peers); i != list_end(&peers); i = list_next(i)) {
			Peer *peer = (Peer*)i;

			if(chipproxy_bucket_write_available(peer->inbound) > 0) {
				FD_SET(peer->fdin, &rdset);
			}

			if(chipproxy_bucket_write_available(peer->outbound) > 0) {
				FD_SET(peer->fdout, &rdset);
			}

			if(chipproxy_bucket_read_available(peer->inbound) > 0) {
				FD_SET(peer->fdout, &wdset);
			}

			if(chipproxy_bucket_read_available(peer->outbound) > 0) {
				FD_SET(peer->fdin, &wdset);
			}

			// if(!peer->connected) {
			// 	FD_SET(peer->fdout, &wdset);
			// }

			max = MAX(max, peer->fdin);
			max = MAX(max, peer->fdout);
		}

		if(select(max + 1, &rdset, &wdset, NULL, &tv) >= 0) {
			printf("peersize: %li\n", list_size(&peers));

			if(FD_ISSET(fd, &rdset)) {
				struct sockaddr_in addr;

				int cfd = accept(fd, (struct sockaddr*)&addr, &(socklen_t){sizeof(addr)});
				if(cfd >= 0) {
					Peer *peer = malloc(sizeof(Peer));
					peer->fdin = cfd;
					peer->fdout = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
					peer->inbound = chipproxy_bucket_create(16384);
					peer->outbound = chipproxy_bucket_create(16384);
					peer->connected = false;

					if(!chipproxy_socket_set_non_block(peer->fdin)) {
						return 1;
					}

					if(!chipproxy_socket_set_non_block(peer->fdout)) {
						return 1;
					}

					list_insert(list_end(&peers), peer);
					continue;
				}
			}

			for(ListNode *i = list_begin(&peers); i != list_end(&peers); i = list_next(i)) {
				Peer *peer = (Peer*)i;

				if(!peer->connected) {
					if(connect(peer->fdout, (struct sockaddr *)&servaddr, sizeof(servaddr)) != -1) {
						peer->connected = true;
					}
					printf("dsssdssz %i\n", peer->connected);
					break;
				}

				if(FD_ISSET(peer->fdin, &rdset)) {
					printf("FDIN rd\n");
					char buf[8192];
					int r = read(peer->fdin, (char*)&buf, sizeof(buf));
					// for(int i = 0; i < r; ++i) {
					// 	printf("%c", buf[i] & 0xFF);
					// }
					// printf("\n");

					if(r <= 0) {
						if(errno != EAGAIN && errno != EWOULDBLOCK) {
							peer_free(peer);
						}
						break;
					}

					chipproxy_bucket_write(peer->inbound, buf, r);
				}

				if(FD_ISSET(peer->fdin, &wdset)) {
					printf("FDIN wd\n");
					int size = chipproxy_bucket_read_available(peer->outbound);

					if(size > 0) {
						int w = write(peer->fdin, chipproxy_bucket_get_buffer(peer->outbound), size);
						if(w <= 0) {
							if(errno != EAGAIN && errno != EWOULDBLOCK) {
								peer_free(peer);
							}
							break;
						}

						chipproxy_bucket_read(peer->outbound, NULL, w);
					}
				}

				if(FD_ISSET(peer->fdout, &rdset)) {
					printf("FDOUT rd\n");
					char buf[8192];
					int r = read(peer->fdout, (char*)&buf, sizeof(buf));
					// for(int i = 0; i < r; ++i) {
					// 	printf("%c", buf[i] & 0xFF);
					// }
					// printf("\n");
					if(r <= 0) {
						if(errno != EAGAIN && errno != EWOULDBLOCK) {
							peer_free(peer);
						}
						break;
					}

					chipproxy_bucket_write(peer->outbound, buf, r);
				}

				if(FD_ISSET(peer->fdout, &wdset)) {
					printf("FDOUT wd\n");
					int size = chipproxy_bucket_read_available(peer->inbound);

					if(size > 0) {
						int w = write(peer->fdout, chipproxy_bucket_get_buffer(peer->inbound), size);
						if(w <= 0) {
							if(errno != EAGAIN && errno != EWOULDBLOCK) {
								peer_free(peer);
							}
							break;
						}

						chipproxy_bucket_read(peer->inbound, NULL, w);
					}
				}
			}
		}

	}

	return 0;
}