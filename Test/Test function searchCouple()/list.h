#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct node{
    struct node *next;
    char nickname[17];
} List; // Lista di nicknames

// Inserisce un nodo in testa alla lista
List *listNicknames_insertOnHead(List *L, char *nickname, size_t sizeNickname);
// Inserisce un nodo in coda alla lista
List *listNicknames_insertOnQueue(List *L, char *nickname, size_t sizeNickname);
// Elimina un nodo all'interno della lista
List *listNicknames_deleteNode(List *L, char *nickname);
// Distrugge la lista
List *listNicknames_destroy(List *L);
// Stampa la lista
void listNicknames_print(List *L);
// Controlla se la stringa "nickname" è già presente nella lista
bool listNicknames_existingNickname(List *L, char *nickname);

typedef struct{
    List *lista;
    pthread_mutex_t mutex;
} ListNicknames;

typedef struct{
    char nickname[17];
    char address[15];
    int actualRoom_id;
    bool isMatched;
    char matchedAddress[15];
    int matchedRoom_id;
    ListNicknames *listNicknames;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Client;

typedef struct node_t{
    struct node_t *next;
    Client *client;
} nodoQueue;

typedef struct{
    nodoQueue *head;
    nodoQueue *tail;
    int numeroClients;
} Queue;

typedef struct{
    Client *couplantClient1;
    Client *couplantClient2;
} Match;

// Accoda un client nella coda Q
void enqueue(Queue *Q, Client *client);
// Decoda Q, restituisce un client
Client *dequeue(Queue *Q);
// Stampa la coda Q
void printQueue(nodoQueue *head);
// Distruggi la coda Q
void destroyQueue(Queue *Q, nodoQueue *head);
/* Cerca una coppia di client dalla coda Q.
Restituisce la prima coppia di client che rispetta determinate condizioni in una struttura 'Match'.
Restituisce 'NULL' se non la trova. */
Match *searchCouple(Queue *Q, nodoQueue *actual, nodoQueue *actualprev, nodoQueue *head, nodoQueue *headprev);
