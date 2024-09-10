#ifndef SRC_THREAD_H_
#define SRC_THREAD_H_

#ifdef CONFIG_STDTHREAD
#include "./thread_std.h"
#else
#include "./thread_posix.h"
#endif
#endif // SRC_THREAD_H_
