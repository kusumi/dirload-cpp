#ifndef SRC_THREAD_POSIX_H_
#define SRC_THREAD_POSIX_H_

#include <pthread.h>

#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }

#define get_thread_id() static_cast<unsigned long>(pthread_self())

typedef void* (*pfn)(void*);
extern pthread_mutex_t __mutex;

int init_lock(void);
int cleanup_lock(void);

static inline void global_lock(void) {
	pthread_mutex_lock(&__mutex);
}

static inline void global_unlock(void) {
	pthread_mutex_unlock(&__mutex);
}

class Thread {
	public:
	Thread(void):
		_t{} {
	}
	int create(pfn fn, void* arg) {
		return pthread_create(&_t, nullptr, fn, arg);
	}
	int join(void) {
		void* res;
		return pthread_join(_t, &res);
	}

	private:
	pthread_t _t;
};
#endif // SRC_THREAD_POSIX_H_
