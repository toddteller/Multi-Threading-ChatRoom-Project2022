#include "list.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

/* Struttura CLIENT: contiene tutte le informazioni sul client necessarie */
typedef struct{
    char nickname[16];
    char address[15];
    int socketfd;
    bool isConnected; // indica se il client è ancora connesso al server
    int actualRoom_id; // indica l'id della stanza attuale
    bool isMatched; // indica se il client è occupato in una chat
    char matchedAddress[15]; // indirizzo del client con cui sta/ha comunicando/comunicato
    int matchedRoom_id; // indica l'id della stanza in cui il client sta/ha comunicando/comunicato
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} Client;

/* Struttura CODA: contiene i clients in attesa di una chat */
typedef struct node_t{ // nodo generico di una coda
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

/* Struttura LISTNICKNAMES: lista con tutti i nicknames dei clients presenti nel server */
typedef struct{
    List *lista;
    int numeroClientsServer; // numero totali clients connessi
    pthread_mutex_t *mutex;
} ListNicknames;

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

/* ----------------------- OPERAZIONI LISTA NICKNAMES ---------------------- */
/* Inizializza la lista di nicknames dei clients. Restituisce 0 se è OK, codice di errore altrimenti.*/
int initListNicknames(ListNicknames *L, pthread_mutexattr_t *restrict attr);
/* Distrugge la lista di nicknames dei clients Restituisce 0 se è OK, codice di errore altrimenti. */
int destroyListNicknames(ListNicknames *L);
/* ------------------------------------------------------------------------- */


/* --------------------------- OPERAZIONI QUEUE --------------------------- */
/* Accoda un client nella coda Q */
void enqueue(Queue *Q, Client *client);
/* Decoda Q, restituisce il client che è in testa alla coda. Restituisce NULL
se la coda è vuota. */
Client *dequeue(Queue *Q);
/* Stampa la coda Q */
void printQueue(nodoQueue *head);
/* Elimina il client che ha nickname uguale a 'nickname'. Non elimina se non esiste. */
void deleteNodeQueue(nodoQueue *head, char *nickname);
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
