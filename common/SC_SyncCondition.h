/*
	SuperCollider real time audio synthesis system
    Copyright (c) 2002 James McCartney. All rights reserved.
	http://www.audiosynth.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#pragma once

#ifdef BELA
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
extern int gXenomaiInited;
#include <xenomai/init.h>
#include <sched.h>
#else /* BELA */
#include "SC_Lock.h"
#endif /* BELA */

class SC_SyncCondition
{
public:
	SC_SyncCondition()
		: read(0), write(0)
	{
#ifdef BELA
		if(!gXenomaiInited)
		{
			int argc = 0;
			char *const *argv;
			xenomai_init(&argc, &argv);
			gXenomaiInited = 1;
		}
		if(int ret = __wrap_pthread_cond_init(&available, NULL))
		{
			fprintf(stderr, "Unable to initialize condition variable: %d %s\n", ret, strerror(ret));
		}
		if(int ret = __wrap_pthread_mutex_init(&mutex, NULL))
		{
			fprintf(stderr, "Unable to initialize mutex %d %s\n", ret, strerror(ret));
		}
		int ret = __wrap_pthread_mutex_lock(&mutex);
		if(ret)
			printf("ctor mutex_lock ret: %d %s\n", ret, strerror(ret));
		ret = __wrap_pthread_mutex_unlock(&mutex);
		if(ret)
			printf("ctor mutex_unlock ret: %d %s\n", ret, strerror(ret));
#endif /* BELA */
	}

	~SC_SyncCondition()
	{}

	void WaitEach()
	{
#ifdef BELA
#else /* BELA */
		// waits if it has caught up.
		// not very friendly, may be trying in vain to keep up.
		unique_lock<SC_Lock> lock(mutex);
		while (read == write)
			available.wait(lock);
		++read;
#endif /* BELA */
	}

	void WaitOnce()
	{
		// waits if not signaled since last time.
		// if only a little late then can still go.

#ifdef BELA
#else /* BELA */
		unique_lock<SC_Lock> lock(mutex);
		int writeSnapshot  = write;
		while (read == writeSnapshot)
			available.wait(lock);
		read = writeSnapshot;
#endif /* BELA */
	}

	void WaitNext()
	{
		// will wait for the next signal after the read = write statement
		// this is the friendliest to other tasks, because if it is
		// late upon entry, then it has to lose a turn.
#ifdef BELA
		//nothing
#else /* BELA */
		unique_lock<SC_Lock> lock(mutex);
#endif /* BELA */
		read = write;
		while (read == write)
		{
#ifdef BELA
			int ret = __wrap_pthread_mutex_lock(&mutex);
			if(ret)
			{
				printf("mutex_lock ret: %d %s\n", ret, strerror(ret));
				struct sched_param_ex param;
				param.sched_priority = 1;

				ret = sched_setscheduler_ex(0, SCHED_FIFO, &param);
				printf("setscheduler: %d\n", ret);
				return;
			}

			ret = __wrap_pthread_cond_wait(&available, &mutex);
			if(ret)
			{
				printf("pthread_cond_wait ret: %d %s\n", ret, strerror(ret));
				usleep(100000);
				break;
			}
			ret = __wrap_pthread_mutex_unlock(&mutex);
			if(ret)
				printf("mutex_unlock ret: %d %s\n", ret, strerror(ret));
#else /* BELA */
			available.wait(lock);
#endif /* BELA */
		}
	}

	void Signal()
	{
		++write;
#ifdef BELA
	int ret;
	if(ret = __wrap_pthread_mutex_trylock(&mutex))
	{
	// If we cannot get the lock, then the other thread is probably still running.
	} else {
		ret = __wrap_pthread_cond_signal(&available);
		if(ret)
			printf("signal: cond_signal ret: %d %s\n", ret, strerror(ret));
		ret = __wrap_pthread_mutex_unlock(&mutex);
		if(ret)
			printf("signal: mutex_unlock (rt) ret: %d %s\n", ret, strerror(ret));
	}
#else /* BELA */
		available.notify_one();
#endif /* BELA */
	}

private:
	// the mutex is only for pthread_cond_wait, which requires it.
	// since there is only supposed to be one signaller and one waiter
	// there is nothing to mutually exclude.
#ifdef BELA
	pthread_cond_t available;
	pthread_mutex_t mutex;
#else /* BELA */
	condition_variable_any available;
	SC_Lock mutex;
#endif /* BELA */
	int read, write;
};
