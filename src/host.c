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
		list_insert(list_end(&host->peers), peer);

		return peer;
	}
	return NULL;
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