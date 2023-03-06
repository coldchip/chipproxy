#ifndef CONFIG_H
#define CONFIG_H

typedef struct _HostConfig {
	char bind_ip[16];
	int bind_port;
	char pass_ip[16];
	int pass_port;
} HostConfig;

#endif