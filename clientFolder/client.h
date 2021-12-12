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
    int maxClients; // max numero clients
    int numClients; // numero clients attuali
} Room;

/* Funzioni controllo errori con perror */
void check_perror(int valueToCheck, const char *s, int unsuccessValue); //socket
/* Funzione controllo errori con strerror */
void check_strerror(int valueToCheck, const char *s, int successValue);
/* Funzioni lettura e scrittura completa e safe */
ssize_t safeRead(int fd, void *buf, size_t count);
ssize_t safeWrite(int fd, const void *buf, size_t count);

/* Funzione di avvio thread: controlla se il client digita 'STOP' per interrompere l'attesa */
void* checkStopWaiting(void *arg);

/* Costruisce il messaggio prima di inviarlo */
void buildMessageChat(char *output, char *messaggio, char *nickname);
/* Pulisce STDIN_FILENO */
void cleanSTDIN();

/* FUNZIONI UI - USER INTERFACE*/
/* Stampa un carattere 'count' volte */
void stampaCarattere(int carattere, int count);
/* Stampa le stanze prelevate */
void stampaStanze(Room stanze[], int max);
/* Stampa menu principale */
void menuPrincipale(char *nickname);