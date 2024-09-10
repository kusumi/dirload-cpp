#include "./thread.h"

pthread_mutex_t __mutex;

int init_lock(void) {
	return pthread_mutex_init(&__mutex, nullptr);
}

int cleanup_lock(void) {
	return pthread_mutex_destroy(&__mutex);
}
