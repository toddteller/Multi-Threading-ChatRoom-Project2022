#include "AVL.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdatomic.h>

/* Struttura CLIENT: contiene tutte le informazioni sul client necessarie */
typedef struct{
    char nickname[16];
    char address[15];
    int socketfd;
    atomic_bool isConnected; // indica se il client è ancora connesso al server
    atomic_bool deletedFromQueue; // indica se il client è stato cancellato o meno dalla coda
    atomic_bool isMatched; // indica se il client è stato accoppiato con un altro client 
    atomic_bool stopWaiting; // indica se il client ha deciso di interrompere l'attesa 
    atomic_bool chatTimedout; // indica se la chat ha superato il tempo massimo di vita
    int actualRoomID; // indica l'id della stanza attuale
    int matchedRoomID; // indica l'id della stanza in cui il client sta/ha comunicando/comunicato
    char matchedAddress[15]; // indirizzo del client con cui sta/ha comunicando/comunicato
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} Client;

/* Struttura CODA: contiene i clients in attesa di una chat */
// Nodo generico di una coda
typedef struct node_t{
    struct node_t *next;
    Client *client;
} nodoQueue;

typedef struct{
    nodoQueue *head; // puntatore testa
    nodoQueue *tail; // puntatore coda
    int numeroClients; // numero di clients presenti nella coda
} Queue;

/* Struttura MATCH: contiene una coppia di client da mettere in comunicazione*/
typedef struct{
    Client *couplantClient1;
    Client *couplantClient2;
} Match;

/* STRUTTURA AVLNICKNAMES: struttura dati albero AVL contenente tutti i nomi dei nicknames 
                           dei clients connessi e il numero di clients del server*/
typedef struct{
    struct node *root;
    int numeroClientsServer;
    pthread_mutex_t *mutex;
} AVLNicknames;

/* Struttura ROOM: contiene tutte le informazioni sulla stanza necessarie */
typedef struct{
    char roomName[16];
    int idRoom;
    int maxClients;
    int numClients;
    Queue *Q;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} Room;

/* Imposta la connessione per il server. Restituisce 0 se è OK, -1 altrimenti. */
int setupConnection(unsigned short int port, int lunghezza_coda);

/* ----------------------- OPERAZIONI ALBERO AVL NICKNAMES ---------------------- */
/* Inizializza l'albero AVL di nicknames dei clients presenti sul server. Restituisce 0 se è OK, codice di errore altrimenti.*/
int initAVLNicknames(AVLNicknames *T, pthread_mutexattr_t *restrict attr);
/* Distrugge l'albero AVL di nicknames dei clients presenti sul server. Restituisce 0 se è OK, codice di errore altrimenti. */
int destroyAVLNicknames(AVLNicknames *T);
/* ------------------------------------------------------------------------------ */

/* --------------------------- OPERAZIONI QUEUE --------------------------- */
/* Accoda un client nella coda Q */
void enqueue(Queue *Q, Client *client);
/* Decoda Q, restituisce il client che è in testa alla coda. Restituisce NULL
se la coda è vuota. */
Client *dequeue(Queue *Q);
/* Stampa la coda Q */
void printQueue(nodoQueue *head);
/* Elimina il client che ha nickname uguale a 'nickname'. Non elimina se non esiste. */
nodoQueue *deleteNodeQueue(Queue *Q, nodoQueue *head, nodoQueue *prev, char *nickname);
/* Distruggi la coda Q */
void destroyQueue(Queue *Q, nodoQueue *head);
/* Cerca una coppia di client dalla coda Q.
Restituisce la prima coppia di client che rispetta determinate condizioni in una struttura 'Match'.
Restituisce 'NULL' se non la trova. */
Match *searchCouple(Queue *Q, nodoQueue *actual, nodoQueue *actualprev, nodoQueue *head, nodoQueue *headprev);
/* ------------------------------------------------------------------------ */


/* ------------------------- OPERAZIONI SULLE ROOM ------------------------ */
/* Definisce i nomi delle stanze del server */
void setRoomName(char **nameRooms);
/* Inizializza una stanza. Restituisce 0 se è OK, codice di errore altrimenti. */
int initRoom(Room *Stanza, int idRoom, char *roomName, int maxClients, int numClients, pthread_mutexattr_t *restrict attr);
/* Distrugge una stanza. Restituisce 0 in caso di successo, codice di errore altrimenti.*/
int destroyRoom(Room *Stanza);
/* ------------------------------------------------------------------------ */


/* ------------------------ OPERAZIONI SUI CLIENTS ------------------------ */
/* Inizializza i dati di un client. Restituisce 0 se è OK, codice di errore altrimenti. */
int initClient(Client *newClient, int socketClient, char *indirizzo, pthread_mutexattr_t *restrict attr);
/* Distrugge una client. Restituisce 0 in caso di successo, codice di errore altrimenti.*/
int destroyClient(Client *client);
/* ------------------------------------------------------------------------ */

/* ------------------------ OPERAZIONI INPUT/OUTPUT ------------------------ */
ssize_t safeRead(int fd, void *buf, size_t count);
ssize_t safeWrite(int fd, const void *buf, size_t count);
void impostaTimerSocket(int socketfd, int seconds);
/* ------------------------------------------------------------------------- */

/* ------------------------ Funzioni controllo errori  ------------------------ */
void check_strerror(int valueToCheck, const char *s, int successValue); // Genericamente per threads
void check_perror(int valueToCheck, const char *s, int unsuccessValue); // Genericamente per socket
/* --------------------------------------------------------------------------- */