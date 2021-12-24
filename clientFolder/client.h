#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>

/* Stanza generica del server */
typedef struct{
    char roomName[16];
    int maxClients; 
    int numeroClients; 
} Room;


/*=======================================================================*\
                            CONTROLLO ERRORI
\*=======================================================================*/ 
/* Funzioni controllo errori con perror */
void check_perror(int valueToCheck, const char *s, int unsuccessValue);
/* Funzione controllo errori con strerror */
void check_strerror(int valueToCheck, const char *s, int successValue);


/*=======================================================================*\
                            OPERAZIONI I/O
\*=======================================================================*/ 
ssize_t safeRead(int fd, void *buf, size_t count);
ssize_t safeWrite(int fd, const void *buf, size_t count);
int setupConnectionAndConnect(struct sockaddr_in indirizzo, unsigned short int port);
void write_to_server(int socketfd, void *buf, size_t count, const char *errorMsg);
void read_from_server(int socketfd, void *buf, size_t count, const char *errorMsg);
void cleanSTDIN();
void buildMessageChat(char *output, char *messaggio, char *nickname);


/*=======================================================================*\
                        FUNZIONI USER INTERFACE
\*=======================================================================*/ 
void stampaCarattere(int carattere, int count);
void stampaStanzeUI(Room stanze[], int max);
void menuPrincipaleUI(char *nickname);
void chatStartUI(char *nickname);

