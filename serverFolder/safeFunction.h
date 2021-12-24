#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdatomic.h>


/*=======================================================================*\
                            FUNZIONI SAFE SIGNAL
\*=======================================================================*/   
void safe_signal(int signum, __sighandler_t handler, const char *errorMsg); // signal()


/*=======================================================================*\
                            FUNZIONI SAFE PTHREAD
\*=======================================================================*/  
void safe_pthread_attr_init(pthread_attr_t *attr, const char *errorMsg);                                                    
void safe_pthread_attr_destroy(pthread_attr_t *attr, const char *errorMsg);                                                    
void safe_pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate, const char *errorMsg);                            
void safe_pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr, const char *errorMsg);  
void safe_pthread_mutex_destroy(pthread_mutex_t *mutex, const char *errorMsg);                                                  
void safe_pthread_mutexattr_init(pthread_mutexattr_t *attr, const char *errorMsg);                                              
void safe_pthread_mutexattr_destroy(pthread_mutexattr_t *attr, const char *errorMsg);                                           
void safe_pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type, const char *errorMsg);                                 
void safe_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr, const char *errorMsg);      
void safe_pthread_cond_destroy(pthread_cond_t *cond, const char *errorMsg);                                                     
void safe_pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr,
                         void *(*start_routine)(void*), void *restrict arg, const char *errorMsg);                              
void safe_pthread_mutex_lock(pthread_mutex_t *mutex, const char *errorMsg);                                                     
void safe_pthread_mutex_unlock(pthread_mutex_t *mutex, const char *errorMsg);                                                   
void safe_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, const char *errorMsg);                                
void safe_pthread_cond_signal(pthread_cond_t *cond, const char *errorMsg);                                                      
void safe_pthread_join(pthread_t thread, void **retval, const char *errorMsg);                                                      


/*=======================================================================*\
                     FUNZIONI SAFE FILE DESCRIPTORS
\*=======================================================================*/  
void safe_close(int fd, const char *errorMsg);

