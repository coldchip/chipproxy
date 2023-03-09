#include <stdlib.h>
#include <string.h>
#include "bio.h"
#include "chipproxy.h"

ProxyBIO *chipproxy_bio_create(int max) {
	ProxyBIO *bucket = malloc(sizeof(ProxyBIO));
	bucket->data = malloc(sizeof(char) * 1);
	bucket->size = 0;
	bucket->max = max;
	return bucket;
}

char *chipproxy_bio_get_buffer(ProxyBIO *bucket) {
	return bucket->data;
}

int chipproxy_bio_read_available(ProxyBIO *bucket) {
	return bucket->size;
}

int chipproxy_bio_write_available(ProxyBIO *bucket) {
	return bucket->max - bucket->size;
}

int chipproxy_bio_write(ProxyBIO *bucket, void *buf, int size) {
	bucket->data = realloc(bucket->data, bucket->size + size);
	memcpy(bucket->data + bucket->size, buf, size);
	bucket->size += size;

	return size;
}

int chipproxy_bio_read(ProxyBIO *bucket, void *buf, int size) {
	size = MIN(bucket->size, size);

	if(buf) {
		memcpy(buf, bucket->data, size);
	}
	memmove(bucket->data, bucket->data + size, bucket->size - size);
	bucket->size -= size;
	bucket->data = realloc(bucket->data, bucket->size);

	return size;
}

void chipproxy_bio_free(ProxyBIO *bucket) {
	free(bucket->data);
	free(bucket);
}