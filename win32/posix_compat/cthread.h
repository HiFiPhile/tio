/**
******************************************************************************
* @file    cthread.h
* @author  john@nachtimwald.com
*          Zixun LI
* @version V1.0
* @date    28/01/2022
* @brief   pthread wrapper
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CPTHREAD_H__
#define __CPTHREAD_H__
/* Includes ------------------------------------------------------------------*/
#ifdef _WIN32
# include <stdbool.h>
# include <windows.h>
# include <time.h>
#else
# include <pthread.h>
#endif
/* Exported defines --------------------------------------------------------- */
/* Exported types ------------------------------------------------------------*/
#ifdef _WIN32
typedef CRITICAL_SECTION pthread_mutex_t;
typedef void pthread_mutexattr_t;
typedef void pthread_condattr_t;
typedef void pthread_rwlockattr_t;
typedef void pthread_attr_t;
typedef HANDLE pthread_t;
typedef CONDITION_VARIABLE pthread_cond_t;

#endif
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
#ifdef _WIN32
int pthread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **value_ptr);
int pthread_detach(pthread_t);
void pthread_exit (void *res);

int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

#endif

void ms_to_timespec(struct timespec *ts, unsigned int ms);
int timespec_to_ms(const struct timespec *ts);

#endif /* __CPTHREAD_H__ */