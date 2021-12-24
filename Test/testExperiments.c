#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>              
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <malloc.h>
#include <termios.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>

void cleanup_handler(void *arg);
int cancellaThread();
void *threadFunction(void *arg);

void prova();
int a;

int main(int argc, char **argv)
{
    a = 1;
    pthread_t TID;
    pthread_create(&TID, NULL, threadFunction, NULL);

    pthread_join(TID, NULL);

    printf("a: %d\n", a);

    return(EXIT_SUCCESS);
}

void cleanup_handler(void *arg)
{
    char *buffer = "Thread terminato\n";
    write(STDERR_FILENO, buffer, strlen(buffer));

    prova();

    a++;

    fprintf(stderr,"CIao\n");
    fprintf(stderr,"CIao\n");
    fprintf(stderr,"CIao\n");
    fprintf(stderr,"CIao\n");
    fprintf(stderr,"CIao\n");
}

int cancellaThread()
{
    int checkerror = 0;
    fprintf(stderr, "cancello\n");
    checkerror = pthread_cancel(pthread_self());
    //fprintf(stderr, "Cancellato\n");
    //fprintf(stderr,"Cancellato %s\n", strerror(checkerror));
    a++;
    return 1;
}

void *threadFunction(void *arg)
{
    pthread_cleanup_push(cleanup_handler, NULL);

    int c;
    c = cancellaThread();

    pthread_cleanup_pop(1);
}


void prova()
{
    fprintf(stderr, "ciao\n");
}