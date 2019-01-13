#include "XenomaiLock.h"

#include <pthread.h>
#include <error.h>
#include <string.h>
#include <cobalt/sys/cobalt.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>

//#define PRINT_XENO_LOCK
#if 0
#include <mutex>
using std::mutex;
using std::condition_variable_any
typedef XenomaiMutex mutex;
typedef XenomaiConditionVariable condition_variable_any;
#else
static int count = 0;

#define XENOMAI_LOCK_INLINE 
// All __wrap calls below are Xenomai libcobalt calls

extern int gXenomaiInited;
extern "C" {
void xenomai_init(int *argcp, char *const **argvp);
}
static inline pid_t get_tid()
{
	pid_t tid = syscall(SYS_gettid);
	return tid;
}

inline int turn_into_cobalt_thread()
{
	if(!gXenomaiInited)
	{
		int argc = 0;
		char *const *argv;
		xenomai_init(&argc, &argv);
		gXenomaiInited = 1;
	}
	++count;
	if(count >= 100)
	{
		return 0;
	}
	int current_mode = cobalt_thread_mode();
	struct sched_param param;
	memset(&param, 0, sizeof(param));
	int policy;
	int ret = pthread_getschedparam(pthread_self(), &policy, &param);
	if(param.sched_priority == 95) // audio thread is already cobalt
		return 0;
	//printf("Returned: %d, policy: %d, param: %d\n", ret, policy, param.sched_priority);
	//param.sched_priority = 0;
	pid_t tid = get_tid();

	if(int ret = __wrap_sched_setscheduler(tid, policy, &param))
	{
		fprintf(stderr, "Error: unable to turn current thread into a Xenomai thread : (%d) %s\n", -ret, strerror(-ret));
		return -1;
	}
#ifdef PRINT_XENO_LOCK
	printf("Turned thread %d into a Cobalt thread\n", tid);
#endif
	return 0;
}
XENOMAI_LOCK_INLINE XenomaiMutex::XenomaiMutex()
{
#ifdef PRINT_XENO_LOCK
	printf("Construct mutex\n");
#endif
	if(int ret = turn_into_cobalt_thread())
		return;
	if(int ret = __wrap_pthread_mutex_init(&mutex, NULL))
	{
		fprintf(stderr, "Error: unable to initialize mutex : (%d) %s\n", ret, strerror(-ret));
		return;
	}
	enabled = true;

}

XENOMAI_LOCK_INLINE XenomaiMutex::~XenomaiMutex()
{
#ifdef PRINT_XENO_LOCK
	printf("Destroy mutex %p\n", &mutex);
#endif
	if(enabled)
		__wrap_pthread_mutex_destroy(&mutex);
}

XENOMAI_LOCK_INLINE bool XenomaiMutex::try_lock()
{
	if(turn_into_cobalt_thread())
		return false;
#ifdef PRINT_XENO_LOCK
	pid_t tid = get_tid();
	printf("tid: %d ", tid);
#endif
	if(enabled)
	{
#ifdef PRINT_XENO_LOCK
		printf("Mutex %p lock()\n", &mutex);
#endif
		return (0 == __wrap_pthread_mutex_trylock(&mutex));
	} else {
		fprintf(stderr, "mutex disabled  on lock %p\n", &mutex);
		return false;
	}
	//TODO: An implementation that can detect the invalid usage is encouraged to throw a std::system_error with error condition resource_deadlock_would_occur instead of deadlocking.
}

XENOMAI_LOCK_INLINE void XenomaiMutex::lock()
{
	if(turn_into_cobalt_thread())
		return;
#ifdef PRINT_XENO_LOCK
	pid_t tid = get_tid();
	printf("tid: %d ", tid);
#endif
	if(enabled)
	{
#ifdef PRINT_XENO_LOCK
		printf("Mutex %p lock()\n", &mutex);
#endif
		__wrap_pthread_mutex_lock(&mutex);
	} else {
		fprintf(stderr, "mutex disabled  on lock %p\n", &mutex);
	}
	//TODO: An implementation that can detect the invalid usage is encouraged to throw a std::system_error with error condition resource_deadlock_would_occur instead of deadlocking.
}

XENOMAI_LOCK_INLINE void XenomaiMutex::unlock()
{
	if(turn_into_cobalt_thread())
		return;
#ifdef PRINT_XENO_LOCK
	pid_t tid = get_tid();
	printf("tid: %d ", tid);
#endif
	if(enabled)
	{
#ifdef PRINT_XENO_LOCK
		printf("Mutex %p unlock() \n", &mutex);
#endif
		__wrap_pthread_mutex_unlock(&mutex);
	} else {
		printf("mutex disabled  on unlock %p\n", &mutex);
	}
}

XENOMAI_LOCK_INLINE XenomaiConditionVariable::XenomaiConditionVariable()
{
	if(int ret = turn_into_cobalt_thread())
		return;
#ifdef PRINT_XENO_LOCK
	pid_t tid = get_tid();
	printf("tid: %d ", tid);
#endif
	if(int ret = __wrap_pthread_cond_init(&cond, NULL))
	{
		fprintf(stderr, "Error: unable to create condition variable : (%d) %s\n", ret, strerror(ret));
		return;
	}
	enabled = true;
}

XENOMAI_LOCK_INLINE XenomaiConditionVariable::~XenomaiConditionVariable()
{
	if(enabled)
	{
		notify_all();
		__wrap_pthread_cond_destroy(&cond);
	}
}

XENOMAI_LOCK_INLINE void XenomaiConditionVariable::wait(std::unique_lock<XenomaiMutex>& lck)
{
// If any parameter has a value that is not valid for this function (such as if lck's mutex object is not locked by the calling thread), it causes undefined behavior.

// Otherwise, if an exception is thrown, both the condition_variable_any object and the arguments are in a valid state (basic guarantee). Additionally, on exception, the state of lck is attempted to be restored before exiting the function scope (by calling lck.lock()).

// It may throw system_error in case of failure (transmitting any error condition from the respective call to lock or unlock).
// The predicate version (2) may also throw exceptions thrown by pred.

	//usleep(10000);
	if(turn_into_cobalt_thread())
		return;
#ifdef PRINT_XENO_LOCK
	pid_t tid = get_tid();
	printf("tid: %d ", tid);
#endif
	if(enabled)
	{
#ifdef PRINT_XENO_LOCK
		printf("waiting on cond %p with mutex %p\n", &cond, &lck.mutex()->mutex);
#endif
		__wrap_pthread_cond_wait(&cond, &lck.mutex()->mutex);
#ifdef PRINT_XENO_LOCK
		printf("released cond\n");
#endif
	} else {
		fprintf(stderr, "cond  disabled  on wait %p\n", &cond);
	}
}

XENOMAI_LOCK_INLINE void XenomaiConditionVariable::notify_one() noexcept
{
	if(turn_into_cobalt_thread())
		return;
#ifdef PRINT_XENO_LOCK
	pid_t tid = get_tid();
	printf("tid: %d ", tid);
#endif
	if(enabled)
	{
#ifdef PRINT_XENO_LOCK
		printf("notifying cond %p\n", &cond);
#endif		
		if(int ret = __wrap_pthread_cond_signal(&cond))
		{
			fprintf(stderr, "cond_signal returned: %d %s\n", ret, strerror(ret));
		}
	} else {
		fprintf(stderr, "cond  disabled on notify_one %p\n", &cond);
	}
}

XENOMAI_LOCK_INLINE void XenomaiConditionVariable::notify_all() noexcept
{
	if(turn_into_cobalt_thread())
		return;
#ifdef PRINT_XENO_LOCK
	pid_t tid = get_tid();
	printf("tid: %d ", tid);
#endif
	if(enabled)
	{
#ifdef PRINT_XENO_LOCK
		printf("notifying all cond %p\n", &cond);
#endif
		if(int ret = __wrap_pthread_cond_broadcast(&cond))
		{
			fprintf(stderr, "cond_broadcast returned: %d %s\n", ret, strerror(ret));
		}

	} else {
		fprintf(stderr, "cond  disabled  on notify_all %p\n", &cond);
	}
}

#endif // 1 or 0
