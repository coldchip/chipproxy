#include <stdlib.h>
#include <string.h>
#include "bucket.h"

ProxyBucket *chipproxy_bucket_create(int max) {
	ProxyBucket *bucket = malloc(sizeof(ProxyBucket));
	bucket->data = malloc(sizeof(char) * 1);
	bucket->size = 0;
	bucket->max = max;
	return bucket;
}

char *chipproxy_bucket_get_buffer(ProxyBucket *bucket) {
	return bucket->data;
}

int chipproxy_bucket_read_available(ProxyBucket *bucket) {
	return bucket->size;
}

int chipproxy_bucket_write_available(ProxyBucket *bucket) {
	return bucket->max - bucket->size;
}

int chipproxy_bucket_write(ProxyBucket *bucket, void *buf, int size) {
	bucket->data = realloc(bucket->data, bucket->size + size);
	memcpy(bucket->data + bucket->size, buf, size);
	bucket->size += size;

	return size;
}

int chipproxy_bucket_read(ProxyBucket *bucket, void *buf, int size) {
	size = MIN(bucket->size, size);

	if(buf) {
		memcpy(buf, bucket->data, size);
	}
	memmove(bucket->data, bucket->data + size, bucket->size - size);
	bucket->size -= size;
	bucket->data = realloc(bucket->data, bucket->size);

	return size;
}

void chipproxy_bucket_free(ProxyBucket *bucket) {
	free(bucket->data);
	free(bucket);
}