#ifndef BUCKET_H
#define BUCKET_H

typedef struct _ProxyBucket {
	char *data;
	int size;
	int max;
} ProxyBucket;

ProxyBucket     *chipproxy_bucket_create(int max);
char          *chipproxy_bucket_get_buffer(ProxyBucket *bucket);
int            chipproxy_bucket_read_available(ProxyBucket *bucket);
int            chipproxy_bucket_write_available(ProxyBucket *bucket);
int            chipproxy_bucket_write(ProxyBucket *bucket, void *buf, int size);
int            chipproxy_bucket_read(ProxyBucket *bucket, void *buf, int size);
void           chipproxy_bucket_free(ProxyBucket *bucket);

#endif