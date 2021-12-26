#include "AVL.h"
#include "safeFunction.h"

/* Costanti generali */
enum constant{
    SERVER_OFF = 9,
    SOCKET_ERROR = -1,
    VISUALIZZA_STANZE = 1,
    ESCI_DAL_SERVER = 2,
    NO_ROOM = -1,
    SELECT_TIMEDOUT = 0,
    CHAT_TIMEDOUT = 4,
    MAX_NUM_ROOMS = 5,
    MAX_CLIENTS_PER_ROOM = 6,
    SERVER_PORT = 9898,
    SERVER_BACKLOG = 10,
};

/* Struttura CLIENT: contiene tutte le informazioni sul client necessarie */
typedef struct{
    char nickname[16];
    char address[15];
    int socketfd;
    atomic_bool isConnected;        
    atomic_bool deletedFromQueue;   
    atomic_bool isMatched;  
    atomic_bool stopWaiting;        
    atomic_bool chatTimedout;       
    int currentRoom;               
    int matchedRoom;           // Ultima stanza matchata   
    char matchedAddress[15];   // Ultimo client matchato    
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} Client;

/* Struttura CODA: contiene i clients in attesa di una chat */
typedef struct node_t{
    struct node_t *next;
    Client *client;
} nodoQueue;

typedef struct{
    nodoQueue *head;
    nodoQueue *tail;
    int numeroClients;
} Queue;

/* Struttura ROOM: contiene tutte le informazioni sulla stanza necessarie */
typedef struct{
    char roomName[16];
    int idRoom;
    int maxClients;
    int numeroClients;
    Queue *coda;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} Room;

/* Struttura MATCH: contiene una coppia di client da mettere in comunicazione*/
typedef struct{
    Client *couplantClient1;
    Client *couplantClient2;
} Match;

/* STRUTTURA AVLNICKNAMES: contiene tutti i nicknames dei client */
typedef struct{
    AlberoAVL *root;
    int numeroClientsServer;
    pthread_mutex_t *mutex;
} AVLNicknames;



/*=======================================================================*\
                            OPERAZIONI QUEUE
\*=======================================================================*/ 
void enqueue(Queue *Q, Client *client);
Client *dequeue(Queue *Q);
void printQueue(nodoQueue *head);
nodoQueue *deleteNodeQueue(Queue *Q, nodoQueue *head, nodoQueue *prev, char *nickname);
void destroyQueue(Queue *Q, nodoQueue *head);
Match *findPairClientsFromQueue(Queue *Q, nodoQueue *actual, nodoQueue *actualprev);

/*=======================================================================*\
                            OPERAZIONI ROOMS
\*=======================================================================*/ 
void destroyRoom(void *arg);

/*=======================================================================*\
                            OPERAZIONI CLIENTS
\*=======================================================================*/ 
void initClient(Client *newClient, int socketClient, char *indirizzo, pthread_mutexattr_t *restrict attr);
void destroyClient(void *arg);

/*=======================================================================*\
                            OPERAZIONI I/O
\*=======================================================================*/ 
ssize_t safeRead(int fd, void *buf, size_t count);
ssize_t safeWrite(int fd, const void *buf, size_t count);
void impostaTimerSocket(int socketfd, int seconds);

enum errorHandling{NOT_EXIT_ON_ERROR, EXIT_ON_ERROR};
void write_to_client(Client *C, void *buf, size_t count, int error_handling_mode, const char* errorMsg);
void read_from_client(Client *C, void *buf, size_t count, int error_handling_mode, const char* errorMsg);

int setupConnection(unsigned short int port, int lunghezza_coda);