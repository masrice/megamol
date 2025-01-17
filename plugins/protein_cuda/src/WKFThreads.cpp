/***************************************************************************
 * RCS INFORMATION:
 *
 *      $RCSfile: WKFThreads.C,v $
 *      $Author: johns $        $Locker:  $             $State: Exp $
 *      $Revision: 1.11 $       $Date: 2011/02/07 19:57:50 $
 *
 ***************************************************************************
 * WKFThreads.C - code for spawning threads on various platforms.
 *                Code donated by John Stone, john.stone@gmail.com
 *                This code was originally written for the
 *                Tachyon Parallel/Multiprocessor Ray Tracer.
 *                Improvements have been donated by Mr. Stone on an
 *                ongoing basis.  This code is provided under the
 *                three clause BSD Open Source License.
 *
 * NOTE: The latest version of this source file can be re-generated by
 *       running the sequence of 'sed' commands shown at the top of the
 *       "threads.c" file within the Tachyon source distribution.
 *
 ***************************************************************************/
/* Tachyon copyright reproduced below */
/*
 * Copyright (c) 1994-2011 John E. Stone
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * If compiling on Linux, enable the GNU CPU affinity functions in both
 * libc and the libpthreads
 */
#if defined(__linux)
#define _GNU_SOURCE 1
#include <sched.h>
#endif

#include "WKFThreads.h"

#ifdef _MSC_VER
#if 0
#define WKFUSENEWWIN32APIS 1
#define _WIN32_WINNT 0x0400 /**< needed for TryEnterCriticalSection(), etc */
#define WINVER 0x0400       /**< needed for TryEnterCriticalSection(), etc */
#endif
#include <winbase.h> /**< system services headers */
#include <windows.h> /**< main Win32 APIs and types */
#endif

#if defined(_AIX) || defined(_CRAY) || defined(__irix) || defined(__linux) || defined(__osf__) || defined(__sun)
#include <unistd.h> /**< sysconf() headers, used by most systems */
#endif

#if defined(__APPLE__) && defined(WKFTHREADS)
#include <Carbon/Carbon.h> /**< Carbon APIs for Multiprocessing */
#endif

#if defined(__hpux)
#include <sys/mpctl.h> /**< HP-UX Multiprocessing headers */
#endif


#ifdef __cplusplus
extern "C" {
#endif

int wkf_thread_numphysprocessors(void) {
    int a = 1;

#ifdef WKFTHREADS
#if defined(__APPLE__)
    a = MPProcessorsScheduled(); /**< Number of active/running CPUs */
#endif

#ifdef _MSC_VER
    struct _SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    a = sysinfo.dwNumberOfProcessors; /**< total number of CPUs */
#endif                                /* _MSC_VER */

#if defined(_CRAY)
    a = sysconf(_SC_CRAY_NCPU); /**< total number of CPUs */
#endif

#if defined(__sun) || defined(__linux) || defined(__osf__) || defined(_AIX)
    a = sysconf(_SC_NPROCESSORS_ONLN); /**< Number of active/running CPUs */
#endif                                 /* SunOS */

#if defined(__irix)
    a = sysconf(_SC_NPROC_ONLN); /**< Number of active/running CPUs */
#endif                           /* IRIX */

#if defined(__hpux)
    a = mpctl(MPC_GETNUMSPUS, 0, 0); /**< total number of CPUs */
#endif                               /* HPUX */
#endif                               /* WKFTHREADS */

    return a;
}


int wkf_thread_numprocessors(void) {
    int a = 1;

#ifdef WKFTHREADS
    /* Allow the user to override the number of CPUs for use */
    /* in scalability testing, debugging, etc.               */
    char* forcecount = getenv("WKFFORCECPUCOUNT");
    if (forcecount != NULL) {
        if (sscanf(forcecount, "%d", &a) == 1) {
            return a; /* if we got a valid count, return it */
        } else {
            a = 1; /* otherwise use the real available hardware CPU count */
        }
    }

    /* otherwise return the number of physical processors currently available */
    a = wkf_thread_numphysprocessors();

    /* XXX we should add checking for the current CPU affinity masks here, */
    /* and return the min of the physical processor count and CPU affinity */
    /* mask enabled CPU count.                                             */
#endif /* WKFTHREADS */

    return a;
}


int* wkf_cpu_affinitylist(int* cpuaffinitycount) {
    int* affinitylist = NULL;
    *cpuaffinitycount = -1; /* return count -1 if unimplemented or err occurs */

/* Win32 process affinity mask query */
#if 0 && defined(_MSC_VER)
  /* XXX untested, but based on the linux code, may work with a few tweaks */
  HANDLE myproc = GetCurrentProcess(); /* returns a psuedo-handle */
  DWORD affinitymask, sysaffinitymask;

  if (!GetProcessAffinityMask(myproc, &affinitymask, &sysaffinitymask)) {
    /* count length of affinity list */
    int affinitycount=0;
    int i;
    for (i=0; i<31; i++) {
      affinitycount += (affinitymask >> i) & 0x1;
    }

    /* build affinity list */
    if (affinitycount > 0) {
      affinitylist = (int *) malloc(affinitycount * sizeof(int));
      if (affinitylist == NULL)
        return NULL;

      int curcount = 0;
      for (i=0; i<CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &affinitymask)) {
          affinitylist[curcount] = i;
          curcount++;
        }
      }
    }

    *cpuaffinitycount = affinitycount; /* return final affinity list */
  }
#endif

/* Linux process affinity mask query */
#if defined(__linux)

/* protect ourselves from some older Linux distros */
#if defined(CPU_SETSIZE)
    int i;
    cpu_set_t affinitymask;
    int affinitycount = 0;

    /* PID 0 refers to the current process */
    if (sched_getaffinity(0, sizeof(affinitymask), &affinitymask) < 0) {
        perror("wkf_cpu_affinitylist: sched_getaffinity");
        return NULL;
    }

    /* count length of affinity list */
    for (i = 0; i < CPU_SETSIZE; i++) {
        affinitycount += CPU_ISSET(i, &affinitymask);
    }

    /* build affinity list */
    if (affinitycount > 0) {
        affinitylist = (int*)malloc(affinitycount * sizeof(int));
        if (affinitylist == NULL)
            return NULL;

        int curcount = 0;
        for (i = 0; i < CPU_SETSIZE; i++) {
            if (CPU_ISSET(i, &affinitymask)) {
                affinitylist[curcount] = i;
                curcount++;
            }
        }
    }

    *cpuaffinitycount = affinitycount; /* return final affinity list */
#endif
#endif

    /* MacOS X 10.5.x has a CPU affinity query/set capability finally      */
    /* http://developer.apple.com/releasenotes/Performance/RN-AffinityAPI/ */

    /* Solaris and HP-UX use pset_bind() and related functions, and they   */
    /* don't use the single-level mask-based scheduling mechanism that     */
    /* the others, use.  Instead, they use a hierarchical tree of          */
    /* processor sets and processes float within those, or are tied to one */
    /* processor that's a member of a particular set.                      */

    return affinitylist;
}


int wkf_thread_set_self_cpuaffinity(int cpu) {
    int status = -1; /* unsupported by default */

#ifdef WKFTHREADS

#if defined(__linux) && defined(CPU_ZERO) && defined(CPU_SET)
#if 0
  /* XXX this code is too new even for RHEL4, though it runs on Fedora 7 */
  /* and other newer revs.                                               */
  /* NPTL systems can assign per-thread affinities this way              */
  cpu_set_t affinitymask;
  CPU_ZERO(&affinitymask);
  CPU_SET(cpu, &affinitymask);
  status = pthread_setaffinity_np(pthread_self(), sizeof(affinitymask), &affinitymask);
#else
    /* non-NPTL systems based on the clone() API must use this method      */
    cpu_set_t affinitymask;
    CPU_ZERO(&affinitymask);
    CPU_SET(cpu, &affinitymask);

    /* PID 0 refers to the current process */
    if ((status = sched_setaffinity(0, sizeof(affinitymask), &affinitymask)) < 0) {
        perror("wkf_thread_set_self_cpuaffinitylist: sched_setaffinity");
        return status;
    }
#endif

    /* call sched_yield() so new affinity mask takes effect immediately */
    sched_yield();
#endif /* linux */

    /* MacOS X 10.5.x has a CPU affinity query/set capability finally      */
    /* http://developer.apple.com/releasenotes/Performance/RN-AffinityAPI/ */

    /* Solaris and HP-UX use pset_bind() and related functions, and they   */
    /* don't use the single-level mask-based scheduling mechanism that     */
    /* the others, use.  Instead, they use a hierarchical tree of          */
    /* processor sets and processes float within those, or are tied to one */
    /* processor that's a member of a particular set.                      */
#endif

    return status;
}


int wkf_thread_setconcurrency(int nthr) {
    int status = 0;

#ifdef WKFTHREADS
#if defined(__sun)
#ifdef USEPOSIXTHREADS
    status = pthread_setconcurrency(nthr);
#else
    status = thr_setconcurrency(nthr);
#endif
#endif /* SunOS */

#if defined(__irix) || defined(_AIX)
    status = pthread_setconcurrency(nthr);
#endif
#endif /* WKFTHREADS */

    return status;
}


/*
 * Thread creation/management
 */
/** Typedef to eliminate compiler warning caused by C/C++ linkage conflict. */
typedef void* (*WKFTHREAD_START_ROUTINE)(void*);

int wkf_thread_create(wkf_thread_t* thr, void* fctn(void*), void* arg) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
    DWORD tid; /* thread id, msvc only */
    *thr = CreateThread(NULL, 8192, (LPTHREAD_START_ROUTINE)fctn, arg, 0, &tid);
    if (*thr == NULL) {
        status = -1;
    }
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
#if defined(_AIX)
    /* AIX schedule threads in system scope by default, have to ask explicitly */
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        status = pthread_create(thr, &attr, (WKFTHREAD_START_ROUTINE)fctn, arg);
        pthread_attr_destroy(&attr);
    }
#else
    status = pthread_create(thr, NULL, (WKFTHREAD_START_ROUTINE)fctn, arg);
#endif
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


int wkf_thread_join(wkf_thread_t thr, void** stat) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
    DWORD wstatus = 0;

    wstatus = WAIT_TIMEOUT;

    while (wstatus != WAIT_OBJECT_0) {
        wstatus = WaitForSingleObject(thr, INFINITE);
    }
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_join(thr, stat);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


/*
 * Mutexes
 */
int wkf_mutex_init(wkf_mutex_t* mp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
    InitializeCriticalSection(mp);
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_mutex_init(mp, 0);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


int wkf_mutex_lock(wkf_mutex_t* mp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
    EnterCriticalSection(mp);
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_mutex_lock(mp);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


int wkf_mutex_trylock(wkf_mutex_t* mp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
#if defined(WKFUSENEWWIN32APIS)
    /* TryEnterCriticalSection() is only available on newer */
    /* versions of Win32: _WIN32_WINNT/WINVER >= 0x0400     */
    status = (!(TryEnterCriticalSection(mp)));
#endif
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = (pthread_mutex_lock(mp) != 0);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


int wkf_mutex_spin_lock(wkf_mutex_t* mp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
#if defined(WKFUSENEWWIN32APIS)
    /* TryEnterCriticalSection() is only available on newer */
    /* versions of Win32: _WIN32_WINNT/WINVER >= 0x0400     */
    while (!TryEnterCriticalSection(mp))
        ;
#else
    EnterCriticalSection(mp);
#endif
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    while ((status = pthread_mutex_trylock(mp)) != 0)
        ;
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


int wkf_mutex_unlock(wkf_mutex_t* mp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
    LeaveCriticalSection(mp);
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_mutex_unlock(mp);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


int wkf_mutex_destroy(wkf_mutex_t* mp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
    DeleteCriticalSection(mp);
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_mutex_destroy(mp);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


/*
 * Condition variables
 */
int wkf_cond_init(wkf_cond_t* cvp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
#if defined(WKFUSEWIN2008CONDVARS)
    InitializeConditionVariable(cvp);
#else
    /* XXX not implemented */
    cvp->waiters = 0;

    /* Create an auto-reset event. */
    cvp->events[WKF_COND_SIGNAL] = CreateEvent(NULL, /* no security */
        FALSE,                                       /* auto-reset event */
        FALSE,                                       /* non-signaled initially */
        NULL);                                       /* unnamed */

    /* Create a manual-reset event. */
    cvp->events[WKF_COND_BROADCAST] = CreateEvent(NULL, /* no security */
        TRUE,                                           /* manual-reset */
        FALSE,                                          /* non-signaled initially */
        NULL);                                          /* unnamed */
#endif
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_cond_init(cvp, NULL);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}

int wkf_cond_destroy(wkf_cond_t* cvp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
#if defined(WKFUSEWIN2008CONDVARS)
    /* XXX not implemented */
#else
    CloseHandle(cvp->events[WKF_COND_SIGNAL]);
    CloseHandle(cvp->events[WKF_COND_BROADCAST]);
#endif
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_cond_destroy(cvp);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}

int wkf_cond_wait(wkf_cond_t* cvp, wkf_mutex_t* mp) {
    int status = 0;
#if defined(WKFTHREADS) && defined(_MSC_VER)
    int result = 0;
    LONG last_waiter;
    LONG my_waiter;
#endif

#ifdef WKFTHREADS
#ifdef _MSC_VER
#if defined(WKFUSEWIN2008CONDVARS)
    SleepConditionVariableCS(cvp, mp, INFINITE)
#else
#if !defined(WKFUSEINTERLOCKEDATOMICOPS)
    EnterCriticalSection(&cvp->waiters_lock);
    cvp->waiters++;
    LeaveCriticalSection(&cvp->waiters_lock);
#else
    InterlockedIncrement(&cvp->waiters);
#endif

    LeaveCriticalSection(mp); /* SetEvent() keeps state, avoids lost wakeup */

    /* Wait either a single or broadcast even to become signalled */
    result = WaitForMultipleObjects(2, cvp->events, FALSE, INFINITE);

#if !defined(WKFUSEINTERLOCKEDATOMICOPS)
    EnterCriticalSection(&cvp->waiters_lock);
    cvp->waiters--;
    last_waiter = ((result == (WAIT_OBJECT_0 + WKF_COND_BROADCAST)) && cvp->waiters == 0);
    LeaveCriticalSection(&cvp->waiters_lock);
#else
    my_waiter = InterlockedDecrement(&cvp->waiters);
    last_waiter = ((result == (WAIT_OBJECT_0 + WKF_COND_BROADCAST)) && my_waiter == 0);
#endif

    /* Some thread called cond_broadcast() */
    if (last_waiter)
        /* We're the last waiter to be notified or to stop waiting, so */
        /* reset the manual event.                                     */
        ResetEvent(cvp->events[WKF_COND_BROADCAST]);

    EnterCriticalSection(mp);
#endif
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
        status = pthread_cond_wait(cvp, mp);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}

int wkf_cond_signal(wkf_cond_t* cvp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
#if defined(WKFUSEWIN2008CONDVARS)
    WakeConditionVariable(cvp);
#else
#if !defined(WKFUSEINTERLOCKEDATOMICOPS)
    EnterCriticalSection(&cvp->waiters_lock);
    int have_waiters = (cvp->waiters > 0);
    LeaveCriticalSection(&cvp->waiters_lock);
    if (have_waiters)
        SetEvent(cvp->events[WKF_COND_SIGNAL]);
#else
    if (InterlockedExchangeAdd(&cvp->waiters, 0) > 0)
        SetEvent(cvp->events[WKF_COND_SIGNAL]);
#endif
#endif
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_cond_signal(cvp);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}

int wkf_cond_broadcast(wkf_cond_t* cvp) {
    int status = 0;

#ifdef WKFTHREADS
#ifdef _MSC_VER
#if defined(WKFUSEWIN2008CONDVARS)
    WakeAllConditionVariable(cvp);
#else
#if !defined(WKFUSEINTERLOCKEDATOMICOPS)
    EnterCriticalSection(&cvp->waiters_lock);
    int have_waiters = (cvp->waiters > 0);
    LeaveCriticalSection(&cvp->waiters_lock);
    if (have_waiters)
        SetEvent(cvp->events[WKF_COND_BROADCAST]);
#else
    if (InterlockedExchangeAdd(&cvp->waiters, 0) > 0)
        SetEvent(cvp->events[WKF_COND_BROADCAST]);
#endif

#endif
#endif /* _MSC_VER */

#ifdef USEPOSIXTHREADS
    status = pthread_cond_broadcast(cvp);
#endif /* USEPOSIXTHREADS */
#endif /* WKFTHREADS */

    return status;
}


/*
 * Barriers used for sleepable thread pools
 */
/* symmetric run barrier for use within a single process */
int wkf_thread_run_barrier_init(wkf_run_barrier_t* barrier, int n_clients) {
#ifdef WKFTHREADS
    if (barrier != NULL) {
        barrier->n_clients = n_clients;
        barrier->n_waiting = 0;
        barrier->phase = 0;
        barrier->fctn = NULL;

        wkf_mutex_init(&barrier->lock);
        wkf_cond_init(&barrier->wait_cv);
    }
#endif

    return 0;
}

void wkf_thread_run_barrier_destroy(wkf_run_barrier_t* barrier) {
#ifdef WKFTHREADS
    wkf_mutex_destroy(&barrier->lock);
    wkf_cond_destroy(&barrier->wait_cv);
#endif
}


/**
 * Wait until all threads reach barrier, and return the function
 * pointer passed in by the master thread.
 */
void* (*wkf_thread_run_barrier(wkf_run_barrier_t* barrier, void* fctn(void*), void* parms, void** rsltparms))(void*) {
#if defined(WKFTHREADS)
    int my_phase;
    void* (*my_result)(void*);

    wkf_mutex_lock(&barrier->lock);
    my_phase = barrier->phase;
    if (fctn != NULL)
        barrier->fctn = fctn;
    if (parms != NULL)
        barrier->parms = parms;
    barrier->n_waiting++;

    if (barrier->n_waiting == barrier->n_clients) {
        barrier->rslt = barrier->fctn;
        barrier->rsltparms = barrier->parms;
        barrier->fctn = NULL;
        barrier->parms = NULL;
        barrier->n_waiting = 0;
        barrier->phase = 1 - my_phase;
        wkf_cond_broadcast(&barrier->wait_cv);
    }

    while (barrier->phase == my_phase) {
        wkf_cond_wait(&barrier->wait_cv, &barrier->lock);
    }

    my_result = barrier->rslt;
    if (rsltparms != NULL)
        *rsltparms = barrier->rsltparms;

    wkf_mutex_unlock(&barrier->lock);
#else
    void* (*my_result)(void*) = fctn;
    if (rsltparms != NULL)
        *rsltparms = parms;
#endif

    return my_result;
}


/** non-blocking poll to see if peers are already at the barrier */
int wkf_thread_run_barrier_poll(wkf_run_barrier_t* barrier) {
    int rc = 0;
#if defined(WKFTHREADS)
    wkf_mutex_lock(&barrier->lock);
    if (barrier->n_waiting == (barrier->n_clients - 1)) {
        rc = 1;
    }
    wkf_mutex_unlock(&barrier->lock);
#endif
    return rc;
}


/*
 * task tile stack
 */
int wkf_tilestack_init(wkf_tilestack_t* s, int size) {
    if (s == NULL)
        return -1;

#if defined(WKFTHREADS)
    wkf_mutex_init(&s->mtx);
#endif

    s->growthrate = 512;
    s->top = -1;

    if (size > 0) {
        s->size = size;
        s->s = (wkf_tasktile_t*)malloc(s->size * sizeof(wkf_tasktile_t));
    } else {
        s->size = 0;
        s->s = NULL;
    }

    return 0;
}


void wkf_tilestack_destroy(wkf_tilestack_t* s) {
#if defined(WKFTHREADS)
    wkf_mutex_destroy(&s->mtx);
#endif
    free(s->s);
    s->s = NULL; /* prevent access after free */
}


int wkf_tilestack_compact(wkf_tilestack_t* s) {
#if defined(WKFTHREADS)
    wkf_mutex_lock(&s->mtx);
#endif
    if (s->size > (s->top + 1)) {
        int newsize = s->top + 1;
        wkf_tasktile_t* tmp = (wkf_tasktile_t*)realloc(s->s, newsize * sizeof(wkf_tasktile_t));
        if (tmp == NULL) {
#if defined(WKFTHREADS)
            wkf_mutex_unlock(&s->mtx);
#endif
            return -1; /* out of space! */
        }
        s->s = tmp;
        s->size = newsize;
    }
#if defined(WKFTHREADS)
    wkf_mutex_unlock(&s->mtx);
#endif

    return 0;
}


int wkf_tilestack_push(wkf_tilestack_t* s, const wkf_tasktile_t* t) {
#if defined(WKFTHREADS)
    wkf_mutex_lock(&s->mtx);
#endif
    s->top++;
    if (s->top >= s->size) {
        int newsize = s->size + s->growthrate;
        wkf_tasktile_t* tmp = (wkf_tasktile_t*)realloc(s->s, newsize * sizeof(wkf_tasktile_t));
        if (tmp == NULL) {
            s->top--;
#if defined(WKFTHREADS)
            wkf_mutex_unlock(&s->mtx);
#endif
            return -1; /* out of space! */
        }
        s->s = tmp;
        s->size = newsize;
    }

    s->s[s->top] = *t; /* push onto the stack */

#if defined(WKFTHREADS)
    wkf_mutex_unlock(&s->mtx);
#endif

    return 0;
}


int wkf_tilestack_pop(wkf_tilestack_t* s, wkf_tasktile_t* t) {
#if defined(WKFTHREADS)
    wkf_mutex_lock(&s->mtx);
#endif

    if (s->top < 0) {
#if defined(WKFTHREADS)
        wkf_mutex_unlock(&s->mtx);
#endif
        return WKF_TILESTACK_EMPTY; /* empty stack */
    }

    *t = s->s[s->top];
    s->top--;

#if defined(WKFTHREADS)
    wkf_mutex_unlock(&s->mtx);
#endif

    return 0;
}


int wkf_tilestack_popall(wkf_tilestack_t* s) {
#if defined(WKFTHREADS)
    wkf_mutex_lock(&s->mtx);
#endif

    s->top = -1;

#if defined(WKFTHREADS)
    wkf_mutex_unlock(&s->mtx);
#endif

    return 0;
}


int wkf_tilestack_empty(wkf_tilestack_t* s) {
#if defined(WKFTHREADS)
    wkf_mutex_lock(&s->mtx);
#endif

    if (s->top < 0) {
#if defined(WKFTHREADS)
        wkf_mutex_unlock(&s->mtx);
#endif
        return 1;
    }

#if defined(WKFTHREADS)
    wkf_mutex_unlock(&s->mtx);
#endif

    return 0;
}


/*
 * shared iterators
 */

/** initialize a shared iterator */
int wkf_shared_iterator_init(wkf_shared_iterator_t* it) {
    memset(it, 0, sizeof(wkf_shared_iterator_t));
#if defined(WKFTHREADS)
    wkf_mutex_init(&it->mtx);
#endif
    return 0;
}


/** destroy a shared iterator */
int wkf_shared_iterator_destroy(wkf_shared_iterator_t* it) {
#if defined(WKFTHREADS)
    wkf_mutex_destroy(&it->mtx);
#endif
    return 0;
}


/** set shared iterator parameters */
int wkf_shared_iterator_set(wkf_shared_iterator_t* it, wkf_tasktile_t* tile) {
#if defined(WKFTHREADS)
    wkf_mutex_lock(&it->mtx);
#endif
    it->start = tile->start;
    it->current = tile->start;
    it->end = tile->end;
    it->fatalerror = 0;
#if defined(WKFTHREADS)
    wkf_mutex_unlock(&it->mtx);
#endif
    return 0;
}


/** iterate the shared iterator, over a requested half-open interval */
int wkf_shared_iterator_next_tile(wkf_shared_iterator_t* it, int reqsize, wkf_tasktile_t* tile) {
    int rc = WKF_SCHED_CONTINUE;

#if defined(WKFTHREADS)
    wkf_mutex_spin_lock(&it->mtx);
#endif
    if (!it->fatalerror) {
        tile->start = it->current; /* set start to the current work unit    */
        it->current += reqsize;    /* increment by the requested tile size  */
        tile->end = it->current;   /* set the (exclusive) endpoint          */

        /* if start is beyond the last work unit, we're done */
        if (tile->start >= it->end) {
            tile->start = 0;
            tile->end = 0;
            rc = WKF_SCHED_DONE;
        }

        /* if the endpoint (exclusive) for the requested tile size */
        /* is beyond the last work unit, roll it back as needed     */
        if (tile->end > it->end) {
            tile->end = it->end;
        }
    } else {
        rc = WKF_SCHED_DONE;
    }
#if defined(WKFTHREADS)
    wkf_mutex_unlock(&it->mtx);
#endif

    return rc;
}


/** worker thread calls this to indicate a fatal error */
int wkf_shared_iterator_setfatalerror(wkf_shared_iterator_t* it) {
#if defined(WKFTHREADS)
    wkf_mutex_spin_lock(&it->mtx);
#endif
    it->fatalerror = 1;
#if defined(WKFTHREADS)
    wkf_mutex_unlock(&it->mtx);
#endif
    return 0;
}


/** master thread calls this to query for fatal errors */
int wkf_shared_iterator_getfatalerror(wkf_shared_iterator_t* it) {
    int rc = 0;
#if defined(WKFTHREADS)
    wkf_mutex_lock(&it->mtx);
#endif
    if (it->fatalerror)
        rc = -1;
#if defined(WKFTHREADS)
    wkf_mutex_unlock(&it->mtx);
#endif
    return rc;
}


#if defined(WKFTHREADS)
/*
 * Thread pool.
 */
static void* wkf_threadpool_workerproc(void* voidparms) {
    void* (*fctn)(void*);
    wkf_threadpool_workerdata_t* workerdata = (wkf_threadpool_workerdata_t*)voidparms;
    wkf_threadpool_t* thrpool = (wkf_threadpool_t*)workerdata->thrpool;

    while ((fctn = wkf_thread_run_barrier(&thrpool->runbar, NULL, NULL, &workerdata->parms)) != NULL) {
        (*fctn)(workerdata);
    }

    return NULL;
}


static void* wkf_threadpool_workersync(void* voidparms) {
    return NULL;
}
#endif


wkf_threadpool_t* wkf_threadpool_create(int workercount, int* devlist) {
    int i;
    wkf_threadpool_t* thrpool = NULL;
    thrpool = (wkf_threadpool_t*)malloc(sizeof(wkf_threadpool_t));
    if (thrpool == NULL)
        return NULL;

    memset(thrpool, 0, sizeof(wkf_threadpool_t));

#if !defined(WKFTHREADS)
    workercount = 1;
#endif

    /* if caller provides a device list, use it, otherwise we assume */
    /* all workers are CPU cores */
    thrpool->devlist = (int*)malloc(sizeof(int) * workercount);
    if (devlist == NULL) {
        for (i = 0; i < workercount; i++)
            thrpool->devlist[i] = -1; /* mark as a CPU core */
    } else {
        memcpy(thrpool->devlist, devlist, sizeof(int) * workercount);
    }

    /* initialize shared iterator */
    wkf_shared_iterator_init(&thrpool->iter);

    /* initialize tile stack for error handling */
    wkf_tilestack_init(&thrpool->errorstack, 64);

    /* create a run barrier with N+1 threads: N workers, 1 master */
    thrpool->workercount = workercount;
    wkf_thread_run_barrier_init(&thrpool->runbar, workercount + 1);

    /* allocate and initialize thread pool */
    thrpool->threads = (wkf_thread_t*)malloc(sizeof(wkf_thread_t) * workercount);
    thrpool->workerdata = (wkf_threadpool_workerdata_t*)malloc(sizeof(wkf_threadpool_workerdata_t) * workercount);
    memset(thrpool->workerdata, 0, sizeof(wkf_threadpool_workerdata_t) * workercount);

    /* setup per-worker data */
    for (i = 0; i < workercount; i++) {
        thrpool->workerdata[i].iter = &thrpool->iter;
        thrpool->workerdata[i].errorstack = &thrpool->errorstack;
        thrpool->workerdata[i].threadid = i;
        thrpool->workerdata[i].threadcount = workercount;
        thrpool->workerdata[i].devid = thrpool->devlist[i];
        thrpool->workerdata[i].devspeed = 1.0f; /* must be reset by dev setup code */
        thrpool->workerdata[i].thrpool = thrpool;
    }

#if defined(WKFTHREADS)
    /* launch thread pool */
    for (i = 0; i < workercount; i++) {
        wkf_thread_create(&thrpool->threads[i], wkf_threadpool_workerproc, &thrpool->workerdata[i]);
    }
#endif

    return thrpool;
}


int wkf_threadpool_launch(wkf_threadpool_t* thrpool, void* fctn(void*), void* parms, int blocking) {
    if (thrpool == NULL)
        return -1;

#if defined(WKFTHREADS)
    /* wake sleeping threads to run fctn(parms) */
    wkf_thread_run_barrier(&thrpool->runbar, fctn, parms, NULL);
    if (blocking)
        wkf_thread_run_barrier(&thrpool->runbar, wkf_threadpool_workersync, NULL, NULL);
#else
    thrpool->workerdata[0].parms = parms;
    (*fctn)(&thrpool->workerdata[0]);
#endif
    return 0;
}


int wkf_threadpool_wait(wkf_threadpool_t* thrpool) {
#if defined(WKFTHREADS)
    wkf_thread_run_barrier(&thrpool->runbar, wkf_threadpool_workersync, NULL, NULL);
#endif
    return 0;
}


int wkf_threadpool_poll(wkf_threadpool_t* thrpool) {
#if defined(WKFTHREADS)
    return wkf_thread_run_barrier_poll(&thrpool->runbar);
#else
    return 1;
#endif
}


int wkf_threadpool_destroy(wkf_threadpool_t* thrpool) {
#if defined(WKFTHREADS)
    int i;
#endif

    /* wake threads and tell them to shutdown */
    wkf_thread_run_barrier(&thrpool->runbar, NULL, NULL, NULL);

#if defined(WKFTHREADS)
    /* join the pool of worker threads */
    for (i = 0; i < thrpool->workercount; i++) {
        wkf_thread_join(thrpool->threads[i], NULL);
    }
#endif

    /* destroy the thread barrier */
    wkf_thread_run_barrier_destroy(&thrpool->runbar);

    /* destroy the shared iterator */
    wkf_shared_iterator_destroy(&thrpool->iter);

    /* destroy tile stack for error handling */
    wkf_tilestack_destroy(&thrpool->errorstack);

    free(thrpool->devlist);
    free(thrpool->threads);
    free(thrpool->workerdata);
    free(thrpool);

    return 0;
}


/** return the number of worker threads currently in the pool */
int wkf_threadpool_get_workercount(wkf_threadpool_t* thrpool) {
    return thrpool->workercount;
}


/** worker thread can call this to get its ID and number of peers */
int wkf_threadpool_worker_getid(void* voiddata, int* threadid, int* threadcount) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voiddata;
    if (threadid != NULL)
        *threadid = worker->threadid;

    if (threadcount != NULL)
        *threadcount = worker->threadcount;

    return 0;
}


/** worker thread can call this to get its CPU/GPU device ID */
int wkf_threadpool_worker_getdevid(void* voiddata, int* devid) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voiddata;
    if (devid != NULL)
        *devid = worker->devid;

    return 0;
}


/**
 * worker thread calls this to set relative speed of this device
 * as determined by the SM/core count and clock rate
 * Note: this should only be called once, during the worker's
 * device initialization process
 */
int wkf_threadpool_worker_setdevspeed(void* voiddata, float speed) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voiddata;
    worker->devspeed = speed;
    return 0;
}


/**
 * worker thread calls this to get relative speed of this device
 * as determined by the SM/core count and clock rate
 */
int wkf_threadpool_worker_getdevspeed(void* voiddata, float* speed) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voiddata;
    if (speed != NULL)
        *speed = worker->devspeed;
    return 0;
}


/**
 * worker thread calls this to scale max tile size by worker speed
 * as determined by the SM/core count and clock rate
 */
int wkf_threadpool_worker_devscaletile(void* voiddata, int* tilesize) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voiddata;
    if (tilesize != NULL) {
        int scaledtilesize;
        scaledtilesize = (int)(worker->devspeed * ((float)(*tilesize)));
        if (scaledtilesize < 1)
            scaledtilesize = 1;

        *tilesize = scaledtilesize;
    }

    return 0;
}


/** worker thread can call this to get its client data pointer */
int wkf_threadpool_worker_getdata(void* voiddata, void** clientdata) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voiddata;
    if (clientdata != NULL)
        *clientdata = worker->parms;

    return 0;
}


/** Set shared iterator state to half-open interval defined by tile */
int wkf_threadpool_sched_dynamic(wkf_threadpool_t* thrpool, wkf_tasktile_t* tile) {
    if (thrpool == NULL)
        return -1;
    return wkf_shared_iterator_set(&thrpool->iter, tile);
}


/** iterate the shared iterator over the requested half-open interval */
int wkf_threadpool_next_tile(void* voidparms, int reqsize, wkf_tasktile_t* tile) {
    int rc;
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voidparms;
    rc = wkf_shared_iterator_next_tile(worker->iter, reqsize, tile);
    if (rc == WKF_SCHED_DONE) {
        /* if the error stack is empty, then we're done, otherwise pop */
        /* a tile off of the error stack and retry it                  */
        if (wkf_tilestack_pop(worker->errorstack, tile) != WKF_TILESTACK_EMPTY)
            return WKF_SCHED_CONTINUE;
    }

    return rc;
}


/**
 * worker thread calls this when a failure occurs on a tile it has
 * already taken from the scheduler
 */
int wkf_threadpool_tile_failed(void* voidparms, wkf_tasktile_t* tile) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voidparms;
    return wkf_tilestack_push(worker->errorstack, tile);
}


/* worker thread calls this to indicate that an unrecoverable error occured */
int wkf_threadpool_setfatalerror(void* voidparms) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voidparms;
    wkf_shared_iterator_setfatalerror(worker->iter);
    return 0;
}


/* worker thread calls this to indicate that an unrecoverable error occured */
int wkf_threadpool_getfatalerror(void* voidparms) {
    wkf_threadpool_workerdata_t* worker = (wkf_threadpool_workerdata_t*)voidparms;
    /* query error status for return to caller */
    return wkf_shared_iterator_getfatalerror(worker->iter);
}


/* launch up to numprocs threads using shared iterator as a load balancer */
int wkf_threadlaunch(int numprocs, void* clientdata, void* fctn(void*), wkf_tasktile_t* tile) {
    wkf_shared_iterator_t iter;
    wkf_threadlaunch_t* parms = NULL;
    wkf_thread_t* threads = NULL;
    int i, rc;

    /* XXX have to ponder what the right thing to do is here */
#if !defined(WKFTHREADS)
    numprocs = 1;
#endif

    /* initialize shared iterator and set the iteration and range */
    wkf_shared_iterator_init(&iter);
    if (wkf_shared_iterator_set(&iter, tile))
        return -1;

    /* allocate array of threads */
    threads = (wkf_thread_t*)calloc(numprocs * sizeof(wkf_thread_t), 1);
    if (threads == NULL)
        return -1;

    /* allocate and initialize array of thread parameters */
    parms = (wkf_threadlaunch_t*)malloc(numprocs * sizeof(wkf_threadlaunch_t));
    if (parms == NULL) {
        free(threads);
        return -1;
    }
    for (i = 0; i < numprocs; i++) {
        parms[i].iter = &iter;
        parms[i].threadid = i;
        parms[i].threadcount = numprocs;
        parms[i].clientdata = clientdata;
    }

#if defined(WKFTHREADS)
    if (numprocs == 1) {
        /* XXX we special-case the single worker thread  */
        /*     scenario because this greatly reduces the */
        /*     GPU kernel launch overhead since a new    */
        /*     contexts doesn't have to be created, and  */
        /*     in the simplest case with a single-GPU we */
        /*     will just be using the same device anyway */
        /*     Ideally we shouldn't need to do this....  */
        /* single thread does all of the work */
        fctn((void*)&parms[0]);
    } else {
        /* spawn child threads to do the work */
        for (i = 0; i < numprocs; i++) {
            wkf_thread_create(&threads[i], fctn, &parms[i]);
        }

        /* join the threads after work is done */
        for (i = 0; i < numprocs; i++) {
            wkf_thread_join(threads[i], NULL);
        }
    }
#else
    /* single thread does all of the work */
    fctn((void*)&parms[0]);
#endif

    /* free threads/parms */
    free(parms);
    free(threads);

    /* query error status for return to caller */
    rc = wkf_shared_iterator_getfatalerror(&iter);

    /* destroy the shared iterator */
    wkf_shared_iterator_destroy(&iter);

    return rc;
}


/** worker thread can call this to get its ID and number of peers */
int wkf_threadlaunch_getid(void* voidparms, int* threadid, int* threadcount) {
    wkf_threadlaunch_t* worker = (wkf_threadlaunch_t*)voidparms;
    if (threadid != NULL)
        *threadid = worker->threadid;

    if (threadcount != NULL)
        *threadcount = worker->threadcount;

    return 0;
}


/** worker thread can call this to get its client data pointer */
int wkf_threadlaunch_getdata(void* voidparms, void** clientdata) {
    wkf_threadlaunch_t* worker = (wkf_threadlaunch_t*)voidparms;
    if (clientdata != NULL)
        *clientdata = worker->clientdata;

    return 0;
}


/** iterate the shared iterator over the requested half-open interval */
int wkf_threadlaunch_next_tile(void* voidparms, int reqsize, wkf_tasktile_t* tile) {
    wkf_threadlaunch_t* worker = (wkf_threadlaunch_t*)voidparms;
    return wkf_shared_iterator_next_tile(worker->iter, reqsize, tile);
}


/** worker thread calls this to indicate that an unrecoverable error occured */
int wkf_threadlaunch_setfatalerror(void* voidparms) {
    wkf_threadlaunch_t* worker = (wkf_threadlaunch_t*)voidparms;
    return wkf_shared_iterator_setfatalerror(worker->iter);
}


#ifdef __cplusplus
}
#endif
