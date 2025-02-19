/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/* lwIP includes. */
#include "s907x.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"


 
#define LWIP_THREAD_TLS 0

struct sys_timeouts {
  struct sys_timeo *next;
};


struct timeoutlist
{
	struct sys_timeouts timeouts;
	thread_hdl_t pid;
};

/* This is the number of threads that can be started with sys_thread_new() */
#define SYS_THREAD_MAX 6

static struct timeoutlist s_timeoutlist[SYS_THREAD_MAX];
static u16_t s_nextthread = 0;
 

/*-----------------------------------------------------------------------------------*/
//  Creates an empty mailbox.
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	(void ) size;
    
#ifndef S907X_AOS
	queue_t queue;

	wl_init_queue(&queue,  sizeof( void * ), size );

	*mbox = queue;
#else
    wl_init_queue((sys_mbox_t*)mbox,  sizeof( void * ), size );

#endif

#if SYS_STATS
      ++lwip_stats.sys.mbox.used;
      if (lwip_stats.sys.mbox.max < lwip_stats.sys.mbox.used) {
         lwip_stats.sys.mbox.max = lwip_stats.sys.mbox.used;
	  }
#endif /* SYS_STATS */

#ifndef S907X_AOS
    if (*mbox == NULL)
        return ERR_MEM;
#else
    if (!mbox || mbox->hdl == NULL)
        return ERR_MEM;    
#endif

    return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/*
  Deallocates a mailbox. If there are messages still present in the
  mailbox when the mailbox is deallocated, it is an indication of a
  programming error in lwIP and the developer should be notified.
*/
void sys_mbox_free(sys_mbox_t *mbox)
{
	int ret;

	ret = wl_free_queue(mbox);
	if(ret) {
#if SYS_STATS
	    lwip_stats.sys.mbox.err++;
#endif /* SYS_STATS */
	}
#if SYS_STATS
     --lwip_stats.sys.mbox.used;
#endif /* SYS_STATS */
}

/*-----------------------------------------------------------------------------------*/
//   Posts the "msg" to the mailbox.
void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
	while ( wl_send_queue(mbox, (void*)&msg, portMAX_DELAY ) != TRUE ){}

}/* For internal use only. */

/*-----------------------------------------------------------------------------------*/
//   Try to post the "msg" to the mailbox.
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
err_t result;

   if ( wl_send_queue( mbox, &msg, 0 ) == TRUE )
   {
      result = ERR_OK;
   }
   else {
      // could not post, queue must be full
      result = ERR_MEM;
			
#if SYS_STATS
      lwip_stats.sys.mbox.err++;
#endif /* SYS_STATS */
			
   }

   return result;
}

/*-----------------------------------------------------------------------------------*/
/*
  Blocks the thread until a message arrives in the mailbox, but does
  not block the thread longer than "timeout" milliseconds (similar to
  the sys_arch_sem_wait() function). The "msg" argument is a result
  parameter that is set by the function (i.e., by doing "*msg =
  ptr"). The "msg" parameter maybe NULL to indicate that the message
  should be dropped.

  The return values are the same as for the sys_arch_sem_wait() function:
  Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
  timeout.

  Note that a function with a similar name, sys_mbox_fetch(), is
  implemented by lwIP.
*/
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
	void *ptr;
	u32 start;
    u32 message;

	start = wl_get_systemtick();

	if ( msg == NULL )
	{
		msg = &ptr;
	}
		
	if ( timeout != 0 )
	{
		if ( TRUE == wl_wait_queue( mbox, (void*)&message, timeout ) )
		{
            *msg = (void*)message;
			return ( wl_systemtick_to_ms(wl_get_systemtick() - start) );
		}
		else // timed out blocking for message
		{
			*msg = NULL;
			
			return SYS_ARCH_TIMEOUT;
		}
	}
	else // block forever for a message.
	{
		while( TRUE != wl_wait_queue( mbox, (void*)&message, portMAX_DELAY ) ){} // time is arbitrary wl_wait_queue
		*msg = (void*)message;	
		return ( wl_systemtick_to_ms(wl_get_systemtick() - start) );
		
	}
}

/*-----------------------------------------------------------------------------------*/
/*
  Similar to sys_arch_mbox_fetch, but if message is not ready immediately, we'll
  return with SYS_MBOX_EMPTY.  On success, 0 is returned.
*/
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	void *ptr;
    u32 message;

	if ( msg == NULL )
	{
		msg = &ptr;
	}

   if ( TRUE == wl_wait_queue( mbox, (void*)&message, 0 ) )
   {
      *msg = (void*)message;	
      return ERR_OK;
   }
   else
   {
      return SYS_MBOX_EMPTY;
   }
}


/*-----------------------------------------------------------------------------------*/
//  Creates a new semaphore. The "count" argument specifies
//  the initial state of the semaphore.
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	sys_sem_t sema;
 	
	wl_init_sema(&sema, count, sema_binary);
	*sem = sema;
	if(!sys_sem_valid(sem))
	{
		
#if SYS_STATS
      ++lwip_stats.sys.sem.err;
#endif /* SYS_STATS */	
		return ERR_MEM;
	}

#if SYS_STATS
	++lwip_stats.sys.sem.used;
 	if (lwip_stats.sys.sem.max < lwip_stats.sys.sem.used) {
		lwip_stats.sys.sem.max = lwip_stats.sys.sem.used;
	}
#endif /* SYS_STATS */
	
	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/*
  Blocks the thread while waiting for the semaphore to be
  signaled. If the "timeout" argument is non-zero, the thread should
  only be blocked for the specified time (measured in
  milliseconds).

  If the timeout argument is non-zero, the return value is the number of
  milliseconds spent waiting for the semaphore to be signaled. If the
  semaphore wasn't signaled within the specified time, the return value is
  SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
  (i.e., it was already signaled), the function may return zero.

  Notice that lwIP implements a function with a similar name,
  sys_sem_wait(), that uses the sys_arch_sem_wait() function.
*/
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	u32 start;

	start = wl_get_systemtick();

	if(	timeout != 0)
	{
		if( wl_wait_sema( sem, timeout ) == TRUE )
		{
			return ( wl_systemtick_to_ms(wl_get_systemtick() - start) );
		}
		else
		{
			return SYS_ARCH_TIMEOUT;
		}
	}
	else // must block without a timeout
	{
		while( wl_wait_sema(sem, portMAX_DELAY) != TRUE){}

		return ( wl_systemtick_to_ms(wl_get_systemtick() - start) );
		
	}
}

/*-----------------------------------------------------------------------------------*/
// Signals a semaphore
void sys_sem_signal(sys_sem_t *sem)
{
	wl_send_sema(sem);
}

/*-----------------------------------------------------------------------------------*/
// Deallocates a semaphore
void sys_sem_free(sys_sem_t *sem)
{

#if SYS_STATS
      --lwip_stats.sys.sem.used;
#endif /* SYS_STATS */

	wl_free_sema(sem);
		
}


/*-----------------------------------------------------------------------------------*/
// Initialize sys arch
void sys_init(void)
{
	int i;

	// Initialize the the per-thread sys_timeouts structures
	// make sure there are no valid pids in the list
	for(i = 0; i < SYS_THREAD_MAX; i++)
	{
		s_timeoutlist[i].pid = 0;
		s_timeoutlist[i].timeouts.next = NULL;
	}

	// keep track of how many threads have been created
	s_nextthread = 0;
}


/*-----------------------------------------------------------------------------------*/
// TODO
/*-----------------------------------------------------------------------------------*/
/*
  Starts a new thread with priority "prio" that will begin its execution in the
  function "thread()". The "arg" argument will be passed as an argument to the
  thread() function. The id of the new thread is returned. Both the id and
  the priority are system dependent.
*/
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread , void *arg, int stacksize, int prio)
{
   thread_hdl_t CreatedTask;

   if ( s_nextthread < SYS_THREAD_MAX )
   {
	   CreatedTask = wl_create_thread(  ( const char * ) name, stacksize,  prio, thread, arg);

	   // For each task created, store the task handle (pid) in the timers array.
	   // This scheme doesn't allow for threads to be deleted
	   wl_enter_critical();
	   s_timeoutlist[s_nextthread++].pid = CreatedTask;
       wl_exit_critical();

#ifndef S907X_AOS
	   return CreatedTask;
#else
       return CreatedTask.hdl;
#endif
	
   }
   else
   {
      return NULL;
   }
}
 
#if 0
/** Create a new mutex
 * @param mutex pointer to the mutex to create
 * @return a new mutex
 *
 **/
err_t sys_mutex_new(sys_mutex_t *mutex)
{
    err_t ret = ERR_MEM;

    wl_init_mutex(mutex);

    ret = ERR_OK;
}

/** Lock a mutex
 * @param mutex the mutex to lock
 **/
void sys_mutex_lock(sys_mutex_t *mutex)
{
    wl_lock_mutex(mutex);
}

/** Unlock a mutex
 * @param mutex the mutex to unlock */
void sys_mutex_unlock(sys_mutex_t *mutex)
{
    wl_unlock_mutex(mutex);
}


/** Delete a semaphore
 * @param mutex the mutex to delete
 **/
void sys_mutex_free(sys_mutex_t *mutex)
{
    if(mutex)
        wl_free_mutex(mutex);
}

#endif

err_t sys_mutex_trylock(sys_mutex_t *pxMutex)
{
	if (xSemaphoreTake(*pxMutex, 0) == pdPASS) return 0;
	else return -1;
}


void
sys_arch_msleep(int ms)
{
	vTaskDelay(ms / portTICK_RATE_MS);
}



int sys_thread_delete(thread_hdl_t pid)
{
	int i, isFind = 0;
	struct timeoutlist *tl, *tend = NULL;

	pid = (( !IS_SEMA_INIT(pid))? wl_currrnet_threadid() : pid);

	if (s_nextthread)
	{
		wl_enter_critical();

		tend = &(s_timeoutlist[s_nextthread-1]);//the last one
		for(i = 0; i < s_nextthread; i++)
		{
			tl = &(s_timeoutlist[i]);
#ifndef S907X_AOS
			if(tl->pid == pid)
#else
            if(tl->pid.hdl == pid.hdl)
#endif
			{//find the task, exchange with the last one
				memcpy(tl, tend, sizeof(struct timeoutlist));
				memset(tend, 0, sizeof(struct timeoutlist));
				s_nextthread --;
				isFind = 1;
				break;
			}
		}

		if (isFind) {
			wl_destory_thread( pid);
		}

		wl_exit_critical();
	   
		if (isFind)
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}
}

/*
  This optional function does a "fast" critical region protection and returns
  the previous protection level. This function is only called during very short
  critical regions. An embedded system which supports ISR-based drivers might
  want to implement this function by disabling interrupts. Task-based systems
  might want to implement this by using a mutex or disabling tasking. This
  function should support recursive calls from the same task or interrupt. In
  other words, sys_arch_protect() could be called while already protected. In
  that case the return value indicates that it is already protected.

  sys_arch_protect() is only required if your port is supporting an operating
  system.
*/
sys_prot_t sys_arch_protect(void)
{
	wl_enter_critical();
	return 1;
}

/*
  This optional function does a "fast" set of critical region protection to the
  value specified by pval. See the documentation for sys_arch_protect() for
  more information. This function is only required if your port is supporting
  an operating system.
*/
void sys_arch_unprotect(sys_prot_t pval)
{
	( void ) pval;
	wl_exit_critical();
}

/*
 * Prints an assertion messages and aborts execution.
 */
void sys_assert( const char *msg )
{	
	( void ) msg;
	/*FSL:only needed for debugging
	printf(msg);
	printf("\n\r");
	*/
    wl_enter_critical(  );
    while(1);

}

u32_t sys_now(void)
{
	return wl_get_systemtick();
}

u32_t sys_jiffies(void)
{
	return wl_get_systemtick();
}

#if LWIP_NETCONN_SEM_PER_THREAD

static void sys_thread_sem_free(int index, void *data) // destructor for TLS semaphore
{
    sys_sem_t *sem = (sys_sem_t *)(data);

    if (sem && *sem){
        LWIP_DEBUGF(LWIP_DBG_ON, ("sem del, sem=%p\n", *sem));
        wl_free_sema(sem);
    }

    if (sem) {
        LWIP_DEBUGF(LWIP_DBG_ON, ("sem pointer del, sem_p=%p\n", sem));
        free(sem);
    }
}

/*
 * get per thread semphore
 */
sys_sem_t* sys_thread_sem_init(void)
{
  sys_sem_t *sem = (sys_sem_t*)mem_malloc(sizeof(sys_sem_t*));

    if (!sem){
        LWIP_DEBUGF(LWIP_DBG_ON, "thread_sem_init: out of memory\n");
        return 0;
    }

    wl_init_sema(sem, 0, sema_binary);
    if (!(*sem)){
        free(sem);
        LWIP_DEBUGF(LWIP_DBG_ON, "thread_sem_init: out of memory\n");
        return 0;
    }

    vTaskSetThreadLocalStoragePointer(NULL, LWIP_THREAD_TLS, sem);

    return sem;
}

/*
 * get per thread semphore
 */
sys_sem_t* sys_thread_sem_get(void)
{
    sys_sem_t *sem = (sys_sem_t *)pvTaskGetThreadLocalStoragePointer(NULL, LWIP_THREAD_TLS);

    if (!sem) {
        sem = sys_thread_sem_init();
    }
    LWIP_DEBUGF(LWIP_DBG_ON, ("sem_get s=%p\n", sem));

    return sem;
}

void sys_thread_sem_deinit(void)
{
    sys_sem_t *sem = (sys_sem_t *)pvTaskGetThreadLocalStoragePointer(NULL, LWIP_THREAD_TLS);
    if (sem != NULL) {
        sys_thread_sem_free(LWIP_THREAD_TLS, sem);
        vTaskSetThreadLocalStoragePointer(NULL, LWIP_THREAD_TLS, NULL);
    }
}

#endif
