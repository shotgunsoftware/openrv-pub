/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2008 by Hewlett-Packard Development Company.
 * All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include "private/gc_priv.h"

#if defined(GC_WIN32_THREADS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#define NOSERVICE
#include <windows.h>

#ifdef THREAD_LOCAL_ALLOC
#include "private/thread_local_alloc.h"
#endif /* THREAD_LOCAL_ALLOC */

/* Allocation lock declarations.        */
#if !defined(USE_PTHREAD_LOCKS)
GC_INNER CRITICAL_SECTION GC_allocate_ml;
GC_INNER DWORD GC_lock_holder = NO_THREAD;
/* Thread id for current holder of allocation lock */
#else
GC_INNER pthread_mutex_t GC_allocate_ml = PTHREAD_MUTEX_INITIALIZER;
GC_INNER unsigned long GC_lock_holder = NO_THREAD;
#endif

#ifdef GC_PTHREADS
#include <errno.h> /* for EAGAIN */

/* Cygwin-specific forward decls */
#undef pthread_create
#undef pthread_join
#undef pthread_detach

#ifndef GC_NO_PTHREAD_SIGMASK
#undef pthread_sigmask
#endif

STATIC void* GC_pthread_start(void* arg);
STATIC void GC_thread_exit_proc(void* arg);

#include <pthread.h>
#ifdef CAN_HANDLE_FORK
#include <unistd.h>
#endif

#else

#undef CreateThread
#undef ExitThread
#undef _beginthreadex
#undef _endthreadex

#ifdef MSWINCE
/* Force DONT_USE_SIGNALANDWAIT implementation of PARALLEL_MARK     */
/* for WinCE (since Win32 SignalObjectAndWait() is missing).        */
#ifndef DONT_USE_SIGNALANDWAIT
#define DONT_USE_SIGNALANDWAIT
#endif
#else
#include <process.h> /* For _beginthreadex, _endthreadex */
#include <errno.h>   /* for errno, EAGAIN */
#endif

#endif

/* DllMain-based thread registration is currently incompatible  */
/* with thread-local allocation, pthreads and WinCE.            */
#if defined(GC_DLL) && !defined(GC_NO_THREADS_DISCOVERY) && !defined(MSWINCE) \
    && !defined(THREAD_LOCAL_ALLOC) && !defined(GC_PTHREADS)
#include "atomic_ops.h"

/* This code operates in two distinct modes, depending on     */
/* the setting of GC_win32_dll_threads.                       */
/* If GC_win32_dll_threads is set, all threads in the process */
/* are implicitly registered with the GC by DllMain.          */
/* No explicit registration is required, and attempts at      */
/* explicit registration are ignored.  This mode is           */
/* very different from the Posix operation of the collector.  */
/* In this mode access to the thread table is lock-free.      */
/* Hence there is a static limit on the number of threads.    */

#ifdef GC_DISCOVER_TASK_THREADS
/* GC_DISCOVER_TASK_THREADS should be used if DllMain-based */
/* thread registration is required but it is impossible to  */
/* call GC_use_threads_discovery before other GC routines.  */
#define GC_win32_dll_threads TRUE
#else
STATIC GC_bool GC_win32_dll_threads = FALSE;
/* GC_win32_dll_threads must be set (if needed) at the      */
/* application initialization time, i.e. before any         */
/* collector or thread calls.  We make it a "dynamic"       */
/* option only to avoid multiple library versions.          */
#endif

#else
/* If GC_win32_dll_threads is FALSE (or the collector is      */
/* built without GC_DLL defined), things operate in a way     */
/* that is very similar to Posix platforms, and new threads   */
/* must be registered with the collector, e.g. by using       */
/* preprocessor-based interception of the thread primitives.  */
/* In this case, we use a real data structure for the thread  */
/* table.  Note that there is no equivalent of linker-based   */
/* call interception, since we don't have ELF-like            */
/* facilities.  The Windows analog appears to be "API         */
/* hooking", which really seems to be a standard way to       */
/* do minor binary rewriting (?).  I'd prefer not to have     */
/* the basic collector rely on such facilities, but an        */
/* optional package that intercepts thread calls this way     */
/* would probably be nice.                                    */
#ifndef GC_NO_THREADS_DISCOVERY
#define GC_NO_THREADS_DISCOVERY
#endif
#define GC_win32_dll_threads FALSE
#undef MAX_THREADS
#define MAX_THREADS 1 /* dll_thread_table[] is always empty.   */
#endif                /* GC_NO_THREADS_DISCOVERY */

/* We have two versions of the thread table.  Which one */
/* we us depends on whether or not GC_win32_dll_threads */
/* is set.  Note that before initialization, we don't   */
/* add any entries to either table, even if DllMain is  */
/* called.  The main thread will be added on            */
/* initialization.                                      */

/* The type of the first argument to InterlockedExchange.       */
/* Documented to be LONG volatile *, but at least gcc likes     */
/* this better.                                                 */
typedef LONG* IE_t;

STATIC GC_bool GC_thr_initialized = FALSE;

GC_INNER GC_bool GC_need_to_lock = FALSE;

static GC_bool parallel_initialized = FALSE;

/* GC_use_threads_discovery() is currently incompatible with pthreads   */
/* and WinCE.  It might be possible to get DllMain-based thread         */
/* registration to work with Cygwin, but if you try it then you are on  */
/* your own.                                                            */
GC_API void GC_CALL GC_use_threads_discovery(void)
{
#ifdef GC_NO_THREADS_DISCOVERY
    ABORT("GC DllMain-based thread registration unsupported");
#else
    /* Turn on GC_win32_dll_threads. */
    GC_ASSERT(!parallel_initialized);
#ifndef GC_DISCOVER_TASK_THREADS
    GC_win32_dll_threads = TRUE;
#endif
    GC_init_parallel();
#endif
}

STATIC DWORD GC_main_thread = 0;

#define ADDR_LIMIT ((ptr_t)(word) - 1)

struct GC_Thread_Rep
{
    union
    {
#ifndef GC_NO_THREADS_DISCOVERY
        AO_t in_use; /* Updated without lock.                */
                     /* We assert that unused                */
                     /* entries have invalid ids of          */
                     /* zero and zero stack fields.          */
                     /* Used only with GC_win32_dll_threads. */
#endif
        struct GC_Thread_Rep* next;
        /* Hash table link without              */
        /* GC_win32_dll_threads.                */
        /* More recently allocated threads      */
        /* with a given pthread id come         */
        /* first.  (All but the first are       */
        /* guaranteed to be dead, but we may    */
        /* not yet have registered the join.)   */
    } tm; /* table_management */

    DWORD id;

#ifdef MSWINCE
    /* According to MSDN specs for WinCE targets:                       */
    /* - DuplicateHandle() is not applicable to thread handles; and     */
    /* - the value returned by GetCurrentThreadId() could be used as    */
    /* a "real" thread handle (for SuspendThread(), ResumeThread() and  */
    /* GetThreadContext()).                                             */
#define THREAD_HANDLE(t) (HANDLE)(word)(t)->id
#else
    HANDLE handle;
#define THREAD_HANDLE(t) (t)->handle
#endif

    ptr_t stack_base;     /* The cold end of the stack.   */
                          /* 0 ==> entry not valid.       */
                          /* !in_use ==> stack_base == 0  */
    ptr_t last_stack_min; /* Last known minimum (hottest) address */
                          /* in stack or ADDR_LIMIT if unset      */
#ifdef IA64
    ptr_t backing_store_end;
    ptr_t backing_store_ptr;
#endif

    ptr_t thread_blocked_sp; /* Protected by GC lock.                */
                             /* NULL value means thread unblocked.   */
                             /* If set to non-NULL, thread will      */
                             /* acquire GC lock before doing any     */
                             /* pointer manipulations.  Thus it does */
                             /* not need to stop this thread.        */

    struct GC_traced_stack_sect_s* traced_stack_sect;
    /* Points to the "stack section" data   */
    /* held in stack by the innermost       */
    /* GC_call_with_gc_active() of this     */
    /* thread.  May be NULL.                */

    unsigned short finalizer_skipped;
    unsigned char finalizer_nested;
    /* Used by GC_check_finalizer_nested()  */
    /* to minimize the level of recursion   */
    /* when a client finalizer allocates    */
    /* memory (initially both are 0).       */

    unsigned char suspended; /* really of GC_bool type */

#ifdef GC_PTHREADS
    unsigned char flags; /* Protected by GC lock.                */
#define FINISHED 1       /* Thread has exited.                   */
#define DETACHED 2       /* Thread is intended to be detached.   */
#define KNOWN_FINISHED(t) (((t)->flags) & FINISHED)
    pthread_t pthread_id;
    void* status; /* hold exit value until join in case it's a pointer */
#else
#define KNOWN_FINISHED(t) 0
#endif

#ifdef THREAD_LOCAL_ALLOC
    struct thread_local_freelists tlfs;
#endif
};

typedef struct GC_Thread_Rep* GC_thread;
typedef volatile struct GC_Thread_Rep* GC_vthread;

#ifndef GC_NO_THREADS_DISCOVERY
/* We assumed that volatile ==> memory ordering, at least among       */
/* volatiles.  This code should consistently use atomic_ops.          */
STATIC volatile GC_bool GC_please_stop = FALSE;
#elif defined(GC_ASSERTIONS)
STATIC GC_bool GC_please_stop = FALSE;
#endif

/*
 * We track thread attachments while the world is supposed to be stopped.
 * Unfortunately, we can't stop them from starting, since blocking in
 * DllMain seems to cause the world to deadlock.  Thus we have to recover
 * If we notice this in the middle of marking.
 */

#ifndef GC_NO_THREADS_DISCOVERY
STATIC AO_t GC_attached_thread = FALSE;
#endif

#if !defined(__GNUC__)
/* Return TRUE if an thread was attached since we last asked or */
/* since GC_attached_thread was explicitly reset.               */
GC_bool GC_started_thread_while_stopped(void)
{
#ifndef GC_NO_THREADS_DISCOVERY
    AO_t result;

    if (GC_win32_dll_threads)
    {
        AO_nop_full(); /* Prior heap reads need to complete earlier. */
        result = AO_load(&GC_attached_thread);
        if (result)
        {
            AO_store(&GC_attached_thread, FALSE);
            return TRUE;
        }
    }
#endif
    return FALSE;
}
#endif /* !__GNUC__ */

/* Thread table used if GC_win32_dll_threads is set.    */
/* This is a fixed size array.                          */
/* Since we use runtime conditionals, both versions     */
/* are always defined.                                  */
#ifndef MAX_THREADS
#define MAX_THREADS 512
#endif

/* Things may get quite slow for large numbers of threads,      */
/* since we look them up with sequential search.                */
volatile struct GC_Thread_Rep dll_thread_table[MAX_THREADS];

STATIC volatile LONG GC_max_thread_index = 0;
/* Largest index in dll_thread_table    */
/* that was ever used.                  */

/* And now the version used if GC_win32_dll_threads is not set. */
/* This is a chained hash table, with much of the code borrowed */
/* From the Posix implementation.                               */
#ifndef THREAD_TABLE_SZ
#define THREAD_TABLE_SZ 256 /* Power of 2 (for speed). */
#endif
#define THREAD_TABLE_INDEX(id) (((word)(id) >> 2) % THREAD_TABLE_SZ)
STATIC GC_thread GC_threads[THREAD_TABLE_SZ];

/* It may not be safe to allocate when we register the first thread.    */
/* Thus we allocated one statically.                                    */
static struct GC_Thread_Rep first_thread;
static GC_bool first_thread_used = FALSE;

/* Add a thread to GC_threads.  We assume it wasn't already there.      */
/* Caller holds allocation lock.                                        */
/* Unlike the pthreads version, the id field is set by the caller.      */
STATIC GC_thread GC_new_thread(DWORD id)
{
    word hv = THREAD_TABLE_INDEX(id);
    GC_thread result;

    GC_ASSERT(I_HOLD_LOCK());
    if (!first_thread_used)
    {
        result = &first_thread;
        first_thread_used = TRUE;
    }
    else
    {
        GC_ASSERT(!GC_win32_dll_threads);
        result = (struct GC_Thread_Rep*)GC_INTERNAL_MALLOC(
            sizeof(struct GC_Thread_Rep), NORMAL);
        /* result can be NULL */
        if (result == 0)
            return (0);
    }
    /* result -> id = id; Done by caller.       */
    result->tm.next = GC_threads[hv];
    GC_threads[hv] = result;
#ifdef GC_PTHREADS
    GC_ASSERT(result->flags == 0);
#endif
    GC_ASSERT(result->thread_blocked_sp == NULL);
    return (result);
}

STATIC GC_bool GC_in_thread_creation = FALSE;

/* Protected by allocation lock. */

GC_INLINE void GC_record_stack_base(GC_vthread me,
                                    const struct GC_stack_base* sb)
{
    me->stack_base = sb->mem_base;
#ifdef IA64
    me->backing_store_end = sb->reg_base;
#endif
    if (me->stack_base == NULL)
        ABORT("Bad stack base in GC_register_my_thread");
}

/* This may be called from DllMain, and hence operates under unusual    */
/* constraints.  In particular, it must be lock-free if                 */
/* GC_win32_dll_threads is set.  Always called from the thread being    */
/* added.  If GC_win32_dll_threads is not set, we already hold the      */
/* allocation lock except possibly during single-threaded startup code. */
STATIC GC_thread GC_register_my_thread_inner(const struct GC_stack_base* sb,
                                             DWORD thread_id)
{
    GC_vthread me;

    /* The following should be a no-op according to the win32     */
    /* documentation.  There is empirical evidence that it        */
    /* isn't.             - HB                                    */
#if defined(MPROTECT_VDB)
    if (GC_incremental
#ifdef GWW_VDB
        && !GC_gww_dirty_init()
#endif
    )
        GC_set_write_fault_handler();
#endif

#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        int i;
        /* It appears to be unsafe to acquire a lock here, since this     */
        /* code is apparently not preemptible on some systems.            */
        /* (This is based on complaints, not on Microsoft's official      */
        /* documentation, which says this should perform "only simple     */
        /* initialization tasks".)                                        */
        /* Hence we make do with nonblocking synchronization.             */
        /* It has been claimed that DllMain is really only executed with  */
        /* a particular system lock held, and thus careful use of locking */
        /* around code that doesn't call back into the system libraries   */
        /* might be OK.  But this hasn't been tested across all win32     */
        /* variants.                                                      */
        /* cast away volatile qualifier */
        for (i = 0;
             InterlockedExchange((void*)&dll_thread_table[i].tm.in_use, 1) != 0;
             i++)
        {
            /* Compare-and-swap would make this cleaner, but that's not     */
            /* supported before Windows 98 and NT 4.0.  In Windows 2000,    */
            /* InterlockedExchange is supposed to be replaced by            */
            /* InterlockedExchangePointer, but that's not really what I     */
            /* want here.                                                   */
            /* FIXME: We should eventually declare Win95 dead and use AO_   */
            /* primitives here.                                             */
            if (i == MAX_THREADS - 1)
                ABORT("Too many threads");
        }
        /* Update GC_max_thread_index if necessary.  The following is     */
        /* safe, and unlike CompareExchange-based solutions seems to work */
        /* on all Windows95 and later platforms.                          */
        /* Unfortunately, GC_max_thread_index may be temporarily out of   */
        /* bounds, so readers have to compensate.                         */
        while (i > GC_max_thread_index)
        {
            InterlockedIncrement((IE_t)&GC_max_thread_index);
        }
        if (GC_max_thread_index >= MAX_THREADS)
        {
            /* We overshot due to simultaneous increments.  */
            /* Setting it to MAX_THREADS-1 is always safe.  */
            GC_max_thread_index = MAX_THREADS - 1;
        }
        me = dll_thread_table + i;
    }
    else
#endif
    /* else */ /* Not using DllMain */ {
        GC_ASSERT(I_HOLD_LOCK());
        GC_in_thread_creation = TRUE; /* OK to collect from unknown thread. */
        me = GC_new_thread(thread_id);
        GC_in_thread_creation = FALSE;
        if (me == 0)
            ABORT("Failed to allocate memory for thread registering");
    }
#ifdef GC_PTHREADS
    /* me can be NULL -> segfault */
    me->pthread_id = pthread_self();
#endif
#ifndef MSWINCE
    /* GetCurrentThread() returns a pseudohandle (a const value).       */
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), (HANDLE*)&(me->handle),
                         0 /* dwDesiredAccess */, FALSE /* bInheritHandle */,
                         DUPLICATE_SAME_ACCESS))
    {
        if (GC_print_stats)
            GC_log_printf("DuplicateHandle failed with error code: %d\n",
                          (int)GetLastError());
        ABORT("DuplicateHandle failed");
    }
#endif
    me->last_stack_min = ADDR_LIMIT;
    GC_record_stack_base(me, sb);
    /* Up until this point, GC_push_all_stacks considers this thread      */
    /* invalid.                                                           */
    /* Up until this point, this entry is viewed as reserved but invalid  */
    /* by GC_delete_thread.                                               */
    me->id = thread_id;
#if defined(THREAD_LOCAL_ALLOC)
    GC_init_thread_local((GC_tlfs)(&(me->tlfs)));
#endif
#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        if (GC_please_stop)
        {
            AO_store(&GC_attached_thread, TRUE);
            AO_nop_full(); /* Later updates must become visible after this. */
        }
        /* We'd like to wait here, but can't, since waiting in DllMain    */
        /* provokes deadlocks.                                            */
        /* Thus we force marking to be restarted instead.                 */
    }
    else
#endif
    /* else */ {
        GC_ASSERT(!GC_please_stop);
        /* Otherwise both we and the thread stopping code would be      */
        /* holding the allocation lock.                                 */
    }
    return (GC_thread)(me);
}

/*
 * GC_max_thread_index may temporarily be larger than MAX_THREADS.
 * To avoid subscript errors, we check on access.
 */
GC_INLINE LONG GC_get_max_thread_index(void)
{
    LONG my_max = GC_max_thread_index;
    if (my_max >= MAX_THREADS)
        return MAX_THREADS - 1;
    return my_max;
}

/* Return the GC_thread corresponding to a thread id.  May be called    */
/* without a lock, but should be called in contexts in which the        */
/* requested thread cannot be asynchronously deleted, e.g. from the     */
/* thread itself.                                                       */
/* This version assumes that either GC_win32_dll_threads is set, or     */
/* we hold the allocator lock.                                          */
/* Also used (for assertion checking only) from thread_local_alloc.c.   */
STATIC GC_thread GC_lookup_thread_inner(DWORD thread_id)
{
#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        int i;
        LONG my_max = GC_get_max_thread_index();
        for (i = 0; i <= my_max
                    && (!AO_load_acquire(&dll_thread_table[i].tm.in_use)
                        || dll_thread_table[i].id != thread_id);
             /* Must still be in_use, since nobody else can store our     */
             /* thread_id.                                                */
             i++)
        {
            /* empty */
        }
        return i <= my_max ? (GC_thread)(dll_thread_table + i) : NULL;
    }
    else
#endif
    /* else */ {
        word hv = THREAD_TABLE_INDEX(thread_id);
        register GC_thread p = GC_threads[hv];

        GC_ASSERT(I_HOLD_LOCK());
        while (p != 0 && p->id != thread_id)
            p = p->tm.next;
        return (p);
    }
}

#ifdef LINT2
#define CHECK_LOOKUP_MY_THREAD(me) \
    if (!(me))                     \
    ABORT("GC_lookup_thread_inner(GetCurrentThreadId) failed")
#else
#define CHECK_LOOKUP_MY_THREAD(me) /* empty */
#endif

/* Called by GC_finalize() (in case of an allocation failure observed). */
/* GC_reset_finalizer_nested() is the same as in pthread_support.c.     */
GC_INNER void GC_reset_finalizer_nested(void)
{
    GC_thread me = GC_lookup_thread_inner(GetCurrentThreadId());
    CHECK_LOOKUP_MY_THREAD(me);
    me->finalizer_nested = 0;
}

/* Checks and updates the thread-local level of finalizers recursion.   */
/* Returns NULL if GC_invoke_finalizers() should not be called by the   */
/* collector (to minimize the risk of a deep finalizers recursion),     */
/* otherwise returns a pointer to the thread-local finalizer_nested.    */
/* Called by GC_notify_or_invoke_finalizers() only (the lock is held).  */
/* GC_check_finalizer_nested() is the same as in pthread_support.c.     */
GC_INNER unsigned char* GC_check_finalizer_nested(void)
{
    GC_thread me = GC_lookup_thread_inner(GetCurrentThreadId());
    unsigned nesting_level;
    CHECK_LOOKUP_MY_THREAD(me);
    nesting_level = me->finalizer_nested;
    if (nesting_level)
    {
        /* We are inside another GC_invoke_finalizers().            */
        /* Skip some implicitly-called GC_invoke_finalizers()       */
        /* depending on the nesting (recursion) level.              */
        if (++me->finalizer_skipped < (1U << nesting_level))
            return NULL;
        me->finalizer_skipped = 0;
    }
    me->finalizer_nested = (unsigned char)(nesting_level + 1);
    return &me->finalizer_nested;
}

#if defined(GC_ASSERTIONS) && defined(THREAD_LOCAL_ALLOC)
/* This is called from thread-local GC_malloc(). */
GC_bool GC_is_thread_tsd_valid(void* tsd)
{
    GC_thread me;
    DCL_LOCK_STATE;

    LOCK();
    me = GC_lookup_thread_inner(GetCurrentThreadId());
    UNLOCK();
    return (char*)tsd >= (char*)&me->tlfs
           && (char*)tsd < (char*)&me->tlfs + sizeof(me->tlfs);
}
#endif /* GC_ASSERTIONS && THREAD_LOCAL_ALLOC */

/* Make sure thread descriptor t is not protected by the VDB            */
/* implementation.                                                      */
/* Used to prevent write faults when the world is (partially) stopped,  */
/* since it may have been stopped with a system lock held, and that     */
/* lock may be required for fault handling.                             */
#if defined(MPROTECT_VDB)
#define UNPROTECT_THREAD(t)                                                 \
    if (!GC_win32_dll_threads && GC_dirty_maintained && t != &first_thread) \
    {                                                                       \
        GC_ASSERT(SMALL_OBJ(GC_size(t)));                                   \
        GC_remove_protection(HBLKPTR(t), 1, FALSE);                         \
    }
#else
#define UNPROTECT_THREAD(t)
#endif

#ifdef CYGWIN32
#define GC_PTHREAD_PTRVAL(pthread_id) pthread_id
#elif defined(GC_WIN32_PTHREADS)
#define GC_PTHREAD_PTRVAL(pthread_id) pthread_id.p
#endif

/* If a thread has been joined, but we have not yet             */
/* been notified, then there may be more than one thread        */
/* in the table with the same win32 id.                         */
/* This is OK, but we need a way to delete a specific one.      */
/* Assumes we hold the allocation lock unless                   */
/* GC_win32_dll_threads is set.                                 */
/* If GC_win32_dll_threads is set it should be called from the  */
/* thread being deleted.                                        */
STATIC void GC_delete_gc_thread(GC_vthread t)
{
#ifndef MSWINCE
    CloseHandle(t->handle);
#endif
#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        /* This is intended to be lock-free.                              */
        /* It is either called synchronously from the thread being        */
        /* deleted, or by the joining thread.                             */
        /* In this branch asynchronous changes to (*t) are possible.      */
        /* It's not allowed to call GC_printf (and the friends) here,     */
        /* see GC_stop_world() for the information.                       */
        t->stack_base = 0;
        t->id = 0;
#ifdef GC_PTHREADS
        GC_PTHREAD_PTRVAL(t->pthread_id) = 0;
#endif
        AO_store_release(&t->tm.in_use, FALSE);
    }
    else
#endif
    /* else */ {
        DWORD id = ((GC_thread)t)->id;
        /* Cast away volatile qualifier, since we have lock.    */
        word hv = THREAD_TABLE_INDEX(id);
        register GC_thread p = GC_threads[hv];
        register GC_thread prev = 0;

        GC_ASSERT(I_HOLD_LOCK());
        while (p != (GC_thread)t)
        {
            prev = p;
            p = p->tm.next;
        }
        if (prev == 0)
        {
            GC_threads[hv] = p->tm.next;
        }
        else
        {
            prev->tm.next = p->tm.next;
        }
        GC_INTERNAL_FREE(p);
    }
}

/* Delete a thread from GC_threads.  We assume it is there.     */
/* (The code intentionally traps if it wasn't.)  Assumes we     */
/* hold the allocation lock unless GC_win32_dll_threads is set. */
/* If GC_win32_dll_threads is set then it should be called from */
/* the thread being deleted.  It is also safe to delete the     */
/* main thread (unless GC_win32_dll_threads).                   */
STATIC void GC_delete_thread(DWORD id)
{
    if (GC_win32_dll_threads)
    {
        GC_thread t = GC_lookup_thread_inner(id);

        if (0 == t)
        {
            WARN("Removing nonexistent thread, id = %" GC_PRIdPTR "\n", id);
        }
        else
        {
            GC_delete_gc_thread(t);
        }
    }
    else
    {
        word hv = THREAD_TABLE_INDEX(id);
        register GC_thread p = GC_threads[hv];
        register GC_thread prev = 0;

        GC_ASSERT(I_HOLD_LOCK());
        while (p->id != id)
        {
            prev = p;
            p = p->tm.next;
        }
#ifndef MSWINCE
        CloseHandle(p->handle);
#endif
        if (prev == 0)
        {
            GC_threads[hv] = p->tm.next;
        }
        else
        {
            prev->tm.next = p->tm.next;
        }
        if (p != &first_thread)
        {
            GC_INTERNAL_FREE(p);
        }
    }
}

GC_API void GC_CALL GC_allow_register_threads(void)
{
    /* Check GC is initialized and the current thread is registered. */
    GC_ASSERT(GC_lookup_thread_inner(GetCurrentThreadId()) != 0);

#if !defined(GC_NO_THREADS_DISCOVERY) && !defined(PARALLEL_MARK)
    /* GC_init() doesn't call GC_init_parallel() in this case.  */
    parallel_initialized = TRUE;
#endif
    GC_need_to_lock = TRUE; /* We are multi-threaded now. */
}

GC_API int GC_CALL GC_register_my_thread(const struct GC_stack_base* sb)
{
    GC_thread me;
    DWORD thread_id = GetCurrentThreadId();
    DCL_LOCK_STATE;

    if (GC_need_to_lock == FALSE)
        ABORT("Threads explicit registering is not previously enabled");

    /* We lock here, since we want to wait for an ongoing GC.     */
    LOCK();
    me = GC_lookup_thread_inner(thread_id);
    if (me == 0)
    {
#ifdef GC_PTHREADS
        me = GC_register_my_thread_inner(sb, thread_id);
        me->flags |= DETACHED;
        /* Treat as detached, since we do not need to worry about     */
        /* pointer results.                                           */
#else
        GC_register_my_thread_inner(sb, thread_id);
#endif
        UNLOCK();
        return GC_SUCCESS;
    }
    else
#ifdef GC_PTHREADS
        /* else */ if ((me->flags & FINISHED) != 0)
        {
            GC_record_stack_base(me, sb);
            me->flags &= ~FINISHED; /* but not DETACHED */
#ifdef THREAD_LOCAL_ALLOC
            GC_init_thread_local((GC_tlfs)(&me->tlfs));
#endif
            UNLOCK();
            return GC_SUCCESS;
        }
        else
#endif
        /* else */ {
            UNLOCK();
            return GC_DUPLICATE;
        }
}

/* Similar to that in pthread_support.c.        */
STATIC void GC_wait_for_gc_completion(GC_bool wait_for_all)
{
    GC_ASSERT(I_HOLD_LOCK());
    if (GC_incremental && GC_collection_in_progress())
    {
        word old_gc_no = GC_gc_no;

        /* Make sure that no part of our stack is still on the mark stack,  */
        /* since it's about to be unmapped.                                 */
        do
        {
            ENTER_GC();
            GC_in_thread_creation = TRUE;
            GC_collect_a_little_inner(1);
            GC_in_thread_creation = FALSE;
            EXIT_GC();

            UNLOCK();
            Sleep(0); /* yield */
            LOCK();
        } while (GC_incremental && GC_collection_in_progress()
                 && (wait_for_all || old_gc_no == GC_gc_no));
    }
}

GC_API int GC_CALL GC_unregister_my_thread(void)
{
    DCL_LOCK_STATE;

#ifdef DEBUG_THREADS
    GC_log_printf("Unregistering thread 0x%lx\n", (long)GetCurrentThreadId());
#endif

    if (GC_win32_dll_threads)
    {
#if defined(THREAD_LOCAL_ALLOC)
        /* Can't happen: see GC_use_threads_discovery(). */
        GC_ASSERT(FALSE);
#else
#ifdef GC_PTHREADS
        /* FIXME: If not DETACHED then just set FINISHED. */
#endif
        /* FIXME: Should we just ignore this? */
        GC_delete_thread(GetCurrentThreadId());
#endif
    }
    else
    {
#if defined(THREAD_LOCAL_ALLOC) || defined(GC_PTHREADS)
        GC_thread me;
#endif
        DWORD thread_id = GetCurrentThreadId();

        LOCK();
        GC_wait_for_gc_completion(FALSE);
#if defined(THREAD_LOCAL_ALLOC) || defined(GC_PTHREADS)
        me = GC_lookup_thread_inner(thread_id);
        CHECK_LOOKUP_MY_THREAD(me);
        GC_ASSERT(!KNOWN_FINISHED(me));
#endif
#if defined(THREAD_LOCAL_ALLOC)
        GC_destroy_thread_local(&(me->tlfs));
#endif
#ifdef GC_PTHREADS
        if ((me->flags & DETACHED) == 0)
        {
            me->flags |= FINISHED;
        }
        else
#endif
        /* else */ {
            GC_delete_thread(thread_id);
        }
        UNLOCK();
    }
    return GC_SUCCESS;
}

/* Wrapper for functions that are likely to block for an appreciable    */
/* length of time.                                                      */

/* GC_do_blocking_inner() is nearly the same as in pthread_support.c    */
/*ARGSUSED*/
GC_INNER void GC_do_blocking_inner(ptr_t data, void* context)
{
    struct blocking_data* d = (struct blocking_data*)data;
    DWORD thread_id = GetCurrentThreadId();
    GC_thread me;
#ifdef IA64
    ptr_t stack_ptr = GC_save_regs_in_stack();
#endif
    DCL_LOCK_STATE;

    LOCK();
    me = GC_lookup_thread_inner(thread_id);
    CHECK_LOOKUP_MY_THREAD(me);
    GC_ASSERT(me->thread_blocked_sp == NULL);
#ifdef IA64
    me->backing_store_ptr = stack_ptr;
#endif
    me->thread_blocked_sp = (ptr_t)&d; /* save approx. sp */
    /* Save context here if we want to support precise stack marking */
    UNLOCK();
    d->client_data = (d->fn)(d->client_data);
    LOCK(); /* This will block if the world is stopped. */
    me->thread_blocked_sp = NULL;
    UNLOCK();
}

/* GC_call_with_gc_active() has the opposite to GC_do_blocking()        */
/* functionality.  It might be called from a user function invoked by   */
/* GC_do_blocking() to temporarily back allow calling any GC function   */
/* and/or manipulating pointers to the garbage collected heap.          */
GC_API void* GC_CALL GC_call_with_gc_active(GC_fn_type fn, void* client_data)
{
    struct GC_traced_stack_sect_s stacksect;
    GC_thread me;
    DCL_LOCK_STATE;

    LOCK(); /* This will block if the world is stopped.         */
    me = GC_lookup_thread_inner(GetCurrentThreadId());
    CHECK_LOOKUP_MY_THREAD(me);
    /* Adjust our stack base value (this could happen unless      */
    /* GC_get_stack_base() was used which returned GC_SUCCESS).   */
    GC_ASSERT(me->stack_base != NULL);
    if (me->stack_base < (ptr_t)(&stacksect))
        me->stack_base = (ptr_t)(&stacksect);

    if (me->thread_blocked_sp == NULL)
    {
        /* We are not inside GC_do_blocking() - do nothing more.    */
        UNLOCK();
        return fn(client_data);
    }

    /* Setup new "stack section". */
    stacksect.saved_stack_ptr = me->thread_blocked_sp;
#ifdef IA64
    /* This is the same as in GC_call_with_stack_base().        */
    stacksect.backing_store_end = GC_save_regs_in_stack();
    /* Unnecessarily flushes register stack,    */
    /* but that probably doesn't hurt.          */
    stacksect.saved_backing_store_ptr = me->backing_store_ptr;
#endif
    stacksect.prev = me->traced_stack_sect;
    me->thread_blocked_sp = NULL;
    me->traced_stack_sect = &stacksect;

    UNLOCK();
    client_data = fn(client_data);
    GC_ASSERT(me->thread_blocked_sp == NULL);
    GC_ASSERT(me->traced_stack_sect == &stacksect);

    /* Restore original "stack section".  */
    LOCK();
    me->traced_stack_sect = stacksect.prev;
#ifdef IA64
    me->backing_store_ptr = stacksect.saved_backing_store_ptr;
#endif
    me->thread_blocked_sp = stacksect.saved_stack_ptr;
    UNLOCK();

    return client_data; /* result */
}

#ifdef GC_PTHREADS

/* A quick-and-dirty cache of the mapping between pthread_t   */
/* and win32 thread id.                                       */
#define PTHREAD_MAP_SIZE 512
DWORD GC_pthread_map_cache[PTHREAD_MAP_SIZE] = {0};
#define PTHREAD_MAP_INDEX(pthread_id) \
    ((NUMERIC_THREAD_ID(pthread_id) >> 5) % PTHREAD_MAP_SIZE)
/* It appears pthread_t is really a pointer type ... */
#define SET_PTHREAD_MAP_CACHE(pthread_id, win32_id) \
    (GC_pthread_map_cache[PTHREAD_MAP_INDEX(pthread_id)] = (win32_id))
#define GET_PTHREAD_MAP_CACHE(pthread_id) \
    GC_pthread_map_cache[PTHREAD_MAP_INDEX(pthread_id)]

/* Return a GC_thread corresponding to a given pthread_t.     */
/* Returns 0 if it's not there.                               */
/* We assume that this is only called for pthread ids that    */
/* have not yet terminated or are still joinable, and         */
/* cannot be concurrently terminated.                         */
/* Assumes we do NOT hold the allocation lock.                */
STATIC GC_thread GC_lookup_pthread(pthread_t id)
{
#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        int i;
        LONG my_max = GC_get_max_thread_index();

        for (i = 0; i <= my_max
                    && (!AO_load_acquire(&dll_thread_table[i].tm.in_use)
                        || THREAD_EQUAL(dll_thread_table[i].pthread_id, id));
             /* Must still be in_use, since nobody else can      */
             /* store our thread_id.                             */
             i++)
        {
            /* empty */
        }
        return i <= my_max ? (GC_thread)(dll_thread_table + i) : NULL;
    }
    else
#endif
    /* else */ {
        /* We first try the cache.  If that fails, we use a very slow     */
        /* approach.                                                      */
        word hv_guess = THREAD_TABLE_INDEX(GET_PTHREAD_MAP_CACHE(id));
        int hv;
        GC_thread p;
        DCL_LOCK_STATE;

        LOCK();
        for (p = GC_threads[hv_guess]; 0 != p; p = p->tm.next)
        {
            if (THREAD_EQUAL(p->pthread_id, id))
                goto foundit;
        }
        for (hv = 0; hv < THREAD_TABLE_SZ; ++hv)
        {
            for (p = GC_threads[hv]; 0 != p; p = p->tm.next)
            {
                if (THREAD_EQUAL(p->pthread_id, id))
                    goto foundit;
            }
        }
        p = 0;
    foundit:
        UNLOCK();
        return p;
    }
}

#endif /* GC_PTHREADS */

#ifdef CAN_HANDLE_FORK
/* Similar to that in pthread_support.c but also rehashes the table */
/* since hash map key (thread_id) differs from that in the parent.  */
STATIC void GC_remove_all_threads_but_me(void)
{
    int hv;
    GC_thread p, next, me = NULL;
    DWORD thread_id;
    pthread_t pthread_id = pthread_self(); /* same as in parent */

    GC_ASSERT(!GC_win32_dll_threads);
    for (hv = 0; hv < THREAD_TABLE_SZ; ++hv)
    {
        for (p = GC_threads[hv]; 0 != p; p = next)
        {
            next = p->tm.next;
            if (THREAD_EQUAL(p->pthread_id, pthread_id))
            {
                GC_ASSERT(me == NULL);
                me = p;
                p->tm.next = 0;
            }
            else
            {
#ifdef THREAD_LOCAL_ALLOC
                if ((p->flags & FINISHED) == 0)
                {
                    GC_destroy_thread_local(&p->tlfs);
                }
#endif
                if (&first_thread != p)
                    GC_INTERNAL_FREE(p);
            }
        }
        GC_threads[hv] = NULL;
    }

    /* Put "me" back to GC_threads.   */
    GC_ASSERT(me != NULL);
    thread_id = GetCurrentThreadId(); /* differs from that in parent */
    GC_threads[THREAD_TABLE_INDEX(thread_id)] = me;

    /* Update Win32 thread Id and handle.     */
    me->id = thread_id;
#ifndef MSWINCE
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), (HANDLE*)&me->handle,
                         0 /* dwDesiredAccess */, FALSE /* bInheritHandle */,
                         DUPLICATE_SAME_ACCESS))
        ABORT("DuplicateHandle failed");
#endif

#if defined(THREAD_LOCAL_ALLOC) && !defined(USE_CUSTOM_SPECIFIC)
    /* For Cygwin, we need to re-assign thread-local pointer to     */
    /* 'tlfs' (it is ok to call GC_destroy_thread_local and         */
    /* GC_free_internal before this action).                        */
    if (GC_setspecific(GC_thread_key, &me->tlfs) != 0)
        ABORT("GC_setspecific failed (in child)");
#endif
}

STATIC void GC_fork_prepare_proc(void)
{
    LOCK();
#ifdef PARALLEL_MARK
    if (GC_parallel)
        GC_wait_for_reclaim();
#endif
    GC_wait_for_gc_completion(TRUE);
#ifdef PARALLEL_MARK
    if (GC_parallel)
        GC_acquire_mark_lock();
#endif
}

STATIC void GC_fork_parent_proc(void)
{
#ifdef PARALLEL_MARK
    if (GC_parallel)
        GC_release_mark_lock();
#endif
    UNLOCK();
}

STATIC void GC_fork_child_proc(void)
{
#ifdef PARALLEL_MARK
    if (GC_parallel)
    {
        GC_release_mark_lock();
        GC_markers = 1;
        GC_parallel = FALSE;
        /* Turn off parallel marking in the child, since we are */
        /* probably just going to exec, and we would have to    */
        /* restart mark threads.                                */
    }
#endif
    GC_remove_all_threads_but_me();
    UNLOCK();
}
#endif /* CAN_HANDLE_FORK */

void GC_push_thread_structures(void)
{
    GC_ASSERT(I_HOLD_LOCK());
#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        /* Unlike the other threads implementations, the thread table here */
        /* contains no pointers to the collectable heap.  Thus we have     */
        /* no private structures we need to preserve.                      */
#ifdef GC_PTHREADS
        int i; /* pthreads may keep a pointer in the thread exit value */
        LONG my_max = GC_get_max_thread_index();

        for (i = 0; i <= my_max; i++)
            if (dll_thread_table[i].tm.in_use)
                GC_push_all((ptr_t) & (dll_thread_table[i].status),
                            (ptr_t)(&(dll_thread_table[i].status) + 1));
#endif
    }
    else
#endif
    /* else */ {
        GC_push_all((ptr_t)(GC_threads),
                    (ptr_t)(GC_threads) + sizeof(GC_threads));
    }
#if defined(THREAD_LOCAL_ALLOC)
    GC_push_all((ptr_t)(&GC_thread_key),
                (ptr_t)(&GC_thread_key) + sizeof(&GC_thread_key));
    /* Just in case we ever use our own TLS implementation.     */
#endif
}

/* Suspend the given thread, if it's still active.      */
STATIC void GC_suspend(GC_thread t)
{
#ifndef MSWINCE
    /* Apparently the Windows 95 GetOpenFileName call creates           */
    /* a thread that does not properly get cleaned up, and              */
    /* SuspendThread on its descriptor may provoke a crash.             */
    /* This reduces the probability of that event, though it still      */
    /* appears there's a race here.                                     */
    DWORD exitCode;
#endif
    UNPROTECT_THREAD(t);
#ifndef MSWINCE
    if (GetExitCodeThread(t->handle, &exitCode) && exitCode != STILL_ACTIVE)
    {
#ifdef GC_PTHREADS
        t->stack_base = 0; /* prevent stack from being pushed */
#else
        /* this breaks pthread_join on Cygwin, which is guaranteed to  */
        /* only see user pthreads                                      */
        GC_ASSERT(GC_win32_dll_threads);
        GC_delete_gc_thread(t);
#endif
        return;
    }
#endif
#if defined(MPROTECT_VDB)
    /* Acquire the spin lock we use to update dirty bits.       */
    /* Threads shouldn't get stopped holding it.  But we may    */
    /* acquire and release it in the UNPROTECT_THREAD call.     */
    while (AO_test_and_set_acquire(&GC_fault_handler_lock) == AO_TS_SET)
    {
        /* empty */
    }
#endif

#ifdef MSWINCE
    /* SuspendThread() will fail if thread is running kernel code.      */
    while (SuspendThread(THREAD_HANDLE(t)) == (DWORD)-1)
        Sleep(10); /* in millis */
#else
    if (SuspendThread(t->handle) == (DWORD)-1)
        ABORT("SuspendThread failed");
#endif /* !MSWINCE */
    t->suspended = (unsigned char)TRUE;
#if defined(MPROTECT_VDB)
    AO_CLEAR(&GC_fault_handler_lock);
#endif
}

#if defined(GC_ASSERTIONS) && !defined(CYGWIN32)
GC_INNER GC_bool GC_write_disabled = FALSE;
/* TRUE only if GC_stop_world() acquired GC_write_cs.   */
#endif

GC_INNER void GC_stop_world(void)
{
    DWORD thread_id = GetCurrentThreadId();

    if (!GC_thr_initialized)
        ABORT("GC_stop_world() called before GC_thr_init()");
    GC_ASSERT(I_HOLD_LOCK());

    /* This code is the same as in pthread_stop_world.c */
#ifdef PARALLEL_MARK
    if (GC_parallel)
    {
        GC_acquire_mark_lock();
        GC_ASSERT(GC_fl_builder_count == 0);
        /* We should have previously waited for it to become zero. */
    }
#endif /* PARALLEL_MARK */

#if !defined(GC_NO_THREADS_DISCOVERY) || defined(GC_ASSERTIONS)
    GC_please_stop = TRUE;
#endif
#ifndef CYGWIN32
    GC_ASSERT(!GC_write_disabled);
    EnterCriticalSection(&GC_write_cs);
    /* It's not allowed to call GC_printf() (and friends) here down to  */
    /* LeaveCriticalSection (same applies recursively to                */
    /* GC_get_max_thread_index(), GC_suspend(), GC_delete_gc_thread()   */
    /* (only if GC_win32_dll_threads), GC_size() and                    */
    /* GC_remove_protection()).                                         */
#ifdef GC_ASSERTIONS
    GC_write_disabled = TRUE;
#endif
#endif
#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        int i;
        int my_max;
        /* Any threads being created during this loop will end up setting */
        /* GC_attached_thread when they start.  This will force marking   */
        /* to restart.  This is not ideal, but hopefully correct.         */
        GC_attached_thread = FALSE;
        my_max = (int)GC_get_max_thread_index();
        for (i = 0; i <= my_max; i++)
        {
            GC_vthread t = dll_thread_table + i;
            if (t->stack_base != 0 && t->thread_blocked_sp == NULL
                && t->id != thread_id)
            {
                GC_suspend((GC_thread)t);
            }
        }
    }
    else
#endif
    /* else */ {
        GC_thread t;
        int i;

        for (i = 0; i < THREAD_TABLE_SZ; i++)
        {
            for (t = GC_threads[i]; t != 0; t = t->tm.next)
            {
                if (t->stack_base != 0 && t->thread_blocked_sp == NULL
                    && !KNOWN_FINISHED(t) && t->id != thread_id)
                {
                    GC_suspend(t);
                }
            }
        }
    }
#ifndef CYGWIN32
#ifdef GC_ASSERTIONS
    GC_write_disabled = FALSE;
#endif
    LeaveCriticalSection(&GC_write_cs);
#endif
#ifdef PARALLEL_MARK
    if (GC_parallel)
        GC_release_mark_lock();
#endif
}

GC_INNER void GC_start_world(void)
{
#ifdef GC_ASSERTIONS
    DWORD thread_id = GetCurrentThreadId();
#endif
    int i;

    GC_ASSERT(I_HOLD_LOCK());
    if (GC_win32_dll_threads)
    {
        LONG my_max = GC_get_max_thread_index();
        for (i = 0; i <= my_max; i++)
        {
            GC_thread t = (GC_thread)(dll_thread_table + i);
            if (t->suspended)
            {
                GC_ASSERT(t->stack_base != 0 && t->id != thread_id);
                if (ResumeThread(THREAD_HANDLE(t)) == (DWORD)-1)
                    ABORT("ResumeThread failed");
                t->suspended = FALSE;
            }
        }
    }
    else
    {
        GC_thread t;
        int i;

        for (i = 0; i < THREAD_TABLE_SZ; i++)
        {
            for (t = GC_threads[i]; t != 0; t = t->tm.next)
            {
                if (t->suspended)
                {
                    GC_ASSERT(t->stack_base != 0 && t->id != thread_id);
                    if (ResumeThread(THREAD_HANDLE(t)) == (DWORD)-1)
                        ABORT("ResumeThread failed");
                    UNPROTECT_THREAD(t);
                    t->suspended = FALSE;
                }
            }
        }
    }
#if !defined(GC_NO_THREADS_DISCOVERY) || defined(GC_ASSERTIONS)
    GC_please_stop = FALSE;
#endif
}

#ifdef MSWINCE
/* The VirtualQuery calls below won't work properly on some old WinCE */
/* versions, but since each stack is restricted to an aligned 64 KiB  */
/* region of virtual memory we can just take the next lowest multiple */
/* of 64 KiB.  The result of this macro must not be used as its       */
/* argument later and must not be used as the lower bound for sp      */
/* check (since the stack may be bigger than 64 KiB).                 */
#define GC_wince_evaluate_stack_min(s) (ptr_t)(((word)(s) - 1) & ~(word)0xFFFF)
#elif defined(GC_ASSERTIONS)
#define GC_dont_query_stack_min FALSE
#endif

/* A cache holding the results of the recent VirtualQuery call. */
/* Protected by the allocation lock.                            */
static ptr_t last_address = 0;
static MEMORY_BASIC_INFORMATION last_info;

/* Probe stack memory region (starting at "s") to find out its  */
/* lowest address (i.e. stack top).                             */
/* S must be a mapped address inside the region, NOT the first  */
/* unmapped address.                                            */
STATIC ptr_t GC_get_stack_min(ptr_t s)
{
    ptr_t bottom;

    GC_ASSERT(I_HOLD_LOCK());
    if (s != last_address)
    {
        VirtualQuery(s, &last_info, sizeof(last_info));
        last_address = s;
    }
    do
    {
        bottom = last_info.BaseAddress;
        VirtualQuery(bottom - 1, &last_info, sizeof(last_info));
        last_address = bottom - 1;
    } while ((last_info.Protect & PAGE_READWRITE)
             && !(last_info.Protect & PAGE_GUARD));
    return (bottom);
}

/* Return true if the page at s has protections appropriate     */
/* for a stack page.                                            */
static GC_bool may_be_in_stack(ptr_t s)
{
    GC_ASSERT(I_HOLD_LOCK());
    if (s != last_address)
    {
        VirtualQuery(s, &last_info, sizeof(last_info));
        last_address = s;
    }
    return (last_info.Protect & PAGE_READWRITE)
           && !(last_info.Protect & PAGE_GUARD);
}

STATIC word GC_push_stack_for(GC_thread thread, DWORD me)
{
    ptr_t sp, stack_min;

    struct GC_traced_stack_sect_s* traced_stack_sect =
        thread->traced_stack_sect;
    if (thread->id == me)
    {
        GC_ASSERT(thread->thread_blocked_sp == NULL);
        sp = GC_approx_sp();
    }
    else if ((sp = thread->thread_blocked_sp) == NULL)
    {
        /* Use saved sp value for blocked threads. */
        /* For unblocked threads call GetThreadContext().   */
        CONTEXT context;
        context.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
        if (!GetThreadContext(THREAD_HANDLE(thread), &context))
            ABORT("GetThreadContext failed");

        /* Push all registers that might point into the heap.  Frame        */
        /* pointer registers are included in case client code was           */
        /* compiled with the 'omit frame pointer' optimisation.             */
#define PUSH1(reg) GC_push_one((word)context.reg)
#define PUSH2(r1, r2) PUSH1(r1), PUSH1(r2)
#define PUSH4(r1, r2, r3, r4) PUSH2(r1, r2), PUSH2(r3, r4)
#if defined(I386)
        PUSH4(Edi, Esi, Ebx, Edx), PUSH2(Ecx, Eax), PUSH1(Ebp);
        sp = (ptr_t)context.Esp;
#elif defined(X86_64)
        PUSH4(Rax, Rcx, Rdx, Rbx);
        PUSH2(Rbp, Rsi);
        PUSH1(Rdi);
        PUSH4(R8, R9, R10, R11);
        PUSH4(R12, R13, R14, R15);
        sp = (ptr_t)context.Rsp;
#elif defined(ARM32)
        PUSH4(R0, R1, R2, R3), PUSH4(R4, R5, R6, R7), PUSH4(R8, R9, R10, R11);
        PUSH1(R12);
        sp = (ptr_t)context.Sp;
#elif defined(SHx)
        PUSH4(R0, R1, R2, R3), PUSH4(R4, R5, R6, R7), PUSH4(R8, R9, R10, R11);
        PUSH2(R12, R13), PUSH1(R14);
        sp = (ptr_t)context.R15;
#elif defined(MIPS)
        PUSH4(IntAt, IntV0, IntV1, IntA0), PUSH4(IntA1, IntA2, IntA3, IntT0);
        PUSH4(IntT1, IntT2, IntT3, IntT4), PUSH4(IntT5, IntT6, IntT7, IntS0);
        PUSH4(IntS1, IntS2, IntS3, IntS4), PUSH4(IntS5, IntS6, IntS7, IntT8);
        PUSH4(IntT9, IntK0, IntK1, IntS8);
        sp = (ptr_t)context.IntSp;
#elif defined(PPC)
        PUSH4(Gpr0, Gpr3, Gpr4, Gpr5), PUSH4(Gpr6, Gpr7, Gpr8, Gpr9);
        PUSH4(Gpr10, Gpr11, Gpr12, Gpr14), PUSH4(Gpr15, Gpr16, Gpr17, Gpr18);
        PUSH4(Gpr19, Gpr20, Gpr21, Gpr22), PUSH4(Gpr23, Gpr24, Gpr25, Gpr26);
        PUSH4(Gpr27, Gpr28, Gpr29, Gpr30), PUSH1(Gpr31);
        sp = (ptr_t)context.Gpr1;
#elif defined(ALPHA)
        PUSH4(IntV0, IntT0, IntT1, IntT2), PUSH4(IntT3, IntT4, IntT5, IntT6);
        PUSH4(IntT7, IntS0, IntS1, IntS2), PUSH4(IntS3, IntS4, IntS5, IntFp);
        PUSH4(IntA0, IntA1, IntA2, IntA3), PUSH4(IntA4, IntA5, IntT8, IntT9);
        PUSH4(IntT10, IntT11, IntT12, IntAt);
        sp = (ptr_t)context.IntSp;
#else
#error "architecture is not supported"
#endif
    } /* ! current thread */

    /* Set stack_min to the lowest address in the thread stack,   */
    /* or to an address in the thread stack no larger than sp,    */
    /* taking advantage of the old value to avoid slow traversals */
    /* of large stacks.                                           */
    if (thread->last_stack_min == ADDR_LIMIT)
    {
#ifdef MSWINCE
        if (GC_dont_query_stack_min)
        {
            stack_min = GC_wince_evaluate_stack_min(
                traced_stack_sect != NULL ? (ptr_t)traced_stack_sect
                                          : thread->stack_base);
            /* Keep last_stack_min value unmodified. */
        }
        else
#endif
        /* else */ {
            stack_min = GC_get_stack_min(traced_stack_sect != NULL
                                             ? (ptr_t)traced_stack_sect
                                             : thread->stack_base);
            UNPROTECT_THREAD(thread);
            thread->last_stack_min = stack_min;
        }
    }
    else
    {
        /* First, adjust the latest known minimum stack address if we       */
        /* are inside GC_call_with_gc_active().                             */
        if (traced_stack_sect != NULL
            && thread->last_stack_min > (ptr_t)traced_stack_sect)
        {
            UNPROTECT_THREAD(thread);
            thread->last_stack_min = (ptr_t)traced_stack_sect;
        }

        if (sp < thread->stack_base && sp >= thread->last_stack_min)
        {
            stack_min = sp;
        }
        else
        {
            /* In the current thread it is always safe to use sp value.       */
            if (may_be_in_stack(thread->id == me && sp < thread->last_stack_min
                                    ? sp
                                    : thread->last_stack_min))
            {
                stack_min = last_info.BaseAddress;
                /* Do not probe rest of the stack if sp is correct. */
                if (sp < stack_min || sp >= thread->stack_base)
                    stack_min = GC_get_stack_min(thread->last_stack_min);
            }
            else
            {
                /* Stack shrunk?  Is this possible? */
                stack_min = GC_get_stack_min(thread->stack_base);
            }
            UNPROTECT_THREAD(thread);
            thread->last_stack_min = stack_min;
        }
    }

    GC_ASSERT(GC_dont_query_stack_min
              || stack_min == GC_get_stack_min(thread->stack_base)
              || (sp >= stack_min && stack_min < thread->stack_base
                  && stack_min > GC_get_stack_min(thread->stack_base)));

    if (sp >= stack_min && sp < thread->stack_base)
    {
#ifdef DEBUG_THREADS
        GC_log_printf("Pushing stack for 0x%x from sp %p to %p from 0x%x\n",
                      (int)thread->id, sp, thread->stack_base, (int)me);
#endif
        GC_push_all_stack_sections(sp, thread->stack_base, traced_stack_sect);
    }
    else
    {
        /* If not current thread then it is possible for sp to point to     */
        /* the guarded (untouched yet) page just below the current          */
        /* stack_min of the thread.                                         */
        if (thread->id == me || sp >= thread->stack_base
            || sp + GC_page_size < stack_min)
            WARN("Thread stack pointer %p out of range, pushing everything\n",
                 sp);
#ifdef DEBUG_THREADS
        GC_log_printf("Pushing stack for 0x%x from (min) %p to %p from 0x%x\n",
                      (int)thread->id, stack_min, thread->stack_base, (int)me);
#endif
        /* Push everything - ignore "traced stack section" data.            */
        GC_push_all_stack(stack_min, thread->stack_base);
    }
    return thread->stack_base - sp; /* stack grows down */
}

GC_INNER void GC_push_all_stacks(void)
{
    DWORD thread_id = GetCurrentThreadId();
    GC_bool found_me = FALSE;
#ifndef SMALL_CONFIG
    unsigned nthreads = 0;
#endif
    word total_size = 0;
#ifndef GC_NO_THREADS_DISCOVERY
    if (GC_win32_dll_threads)
    {
        int i;
        LONG my_max = GC_get_max_thread_index();

        for (i = 0; i <= my_max; i++)
        {
            GC_thread t = (GC_thread)(dll_thread_table + i);
            if (t->tm.in_use && t->stack_base)
            {
#ifndef SMALL_CONFIG
                ++nthreads;
#endif
                total_size += GC_push_stack_for(t, thread_id);
                if (t->id == thread_id)
                    found_me = TRUE;
            }
        }
    }
    else
#endif
    /* else */ {
        int i;
        for (i = 0; i < THREAD_TABLE_SZ; i++)
        {
            GC_thread t;
            for (t = GC_threads[i]; t != 0; t = t->tm.next)
            {
                if (!KNOWN_FINISHED(t) && t->stack_base)
                {
#ifndef SMALL_CONFIG
                    ++nthreads;
#endif
                    total_size += GC_push_stack_for(t, thread_id);
                    if (t->id == thread_id)
                        found_me = TRUE;
                }
            }
        }
    }
#ifndef SMALL_CONFIG
    if (GC_print_stats == VERBOSE)
    {
        GC_log_printf("Pushed %d thread stacks%s\n", nthreads,
                      GC_win32_dll_threads ? " based on DllMain thread tracking"
                                           : "");
    }
#endif
    if (!found_me && !GC_in_thread_creation)
        ABORT("Collecting from unknown thread");
    GC_total_stacksize = total_size;
}

#ifdef PARALLEL_MARK

#ifndef MAX_MARKERS
#define MAX_MARKERS 16
#endif

static ptr_t marker_sp[MAX_MARKERS - 1]; /* The cold end of the stack */
                                         /* for markers.              */
#ifdef IA64
static ptr_t marker_bsp[MAX_MARKERS - 1];
#endif

static ptr_t marker_last_stack_min[MAX_MARKERS - 1];
/* Last known minimum (hottest) address */
/* in stack (or ADDR_LIMIT if unset)    */
/* for markers.                         */

#endif /* PARALLEL_MARK */

/* Find stack with the lowest address which overlaps the        */
/* interval [start, limit).                                     */
/* Return stack bounds in *lo and *hi.  If no such stack        */
/* is found, both *hi and *lo will be set to an address         */
/* higher than limit.                                           */
GC_INNER void GC_get_next_stack(char* start, char* limit, char** lo, char** hi)
{
    int i;
    char* current_min = ADDR_LIMIT; /* Least in-range stack base      */
    ptr_t* plast_stack_min = NULL;  /* Address of last_stack_min      */
                                    /* field for thread corresponding */
                                    /* to current_min.                */
    GC_thread thread = NULL;        /* Either NULL or points to the   */
                                    /* thread's hash table entry      */
                                    /* containing *plast_stack_min.   */

    /* First set current_min, ignoring limit. */
    if (GC_win32_dll_threads)
    {
        LONG my_max = GC_get_max_thread_index();

        for (i = 0; i <= my_max; i++)
        {
            ptr_t s = (ptr_t)(dll_thread_table[i].stack_base);

            if (s > start && s < current_min)
            {
                /* Update address of last_stack_min. */
                plast_stack_min = (ptr_t* /* no volatile */)
                                  & dll_thread_table[i].last_stack_min;
                current_min = s;
            }
        }
    }
    else
    {
        for (i = 0; i < THREAD_TABLE_SZ; i++)
        {
            GC_thread t;

            for (t = GC_threads[i]; t != 0; t = t->tm.next)
            {
                ptr_t s = t->stack_base;

                if (s > start && s < current_min)
                {
                    /* Update address of last_stack_min. */
                    plast_stack_min = &t->last_stack_min;
                    thread = t; /* Remember current thread to unprotect. */
                    current_min = s;
                }
            }
        }
#ifdef PARALLEL_MARK
        for (i = 0; i < GC_markers - 1; ++i)
        {
            ptr_t s = marker_sp[i];
#ifdef IA64
            /* FIXME: not implemented */
#endif
            if (s > start && s < current_min)
            {
                GC_ASSERT(marker_last_stack_min[i] != NULL);
                plast_stack_min = &marker_last_stack_min[i];
                current_min = s;
                thread = NULL; /* Not a thread's hash table entry. */
            }
        }
#endif
    }

    *hi = current_min;
    if (current_min == ADDR_LIMIT)
    {
        *lo = ADDR_LIMIT;
        return;
    }

    GC_ASSERT(current_min > start && plast_stack_min != NULL);
#ifdef MSWINCE
    if (GC_dont_query_stack_min)
    {
        *lo = GC_wince_evaluate_stack_min(current_min);
        /* Keep last_stack_min value unmodified. */
        return;
    }
#endif

    if (current_min > limit && !may_be_in_stack(limit))
    {
        /* Skip the rest since the memory region at limit address is        */
        /* not a stack (so the lowest address of the found stack would      */
        /* be above the limit value anyway).                                */
        *lo = ADDR_LIMIT;
        return;
    }

    /* Get the minimum address of the found stack by probing its memory   */
    /* region starting from the recent known minimum (if set).            */
    if (*plast_stack_min == ADDR_LIMIT || !may_be_in_stack(*plast_stack_min))
    {
        /* Unsafe to start from last_stack_min value. */
        *lo = GC_get_stack_min(current_min);
    }
    else
    {
        /* Use the recent value to optimize search for min address. */
        *lo = GC_get_stack_min(*plast_stack_min);
    }

    /* Remember current stack_min value. */
    if (thread != NULL)
    {
        UNPROTECT_THREAD(thread);
    }
    *plast_stack_min = *lo;
}

#ifdef PARALLEL_MARK

#if defined(GC_PTHREADS) && !defined(GC_PTHREADS_PARAMARK)
/* Use pthread-based parallel mark implementation.    */
#define GC_PTHREADS_PARAMARK
#endif

/* GC_mark_thread() is the same as in pthread_support.c */
#ifdef GC_PTHREADS_PARAMARK
STATIC void* GC_mark_thread(void* id)
#else
#ifdef MSWINCE
STATIC DWORD WINAPI GC_mark_thread(LPVOID id)
#else
STATIC unsigned __stdcall GC_mark_thread(void* id)
#endif
#endif
{
    word my_mark_no = 0;

    if ((word)id == (word)-1)
        return 0; /* to make compiler happy */
    marker_sp[(word)id] = GC_approx_sp();
#ifdef IA64
    marker_bsp[(word)id] = GC_save_regs_in_stack();
#endif

    for (;; ++my_mark_no)
    {
        if (my_mark_no - GC_mark_no > (word)2)
        {
            /* resynchronize if we get far off, e.g. because GC_mark_no     */
            /* wrapped.                                                     */
            my_mark_no = GC_mark_no;
        }
#ifdef DEBUG_THREADS
        GC_log_printf("Starting mark helper for mark number %lu\n",
                      (unsigned long)my_mark_no);
#endif
        GC_help_marker(my_mark_no);
    }
}

#ifdef GC_ASSERTIONS
GC_INNER unsigned long GC_mark_lock_holder = NO_THREAD;
#endif

/* GC_mark_threads[] is unused here unlike that in pthread_support.c  */

#ifdef GC_PTHREADS_PARAMARK
#include <pthread.h>

#ifndef NUMERIC_THREAD_ID
#define NUMERIC_THREAD_ID(id) (unsigned long)GC_PTHREAD_PTRVAL(id)
#endif

/* start_mark_threads() is the same as in pthread_support.c except for: */
/* - GC_markers value is adjusted already;                              */
/* - thread stack is assumed to be large enough; and                    */
/* - statistics about the number of marker threads is printed outside.  */
static void start_mark_threads(void)
{
    int i;
    pthread_attr_t attr;
    pthread_t new_thread;

    if (0 != pthread_attr_init(&attr))
        ABORT("pthread_attr_init failed");

    if (0 != pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
        ABORT("pthread_attr_setdetachstate failed");

    for (i = 0; i < GC_markers - 1; ++i)
    {
        marker_last_stack_min[i] = ADDR_LIMIT;
        if (0
            != pthread_create(&new_thread, &attr, GC_mark_thread,
                              (void*)(word)i))
        {
            WARN("Marker thread creation failed.\n", 0);
            /* Don't try to create other marker threads.    */
            GC_markers = i + 1;
            if (i == 0)
                GC_parallel = FALSE;
            break;
        }
    }
    pthread_attr_destroy(&attr);
}

static pthread_mutex_t mark_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t builder_cv = PTHREAD_COND_INITIALIZER;

/* GC_acquire/release_mark_lock(), GC_wait_builder/marker(),          */
/* GC_wait_for_reclaim(), GC_notify_all_builder/marker() are the same */
/* as in pthread_support.c except that GC_generic_lock() is not used. */

#ifdef LOCK_STATS
AO_t GC_block_count = 0;
#endif

GC_INNER void GC_acquire_mark_lock(void)
{
    if (pthread_mutex_lock(&mark_mutex) != 0)
    {
        ABORT("pthread_mutex_lock failed");
    }
#ifdef LOCK_STATS
    (void)AO_fetch_and_add1(&GC_block_count);
#endif
    /* GC_generic_lock(&mark_mutex); */
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NUMERIC_THREAD_ID(pthread_self());
#endif
}

GC_INNER void GC_release_mark_lock(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NO_THREAD;
#endif
    if (pthread_mutex_unlock(&mark_mutex) != 0)
    {
        ABORT("pthread_mutex_unlock failed");
    }
}

/* Collector must wait for a freelist builders for 2 reasons:       */
/* 1) Mark bits may still be getting examined without lock.         */
/* 2) Partial free lists referenced only by locals may not be       */
/* scanned correctly, e.g. if they contain "pointer-free" objects,  */
/* since the free-list link may be ignored.                         */
STATIC void GC_wait_builder(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NO_THREAD;
#endif
    if (pthread_cond_wait(&builder_cv, &mark_mutex) != 0)
    {
        ABORT("pthread_cond_wait failed");
    }
    GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NUMERIC_THREAD_ID(pthread_self());
#endif
}

GC_INNER void GC_wait_for_reclaim(void)
{
    GC_acquire_mark_lock();
    while (GC_fl_builder_count > 0)
    {
        GC_wait_builder();
    }
    GC_release_mark_lock();
}

GC_INNER void GC_notify_all_builder(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
    if (pthread_cond_broadcast(&builder_cv) != 0)
    {
        ABORT("pthread_cond_broadcast failed");
    }
}

static pthread_cond_t mark_cv = PTHREAD_COND_INITIALIZER;

GC_INNER void GC_wait_marker(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NO_THREAD;
#endif
    if (pthread_cond_wait(&mark_cv, &mark_mutex) != 0)
    {
        ABORT("pthread_cond_wait failed");
    }
    GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NUMERIC_THREAD_ID(pthread_self());
#endif
}

GC_INNER void GC_notify_all_marker(void)
{
    if (pthread_cond_broadcast(&mark_cv) != 0)
    {
        ABORT("pthread_cond_broadcast failed");
    }
}

#else /* ! GC_PTHREADS_PARAMARK */

#ifdef DONT_USE_SIGNALANDWAIT
STATIC HANDLE GC_marker_cv[MAX_MARKERS - 1] = {0};
/* Events with manual reset (one for each       */
/* mark helper).                                */

STATIC DWORD GC_marker_Id[MAX_MARKERS - 1] = {0};
/* This table is used for mapping helper        */
/* threads ID to mark helper index (linear      */
/* search is used since the mapping contains    */
/* only a few entries).                         */
#endif

#ifndef MARK_THREAD_STACK_SIZE
#define MARK_THREAD_STACK_SIZE 0 /* default value */
#endif

/* mark_mutex_event, builder_cv, mark_cv are initialized in GC_thr_init */
static HANDLE mark_mutex_event = (HANDLE)0; /* Event with auto-reset.   */
static HANDLE builder_cv = (HANDLE)0;       /* Event with manual reset.       */
static HANDLE mark_cv = (HANDLE)0; /* Event with manual reset.          */

static void start_mark_threads(void)
{
    int i;
#ifdef MSWINCE
    HANDLE handle;
    DWORD thread_id;
#else
    GC_uintptr_t handle;
    unsigned thread_id;
#endif

#ifdef DONT_USE_SIGNALANDWAIT
    /* Initialize GC_marker_cv[] and GC_marker_Id[] fully before    */
    /* starting the first helper thread.                            */
    for (i = 0; i < GC_markers - 1; ++i)
    {
        GC_marker_Id[i] = GetCurrentThreadId();
        if ((GC_marker_cv[i] =
                 CreateEvent(NULL /* attrs */, TRUE /* isManualReset */,
                             FALSE /* initialState */, NULL /* name (A/W) */))
            == (HANDLE)0)
            ABORT("CreateEvent() failed");
    }
#endif

    for (i = 0; i < GC_markers - 1; ++i)
    {
        marker_last_stack_min[i] = ADDR_LIMIT;
#ifdef MSWINCE
        /* There is no _beginthreadex() in WinCE. */
        handle = CreateThread(
            NULL /* lpsa */, MARK_THREAD_STACK_SIZE /* ignored */,
            GC_mark_thread, (LPVOID)(word)i, 0 /* fdwCreate */, &thread_id);
        if (handle == NULL)
        {
            WARN("Marker thread creation failed\n", 0);
            /* The most probable failure reason is "not enough memory". */
            /* Don't try to create other marker threads.                */
            break;
        }
        else
        {
            /* It's safe to detach the thread.  */
            CloseHandle(handle);
        }
#else
        handle = _beginthreadex(NULL /* security_attr */,
                                MARK_THREAD_STACK_SIZE, GC_mark_thread,
                                (void*)(word)i, 0 /* flags */, &thread_id);
        if (!handle || handle == (GC_uintptr_t)-1L)
        {
            WARN("Marker thread creation failed\n", 0);
            /* Don't try to create other marker threads.                */
            break;
        }
        else
        { /* We may detach the thread (if handle is of HANDLE type) */
            /* CloseHandle((HANDLE)handle); */
        }
#endif
    }

    /* Adjust GC_markers (and free unused resources) in case of failure. */
#ifdef DONT_USE_SIGNALANDWAIT
    while ((int)GC_markers > i + 1)
    {
        GC_markers--;
        CloseHandle(GC_marker_cv[(int)GC_markers - 1]);
    }
#else
    GC_markers = i + 1;
#endif
    if (i == 0)
    {
        GC_parallel = FALSE;
        CloseHandle(mark_cv);
        CloseHandle(builder_cv);
        CloseHandle(mark_mutex_event);
    }
}

#ifdef DONT_USE_SIGNALANDWAIT
STATIC /* volatile */ LONG GC_mark_mutex_state = 0;
/* Mutex state: 0 - unlocked,           */
/* 1 - locked and no other waiters,     */
/* -1 - locked and waiters may exist.   */
/* Accessed by InterlockedExchange().   */
#else
STATIC volatile AO_t GC_mark_mutex_waitcnt = 0;
/* Number of waiters + 1; 0 - unlocked. */
#endif

/* #define LOCK_STATS */
#ifdef LOCK_STATS
AO_t GC_block_count = 0;
AO_t GC_unlocked_count = 0;
#endif

GC_INNER void GC_acquire_mark_lock(void)
{
#ifdef DONT_USE_SIGNALANDWAIT
    if (InterlockedExchange(&GC_mark_mutex_state, 1 /* locked */) != 0)
#else
    if (AO_fetch_and_add1_acquire(&GC_mark_mutex_waitcnt) != 0)
#endif
    {
#ifdef LOCK_STATS
        (void)AO_fetch_and_add1(&GC_block_count);
#endif
#ifdef DONT_USE_SIGNALANDWAIT
        /* Repeatedly reset the state and wait until acquire the lock. */
        while (InterlockedExchange(&GC_mark_mutex_state,
                                   -1 /* locked_and_has_waiters */)
               != 0)
#endif
        {
            if (WaitForSingleObject(mark_mutex_event, INFINITE) == WAIT_FAILED)
                ABORT("WaitForSingleObject() failed");
        }
    }
#ifdef LOCK_STATS
    else
    {
        (void)AO_fetch_and_add1(&GC_unlocked_count);
    }
#endif

    GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = (unsigned long)GetCurrentThreadId();
#endif
}

GC_INNER void GC_release_mark_lock(void)
{
    GC_ASSERT(GC_mark_lock_holder == (unsigned long)GetCurrentThreadId());
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NO_THREAD;
#endif
#ifdef DONT_USE_SIGNALANDWAIT
    if (InterlockedExchange(&GC_mark_mutex_state, 0 /* unlocked */) < 0)
#else
    GC_ASSERT(AO_load(&GC_mark_mutex_waitcnt) != 0);
    if (AO_fetch_and_sub1_release(&GC_mark_mutex_waitcnt) > 1)
#endif
    {
        /* wake a waiter */
        if (SetEvent(mark_mutex_event) == FALSE)
            ABORT("SetEvent() failed");
    }
}

/* In GC_wait_for_reclaim/GC_notify_all_builder() we emulate POSIX    */
/* cond_wait/cond_broadcast() primitives with WinAPI Event object     */
/* (working in "manual reset" mode).  This works here because         */
/* GC_notify_all_builder() is always called holding lock on           */
/* mark_mutex and the checked condition (GC_fl_builder_count == 0)    */
/* is the only one for which broadcasting on builder_cv is performed. */

GC_INNER void GC_wait_for_reclaim(void)
{
    GC_ASSERT(builder_cv != 0);
    for (;;)
    {
        GC_acquire_mark_lock();
        if (GC_fl_builder_count == 0)
            break;
        if (ResetEvent(builder_cv) == FALSE)
            ABORT("ResetEvent() failed");
        GC_release_mark_lock();
        if (WaitForSingleObject(builder_cv, INFINITE) == WAIT_FAILED)
            ABORT("WaitForSingleObject() failed");
    }
    GC_release_mark_lock();
}

GC_INNER void GC_notify_all_builder(void)
{
    GC_ASSERT(GC_mark_lock_holder == (unsigned long)GetCurrentThreadId());
    GC_ASSERT(builder_cv != 0);
    GC_ASSERT(GC_fl_builder_count == 0);
    if (SetEvent(builder_cv) == FALSE)
        ABORT("SetEvent() failed");
}

#ifdef DONT_USE_SIGNALANDWAIT

/* mark_cv is used (for waiting) by a non-helper thread.  */

GC_INNER void GC_wait_marker(void)
{
    HANDLE event = mark_cv;
    DWORD thread_id = GetCurrentThreadId();
    int i = (int)GC_markers - 1;
    while (i-- > 0)
    {
        if (GC_marker_Id[i] == thread_id)
        {
            event = GC_marker_cv[i];
            break;
        }
    }

    if (ResetEvent(event) == FALSE)
        ABORT("ResetEvent() failed");
    GC_release_mark_lock();
    if (WaitForSingleObject(event, INFINITE) == WAIT_FAILED)
        ABORT("WaitForSingleObject() failed");
    GC_acquire_mark_lock();
}

GC_INNER void GC_notify_all_marker(void)
{
    DWORD thread_id = GetCurrentThreadId();
    int i = (int)GC_markers - 1;
    while (i-- > 0)
    {
        /* Notify every marker ignoring self (for efficiency).  */
        if (SetEvent(GC_marker_Id[i] != thread_id ? GC_marker_cv[i] : mark_cv)
            == FALSE)
            ABORT("SetEvent() failed");
    }
}

#else /* DONT_USE_SIGNALANDWAIT */

/* For GC_wait_marker/GC_notify_all_marker() the above technique  */
/* does not work because they are used with different checked     */
/* conditions in different places (and, in addition, notifying is */
/* done after leaving critical section) and this could result in  */
/* a signal loosing between checking for a particular condition   */
/* and calling WaitForSingleObject.  So, we use PulseEvent() and  */
/* NT SignalObjectAndWait() (which atomically sets mutex event to */
/* signaled state and starts waiting on condvar).  A special      */
/* case here is GC_mark_mutex_waitcnt == 1 (i.e. nobody waits for */
/* mark lock at this moment) - we don't change it (otherwise we   */
/* may loose a signal sent between decrementing                   */
/* GC_mark_mutex_waitcnt and calling WaitForSingleObject()).      */

#ifdef MSWINCE
/* SignalObjectAndWait() is missing in WinCE (for now), so you  */
/* should supply its emulation (externally) to use this code.   */
WINBASEAPI DWORD WINAPI SignalObjectAndWait(HANDLE, HANDLE, DWORD, BOOL);
#define signalObjectAndWait_func SignalObjectAndWait
#else
typedef DWORD(WINAPI* SignalObjectAndWait_type)(HANDLE, HANDLE, DWORD, BOOL);
static SignalObjectAndWait_type signalObjectAndWait_func = 0;
#endif

GC_INNER void GC_wait_marker(void)
{
    /* Here we assume that GC_wait_marker() is always called        */
    /* from a while(check_cond) loop.                               */
    AO_t waitcnt;
    GC_ASSERT(mark_cv != 0);

    /* We inline GC_release_mark_lock() to have atomic              */
    /* unlock-and-wait action here.                                 */
    GC_ASSERT(GC_mark_lock_holder == (unsigned long)GetCurrentThreadId());
#ifdef GC_ASSERTIONS
    GC_mark_lock_holder = NO_THREAD;
#endif

    if ((waitcnt = AO_load(&GC_mark_mutex_waitcnt)) > 1)
    {
        (void)AO_fetch_and_sub1_release(&GC_mark_mutex_waitcnt);
    }
    else
    {
        GC_ASSERT(AO_load(&GC_mark_mutex_waitcnt) != 0);
    }

    /* The state of mark_cv is non-signaled here. */
    if (signalObjectAndWait_func(mark_mutex_event /* hObjectToSignal */,
                                 mark_cv /* hObjectToWaitOn */,
                                 INFINITE /* timeout */,
                                 FALSE /* isAlertable */)
        == WAIT_FAILED)
        ABORT("SignalObjectAndWait() failed");
    /* The state of mark_cv is non-signaled here again. */

    if (waitcnt > 1)
    {
        GC_acquire_mark_lock();
    }
    else
    {
        GC_ASSERT(GC_mark_mutex_waitcnt != 0);
        /* Acquire mark lock */
        if (WaitForSingleObject(mark_mutex_event, INFINITE) == WAIT_FAILED)
            ABORT("WaitForSingleObject() failed");
        GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#ifdef GC_ASSERTIONS
        GC_mark_lock_holder = (unsigned long)GetCurrentThreadId();
#endif
    }
}

GC_INNER void GC_notify_all_marker(void)
{
    GC_ASSERT(mark_cv != 0);
    if (PulseEvent(mark_cv) == FALSE)
        ABORT("PulseEvent() failed");
}

#endif /* !DONT_USE_SIGNALANDWAIT */

#endif /* ! GC_PTHREADS_PARAMARK */

#endif /* PARALLEL_MARK */

#ifndef GC_PTHREADS

/* We have no DllMain to take care of new threads.  Thus we   */
/* must properly intercept thread creation.                   */

typedef struct
{
    LPTHREAD_START_ROUTINE start;
    LPVOID param;
} thread_args;

STATIC void* GC_CALLBACK GC_win32_start_inner(struct GC_stack_base* sb,
                                              void* arg)
{
    void* ret;
    LPTHREAD_START_ROUTINE start = ((thread_args*)arg)->start;
    LPVOID param = ((thread_args*)arg)->param;

    GC_register_my_thread(sb); /* This waits for an in-progress GC.     */

#ifdef DEBUG_THREADS
    GC_log_printf("thread 0x%lx starting...\n", (long)GetCurrentThreadId());
#endif

    GC_free(arg);

    /* Clear the thread entry even if we exit with an exception.        */
    /* This is probably pointless, since an uncaught exception is       */
    /* supposed to result in the process being killed.                  */
#ifndef __GNUC__
    __try
#endif
    {
        ret = (void*)(word)(*start)(param);
    }
#ifndef __GNUC__
    __finally
#endif
    {
        GC_unregister_my_thread();
    }

#ifdef DEBUG_THREADS
    GC_log_printf("thread 0x%lx returned from start routine\n",
                  (long)GetCurrentThreadId());
#endif
    return ret;
}

STATIC DWORD WINAPI GC_win32_start(LPVOID arg)
{
    return (DWORD)(word)GC_call_with_stack_base(GC_win32_start_inner, arg);
}

GC_API HANDLE WINAPI GC_CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                     DWORD dwStackSize,
                                     LPTHREAD_START_ROUTINE lpStartAddress,
                                     LPVOID lpParameter, DWORD dwCreationFlags,
                                     LPDWORD lpThreadId)
{
    HANDLE thread_h;
    thread_args* args;

    if (!parallel_initialized)
        GC_init_parallel();
    /* make sure GC is initialized (i.e. main thread is     */
    /* attached, tls initialized).                          */

#ifdef DEBUG_THREADS
    GC_log_printf("About to create a thread from 0x%lx\n",
                  (long)GetCurrentThreadId());
#endif
    if (GC_win32_dll_threads)
    {
        return CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress,
                            lpParameter, dwCreationFlags, lpThreadId);
    }
    else
    {
        args = GC_malloc_uncollectable(sizeof(thread_args));
        /* Handed off to and deallocated by child thread.       */
        if (0 == args)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return NULL;
        }

        /* set up thread arguments */
        args->start = lpStartAddress;
        args->param = lpParameter;

        GC_need_to_lock = TRUE;
        thread_h = CreateThread(lpThreadAttributes, dwStackSize, GC_win32_start,
                                args, dwCreationFlags, lpThreadId);
        if (thread_h == 0)
            GC_free(args);
        return thread_h;
    }
}

GC_API DECLSPEC_NORETURN void WINAPI GC_ExitThread(DWORD dwExitCode)
{
    GC_unregister_my_thread();
    ExitThread(dwExitCode);
}

#ifndef MSWINCE

GC_API GC_uintptr_t GC_CALL
GC_beginthreadex(void* security, unsigned stack_size,
                 unsigned(__stdcall* start_address)(void*), void* arglist,
                 unsigned initflag, unsigned* thrdaddr)
{
    GC_uintptr_t thread_h;
    thread_args* args;

    if (!parallel_initialized)
        GC_init_parallel();
    /* make sure GC is initialized (i.e. main thread is     */
    /* attached, tls initialized).                          */
#ifdef DEBUG_THREADS
    GC_log_printf("About to create a thread from 0x%lx\n",
                  (long)GetCurrentThreadId());
#endif

    if (GC_win32_dll_threads)
    {
        return _beginthreadex(security, stack_size, start_address, arglist,
                              initflag, thrdaddr);
    }
    else
    {
        args = GC_malloc_uncollectable(sizeof(thread_args));
        /* Handed off to and deallocated by child thread.       */
        if (0 == args)
        {
            /* MSDN docs say _beginthreadex() returns 0 on error and sets */
            /* errno to either EAGAIN (too many threads) or EINVAL (the   */
            /* argument is invalid or the stack size is incorrect), so we */
            /* set errno to EAGAIN on "not enough memory".                */
            errno = EAGAIN;
            return 0;
        }

        /* set up thread arguments */
        args->start = (LPTHREAD_START_ROUTINE)start_address;
        args->param = arglist;

        GC_need_to_lock = TRUE;
        thread_h = _beginthreadex(security, stack_size,
                                  (unsigned(__stdcall*)(void*))GC_win32_start,
                                  args, initflag, thrdaddr);
        if (thread_h == 0)
            GC_free(args);
        return thread_h;
    }
}

GC_API void GC_CALL GC_endthreadex(unsigned retval)
{
    GC_unregister_my_thread();
    _endthreadex(retval);
}

#endif /* !MSWINCE */

#endif /* !GC_PTHREADS */

#ifdef GC_WINMAIN_REDIRECT
/* This might be useful on WinCE.  Shouldn't be used with GC_DLL.     */

#if defined(MSWINCE) && defined(UNDER_CE)
#define WINMAIN_LPTSTR LPWSTR
#else
#define WINMAIN_LPTSTR LPSTR
#endif

/* This is defined in gc.h.   */
#undef WinMain

/* Defined outside GC by an application.      */
int WINAPI GC_WinMain(HINSTANCE, HINSTANCE, WINMAIN_LPTSTR, int);

typedef struct
{
    HINSTANCE hInstance;
    HINSTANCE hPrevInstance;
    WINMAIN_LPTSTR lpCmdLine;
    int nShowCmd;
} main_thread_args;

static DWORD WINAPI main_thread_start(LPVOID arg)
{
    main_thread_args* args = (main_thread_args*)arg;
    return (DWORD)GC_WinMain(args->hInstance, args->hPrevInstance,
                             args->lpCmdLine, args->nShowCmd);
}

STATIC void* GC_waitForSingleObjectInfinite(void* handle)
{
    return (void*)(word)WaitForSingleObject((HANDLE)handle, INFINITE);
}

#ifndef WINMAIN_THREAD_STACK_SIZE
#define WINMAIN_THREAD_STACK_SIZE 0 /* default value */
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   WINMAIN_LPTSTR lpCmdLine, int nShowCmd)
{
    DWORD exit_code = 1;

    main_thread_args args = {hInstance, hPrevInstance, lpCmdLine, nShowCmd};
    HANDLE thread_h;
    DWORD thread_id;

    /* initialize everything */
    GC_INIT();

    /* start the main thread */
    thread_h = GC_CreateThread(
        NULL /* lpsa */, WINMAIN_THREAD_STACK_SIZE /* ignored on WinCE */,
        main_thread_start, &args, 0 /* fdwCreate */, &thread_id);

    if (thread_h != NULL)
    {
        if ((DWORD)(word)GC_do_blocking(GC_waitForSingleObjectInfinite,
                                        (void*)thread_h)
            == WAIT_FAILED)
            ABORT("WaitForSingleObject(main_thread) failed");
        GetExitCodeThread(thread_h, &exit_code);
        CloseHandle(thread_h);
    }
    else
    {
        ABORT("GC_CreateThread(main_thread) failed");
    }

#ifdef MSWINCE
    GC_deinit();
    DeleteCriticalSection(&GC_allocate_ml);
#endif
    return (int)exit_code;
}

#endif /* GC_WINMAIN_REDIRECT */

/* Called by GC_init() - we hold the allocation lock.   */
GC_INNER void GC_thr_init(void)
{
    struct GC_stack_base sb;
#ifdef GC_ASSERTIONS
    int sb_result;
#endif

    GC_ASSERT(I_HOLD_LOCK());
    if (GC_thr_initialized)
        return;
    GC_main_thread = GetCurrentThreadId();
    GC_thr_initialized = TRUE;

#ifdef CAN_HANDLE_FORK
    /* Prepare for forks if requested.  */
    if (GC_handle_fork
        && pthread_atfork(GC_fork_prepare_proc, GC_fork_parent_proc,
                          GC_fork_child_proc)
               != 0)
        ABORT("pthread_atfork failed");
#endif

    /* Add the initial thread, so we can stop it. */
#ifdef GC_ASSERTIONS
    sb_result =
#endif
        GC_get_stack_base(&sb);
    GC_ASSERT(sb_result == GC_SUCCESS);

#if defined(PARALLEL_MARK)
    /* Set GC_markers. */
    {
        char* markers_string = GETENV("GC_MARKERS");
        if (markers_string != NULL)
        {
            GC_markers = atoi(markers_string);
            if (GC_markers > MAX_MARKERS)
            {
                WARN("Limiting number of mark threads\n", 0);
                GC_markers = MAX_MARKERS;
            }
        }
        else
        {
#ifdef MSWINCE
            /* There is no GetProcessAffinityMask() in WinCE.     */
            /* GC_sysinfo is already initialized.                 */
            GC_markers = GC_sysinfo.dwNumberOfProcessors;
#else
#ifdef _WIN64
            DWORD_PTR procMask = 0;
            DWORD_PTR sysMask;
#else
            DWORD procMask = 0;
            DWORD sysMask;
#endif
            int ncpu = 0;
            if (GetProcessAffinityMask(GetCurrentProcess(), (void*)&procMask,
                                       (void*)&sysMask)
                && procMask)
            {
                do
                {
                    ncpu++;
                } while ((procMask &= procMask - 1) != 0);
            }
            GC_markers = ncpu;
#endif
#ifdef GC_MIN_MARKERS
            /* This is primarily for testing on systems without getenv(). */
            if (GC_markers < GC_MIN_MARKERS)
                GC_markers = GC_MIN_MARKERS;
#endif
            if (GC_markers >= MAX_MARKERS)
                GC_markers = MAX_MARKERS; /* silently limit GC_markers value  */
        }
    }

    /* Set GC_parallel. */
    {
#if !defined(GC_PTHREADS_PARAMARK) && !defined(MSWINCE) \
    && !defined(DONT_USE_SIGNALANDWAIT)
        HMODULE hK32;
        /* SignalObjectAndWait() API call works only under NT.          */
#endif
        if (GC_win32_dll_threads || GC_markers <= 1
#if !defined(GC_PTHREADS_PARAMARK) && !defined(MSWINCE) \
    && !defined(DONT_USE_SIGNALANDWAIT)
            || GC_wnt == FALSE
            || (hK32 = GetModuleHandle(TEXT("kernel32.dll"))) == (HMODULE)0
            || (signalObjectAndWait_func = (SignalObjectAndWait_type)
                    GetProcAddress(hK32, "SignalObjectAndWait"))
                   == 0
#endif
        )
        {
            /* Disable parallel marking. */
            GC_parallel = FALSE;
            GC_markers = 1;
        }
        else
        {
#ifndef GC_PTHREADS_PARAMARK
            /* Initialize Win32 event objects for parallel marking.       */
            mark_mutex_event =
                CreateEvent(NULL /* attrs */, FALSE /* isManualReset */,
                            FALSE /* initialState */, NULL /* name */);
            builder_cv = CreateEvent(NULL /* attrs */, TRUE /* isManualReset */,
                                     FALSE /* initialState */, NULL /* name */);
            mark_cv = CreateEvent(NULL /* attrs */, TRUE /* isManualReset */,
                                  FALSE /* initialState */, NULL /* name */);
            if (mark_mutex_event == (HANDLE)0 || builder_cv == (HANDLE)0
                || mark_cv == (HANDLE)0)
                ABORT("CreateEvent() failed");
#endif
            GC_parallel = TRUE;
            /* Disable true incremental collection, but generational is OK. */
            GC_time_limit = GC_TIME_UNLIMITED;
        }
    }
#endif /* PARALLEL_MARK */

    GC_ASSERT(0 == GC_lookup_thread_inner(GC_main_thread));
    GC_register_my_thread_inner(&sb, GC_main_thread);

#ifdef PARALLEL_MARK
    /* If we are using a parallel marker, actually start helper threads. */
    if (GC_parallel)
        start_mark_threads();
    if (GC_print_stats)
    {
        GC_log_printf("Started %ld mark helper threads\n", GC_markers - 1);
    }
#endif
}

#ifdef GC_PTHREADS

struct start_info
{
    void* (*start_routine)(void*);
    void* arg;
    GC_bool detached;
};

GC_API int GC_pthread_join(pthread_t pthread_id, void** retval)
{
    int result;
    GC_thread t;

#ifdef DEBUG_THREADS
    GC_log_printf("thread %p(0x%lx) is joining thread %p\n",
                  GC_PTHREAD_PTRVAL(pthread_self()), (long)GetCurrentThreadId(),
                  GC_PTHREAD_PTRVAL(pthread_id));
#endif

    if (!parallel_initialized)
        GC_init_parallel();

    /* Thread being joined might not have registered itself yet. */
    /* After the join,thread id may have been recycled.          */
    /* FIXME: It would be better if this worked more like        */
    /* pthread_support.c.                                        */
#ifndef GC_WIN32_PTHREADS
    while ((t = GC_lookup_pthread(pthread_id)) == 0)
        Sleep(10);
#endif

    result = pthread_join(pthread_id, retval);

#ifdef GC_WIN32_PTHREADS
    /* win32_pthreads id are unique */
    t = GC_lookup_pthread(pthread_id);
#endif

    if (!GC_win32_dll_threads)
    {
        DCL_LOCK_STATE;

        LOCK();
        GC_delete_gc_thread(t);
        UNLOCK();
    } /* otherwise DllMain handles it.  */

#ifdef DEBUG_THREADS
    GC_log_printf("thread %p(0x%lx) completed join with thread %p\n",
                  GC_PTHREAD_PTRVAL(pthread_self()), (long)GetCurrentThreadId(),
                  GC_PTHREAD_PTRVAL(pthread_id));
#endif
    return result;
}

/* Cygwin-pthreads calls CreateThread internally, but it's not easily */
/* interceptible by us..., so intercept pthread_create instead.       */
GC_API int GC_pthread_create(pthread_t* new_thread,
                             GC_PTHREAD_CREATE_CONST pthread_attr_t* attr,
                             void* (*start_routine)(void*), void* arg)
{
    if (!parallel_initialized)
        GC_init_parallel();
    /* make sure GC is initialized (i.e. main thread is attached) */
    if (GC_win32_dll_threads)
    {
        return pthread_create(new_thread, attr, start_routine, arg);
    }
    else
    {
        int result;
        struct start_info* si;

        /* This is otherwise saved only in an area mmapped by the thread  */
        /* library, which isn't visible to the collector.                 */
        si = GC_malloc_uncollectable(sizeof(struct start_info));
        if (0 == si)
            return (EAGAIN);

        si->start_routine = start_routine;
        si->arg = arg;
        if (attr != 0
            && pthread_attr_getdetachstate(attr, &si->detached)
                   == PTHREAD_CREATE_DETACHED)
        {
            si->detached = TRUE;
        }

#ifdef DEBUG_THREADS
        GC_log_printf("About to create a thread from %p(0x%lx)\n",
                      GC_PTHREAD_PTRVAL(pthread_self()),
                      (long)GetCurrentThreadId());
#endif
        GC_need_to_lock = TRUE;
        result = pthread_create(new_thread, attr, GC_pthread_start, si);

        if (result)
        { /* failure */
            GC_free(si);
        }
        return (result);
    }
}

STATIC void* GC_CALLBACK GC_pthread_start_inner(struct GC_stack_base* sb,
                                                void* arg)
{
    struct start_info* si = arg;
    void* result;
    void* (*start)(void*);
    void* start_arg;
    DWORD thread_id = GetCurrentThreadId();
    pthread_t pthread_id = pthread_self();
    GC_thread me;
    DCL_LOCK_STATE;

#ifdef DEBUG_THREADS
    GC_log_printf("thread %p(0x%x) starting...\n",
                  GC_PTHREAD_PTRVAL(pthread_id), (int)thread_id);
#endif

    GC_ASSERT(!GC_win32_dll_threads);
    /* If a GC occurs before the thread is registered, that GC will     */
    /* ignore this thread.  That's fine, since it will block trying to  */
    /* acquire the allocation lock, and won't yet hold interesting      */
    /* pointers.                                                        */
    LOCK();
    /* We register the thread here instead of in the parent, so that    */
    /* we don't need to hold the allocation lock during pthread_create. */
    me = GC_register_my_thread_inner(sb, thread_id);
    SET_PTHREAD_MAP_CACHE(pthread_id, thread_id);
    me->pthread_id = pthread_id;
    if (si->detached)
        me->flags |= DETACHED;
    UNLOCK();

    start = si->start_routine;
    start_arg = si->arg;

    GC_free(si); /* was allocated uncollectable */

    pthread_cleanup_push(GC_thread_exit_proc, (void*)me);
    result = (*start)(start_arg);
    me->status = result;
    pthread_cleanup_pop(1);

#ifdef DEBUG_THREADS
    GC_log_printf("thread %p(0x%x) returned from start routine\n",
                  GC_PTHREAD_PTRVAL(pthread_id), (int)thread_id);
#endif
    return (result);
}

STATIC void* GC_pthread_start(void* arg)
{
    return GC_call_with_stack_base(GC_pthread_start_inner, arg);
}

STATIC void GC_thread_exit_proc(void* arg)
{
    GC_thread me = (GC_thread)arg;
    DCL_LOCK_STATE;

    GC_ASSERT(!GC_win32_dll_threads);
#ifdef DEBUG_THREADS
    GC_log_printf("thread %p(0x%lx) called pthread_exit()\n",
                  GC_PTHREAD_PTRVAL(pthread_self()),
                  (long)GetCurrentThreadId());
#endif

    LOCK();
    GC_wait_for_gc_completion(FALSE);
#if defined(THREAD_LOCAL_ALLOC)
    GC_destroy_thread_local(&(me->tlfs));
#endif
    if (me->flags & DETACHED)
    {
        GC_delete_thread(GetCurrentThreadId());
    }
    else
    {
        /* deallocate it as part of join */
        me->flags |= FINISHED;
    }
    UNLOCK();
}

#ifndef GC_NO_PTHREAD_SIGMASK
/* Win32 pthread does not support sigmask.  */
/* So, nothing required here...             */
GC_API int GC_pthread_sigmask(int how, const sigset_t* set, sigset_t* oset)
{
    if (!parallel_initialized)
        GC_init_parallel();
    return pthread_sigmask(how, set, oset);
}
#endif /* !GC_NO_PTHREAD_SIGMASK */

GC_API int GC_pthread_detach(pthread_t thread)
{
    int result;
    GC_thread t;
    DCL_LOCK_STATE;

    if (!parallel_initialized)
        GC_init_parallel();
    LOCK();
    t = GC_lookup_pthread(thread);
    UNLOCK();
    result = pthread_detach(thread);
    if (result == 0)
    {
        LOCK();
        t->flags |= DETACHED;
        /* Here the pthread thread id may have been recycled. */
        if ((t->flags & FINISHED) != 0)
        {
            GC_delete_gc_thread(t);
        }
        UNLOCK();
    }
    return result;
}

#else /* !GC_PTHREADS */

#ifndef GC_NO_THREADS_DISCOVERY
/* We avoid acquiring locks here, since this doesn't seem to be     */
/* preemptible.  This may run with an uninitialized collector, in   */
/* which case we don't do much.  This implies that no threads other */
/* than the main one should be created with an uninitialized        */
/* collector.  (The alternative of initializing the collector here  */
/* seems dangerous, since DllMain is limited in what it can do.)    */

/*ARGSUSED*/
BOOL WINAPI DllMain(HINSTANCE inst, ULONG reason, LPVOID reserved)
{
    struct GC_stack_base sb;
    DWORD thread_id;
#ifdef GC_ASSERTIONS
    int sb_result;
#endif
    static int entry_count = 0;

    if (!GC_win32_dll_threads && parallel_initialized)
        return TRUE;

    switch (reason)
    {
    case DLL_THREAD_ATTACH:
#ifdef PARALLEL_MARK
        /* Don't register marker threads. */
        if (GC_parallel)
        {
            /* We could reach here only if parallel_initialized == FALSE. */
            break;
        }
#endif
        GC_ASSERT(entry_count == 0 || parallel_initialized);
        ++entry_count; /* and fall through: */
    case DLL_PROCESS_ATTACH:
        /* This may run with the collector uninitialized. */
        thread_id = GetCurrentThreadId();
        if (parallel_initialized && GC_main_thread != thread_id)
        {
            /* Don't lock here.   */
#ifdef GC_ASSERTIONS
            sb_result =
#endif
                GC_get_stack_base(&sb);
            GC_ASSERT(sb_result == GC_SUCCESS);
#if defined(THREAD_LOCAL_ALLOC) || defined(PARALLEL_MARK)
            ABORT("Cannot initialize thread local cache from DllMain");
#endif
            GC_register_my_thread_inner(&sb, thread_id);
        } /* o.w. we already did it during GC_thr_init, called by GC_init */
        break;

    case DLL_THREAD_DETACH:
        /* We are hopefully running in the context of the exiting thread. */
        GC_ASSERT(parallel_initialized);
        if (GC_win32_dll_threads)
        {
            GC_delete_thread(GetCurrentThreadId());
        }
        break;

    case DLL_PROCESS_DETACH:
        if (GC_win32_dll_threads)
        {
            int i;
            int my_max = (int)GC_get_max_thread_index();

            for (i = 0; i <= my_max; ++i)
            {
                if (AO_load(&(dll_thread_table[i].tm.in_use)))
                    GC_delete_gc_thread(dll_thread_table + i);
            }
            GC_deinit();
            DeleteCriticalSection(&GC_allocate_ml);
        }
        break;
    }
    return TRUE;
}
#endif /* !GC_NO_THREADS_DISCOVERY */

#endif /* !GC_PTHREADS */

/* Perform all initializations, including those that    */
/* may require allocation.                              */
/* Called without allocation lock.                      */
/* Must be called before a second thread is created.    */
GC_INNER void GC_init_parallel(void)
{
#if defined(THREAD_LOCAL_ALLOC)
    GC_thread me;
    DCL_LOCK_STATE;
#endif

    if (parallel_initialized)
        return;
    parallel_initialized = TRUE;
    /* GC_init() calls us back, so set flag first.      */

    if (!GC_is_initialized)
        GC_init();
    if (GC_win32_dll_threads)
    {
        GC_need_to_lock = TRUE;
        /* Cannot intercept thread creation.  Hence we don't know if    */
        /* other threads exist.  However, client is not allowed to      */
        /* create other threads before collector initialization.        */
        /* Thus it's OK not to lock before this.                        */
    }
    /* Initialize thread local free lists if used.        */
#if defined(THREAD_LOCAL_ALLOC)
    LOCK();
    me = GC_lookup_thread_inner(GetCurrentThreadId());
    CHECK_LOOKUP_MY_THREAD(me);
    GC_init_thread_local(&me->tlfs);
    UNLOCK();
#endif
}

#if defined(USE_PTHREAD_LOCKS)
/* Support for pthread locking code.          */
/* Pthread_mutex_try_lock may not win here,   */
/* due to builtin support for spinning first? */

GC_INNER volatile GC_bool GC_collecting = 0;

/* A hint that we're in the collector and       */
/* holding the allocation lock for an           */
/* extended period.                             */

GC_INNER void GC_lock(void) { pthread_mutex_lock(&GC_allocate_ml); }
#endif /* USE_PTHREAD_LOCKS */

#if defined(THREAD_LOCAL_ALLOC)

/* Add thread-local allocation support.  VC++ uses __declspec(thread).  */

/* We must explicitly mark ptrfree and gcj free lists, since the free   */
/* list links wouldn't otherwise be found.  We also set them in the     */
/* normal free lists, since that involves touching less memory than if  */
/* we scanned them normally.                                            */
GC_INNER void GC_mark_thread_local_free_lists(void)
{
    int i;
    GC_thread p;

    for (i = 0; i < THREAD_TABLE_SZ; ++i)
    {
        for (p = GC_threads[i]; 0 != p; p = p->tm.next)
        {
            if (!KNOWN_FINISHED(p))
            {
#ifdef DEBUG_THREADS
                GC_log_printf("Marking thread locals for 0x%x\n", (int)p->id);
#endif
                GC_mark_thread_local_fls_for(&(p->tlfs));
            }
        }
    }
}

#if defined(GC_ASSERTIONS)
void GC_check_tls_for(GC_tlfs p);
#if defined(USE_CUSTOM_SPECIFIC)
void GC_check_tsd_marks(tsd* key);
#endif
/* Check that all thread-local free-lists are completely marked.    */
/* also check that thread-specific-data structures are marked.      */
void GC_check_tls(void)
{
    int i;
    GC_thread p;

    for (i = 0; i < THREAD_TABLE_SZ; ++i)
    {
        for (p = GC_threads[i]; 0 != p; p = p->tm.next)
        {
            if (!KNOWN_FINISHED(p))
                GC_check_tls_for(&(p->tlfs));
        }
    }
#if defined(USE_CUSTOM_SPECIFIC)
    if (GC_thread_key != 0)
        GC_check_tsd_marks(GC_thread_key);
#endif
}
#endif /* GC_ASSERTIONS */

#endif /* THREAD_LOCAL_ALLOC ... */

#ifndef GC_NO_THREAD_REDIRECTS
/* Restore thread calls redirection.        */
#define CreateThread GC_CreateThread
#define ExitThread GC_ExitThread
#undef _beginthreadex
#define _beginthreadex GC_beginthreadex
#undef _endthreadex
#define _endthreadex GC_endthreadex
#endif /* !GC_NO_THREAD_REDIRECTS */

#endif /* GC_WIN32_THREADS */
