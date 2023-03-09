#ifndef BUFFER_H
#define BUFFER_H

typedef struct _ProxyBIO {
	char *data;
	int size;
	int max;
} ProxyBIO;

ProxyBIO      *chipproxy_bio_create(int max);
char          *chipproxy_bio_get_buffer(ProxyBIO *bucket);
int            chipproxy_bio_read_available(ProxyBIO *bucket);
int            chipproxy_bio_write_available(ProxyBIO *bucket);
int            chipproxy_bio_write(ProxyBIO *bucket, void *buf, int size);
int            chipproxy_bio_read(ProxyBIO *bucket, void *buf, int size);
void           chipproxy_bio_free(ProxyBIO *bucket);

#endif