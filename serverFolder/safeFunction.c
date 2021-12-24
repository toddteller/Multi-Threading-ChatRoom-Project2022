#include "safeFunction.h"


/*=======================================================================*\
                            FUNZIONI SAFE SIGNAL
\*=======================================================================*/   
void safe_signal(int signum, __sighandler_t handler, const char *errorMsg)
{
    __sighandler_t checkerror = 0;
    checkerror = signal(signum, handler);
    if(checkerror == SIG_ERR)
        perror(errorMsg),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

/*=======================================================================*\
                     FUNZIONI SAFE FILE DESCRIPTORS
\*=======================================================================*/   
void safe_close(int fd, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = close(fd);
    if(checkerror == -1)
        perror(errorMsg),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}


/*=======================================================================*\
                            FUNZIONI SAFE PTHREAD
\*=======================================================================*/   
void safe_pthread_attr_init(pthread_attr_t *attr, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_attr_init(attr);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_attr_destroy(pthread_attr_t *attr, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_attr_destroy(attr);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_attr_setdetachstate(attr, detachstate);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_mutex_init(mutex, attr);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_mutex_destroy(pthread_mutex_t *mutex, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_mutex_destroy(mutex);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_mutexattr_init(pthread_mutexattr_t *attr, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_mutexattr_init(attr);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_mutexattr_destroy(pthread_mutexattr_t *attr, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_mutexattr_destroy(attr);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_mutexattr_settype(attr, type);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_cond_init(cond, attr);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_cond_destroy(pthread_cond_t *cond, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_cond_destroy(cond);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr,
                         void *(*start_routine)(void*), void *restrict arg, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_create(thread, attr, start_routine, arg);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_mutex_lock(pthread_mutex_t *mutex, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_mutex_lock(mutex);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_mutex_unlock(pthread_mutex_t *mutex, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_mutex_unlock(mutex);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_cond_wait(cond, mutex);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_cond_signal(pthread_cond_t *cond, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_cond_signal(cond);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}

void safe_pthread_join(pthread_t thread, void **retval, const char *errorMsg)
{
    int checkerror = 0;
    checkerror = pthread_join(thread, retval);
    if(checkerror != 0)
        fprintf(stderr,"%s%s", errorMsg, strerror(checkerror)),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");
}