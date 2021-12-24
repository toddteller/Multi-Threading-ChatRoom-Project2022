#include "server.h"


/*=======================================================================*\
                            OPERAZIONI QUEUE
\*=======================================================================*/ 

/* Accoda un client nella coda Q */
void enqueue(Queue *Q, Client *client)
{
    nodoQueue *newnode = malloc(sizeof(nodoQueue));
    if(newnode == NULL)
        fprintf(stderr, "Errore malloc enqueue()\n"), exit(EXIT_FAILURE);

    newnode->client = client;
    newnode->next = NULL;

    if(Q->tail == NULL)
    {
        Q->head = newnode;
    }
    else
    {
        Q->tail->next = newnode;
    }

    Q->tail = newnode;
    Q->tail->next = NULL;
    Q->numeroClients += 1;
}

/* Decoda Q */
Client *dequeue(Queue *Q)
{
    Client *client;
    if (Q->head == NULL)
    {
        client = NULL;
    }
    else
    {
        client = Q->head->client;
        nodoQueue *temp = Q->head;
        Q->head = Q->head->next;

        if(Q->head == NULL)  {Q->tail = NULL;}
        free(temp);
        Q->numeroClients -= 1;
    }

    return client;
}

/* Elimina il nodo con campo nome uguale a 'nickname' */
nodoQueue *deleteNodeQueue(Queue *Q, nodoQueue *head, nodoQueue *prev, char *nickname)
{
    if(head != NULL)
    {
        if(strncmp(nickname, head->client->nickname, 16) == 0){
            if(head == Q->tail) Q->tail = prev;
            nodoQueue *tmp;
            tmp = head;
            head = head->next;
            free(tmp);
            Q->numeroClients -= 1;
            if(Q->numeroClients == 0){
                Q->head = NULL;
                Q->tail = NULL;
            }
        }
        else{
            head->next = deleteNodeQueue(Q, head->next, head, nickname);
        }
    }
    return head;
}

/* Stampa la coda Q */
void printQueue(nodoQueue *head)
{
    if(head != NULL)
    {
        printf("%s", head->client->nickname);
        printQueue(head->next);
    }
}

/* Distrugge la coda Q */
void destroyQueue(Queue *Q, nodoQueue *head)
{
    if(head != NULL)
    {
        destroyQueue(Q, head->next);
        free(head);
    }
    else
    {
        Q->numeroClients = 0;
        Q->head = NULL;
        Q->tail = NULL;
    }
}

/*   Cerca e restituisce (se esiste, restituisce NULL altrimenti) una coppia di client dalla coda 'Q' che 
*    rispetta determinate condizioni. Implementa la funzionalità richiesta dalla traccia del progetto, cioè 
*    un client non può essere assegnato ad una medesima controparte nella stessa stanza in due assegnazioni consecutive.
*    Parametri di input:
*    > Q è la coda della stanza e Q->head rappresenta il nodo in testa alla coda;
*    > actual: rappresenta il nodo attualmente esaminato dalla funzione ricorsiva;
*    > actualprev: rappresenta il nodo precedente ad 'actual'; */                       
Match *findPairClientsFromQueue(Queue *Q, nodoQueue *actual, nodoQueue *actualprev)
{
    Match *coupleClients = NULL;
    if(actual != NULL && Q->numeroClients >= 2)
    {
        int currentRoom = Q->head->client->currentRoom;

        int CurrentRoomEqualsMatchedRoom_Client1 = (currentRoom == actual->client->matchedRoom);
        int CurrentRoomEqualsMatchedRoom_Client2 = (currentRoom == Q->head->client->matchedRoom);
        int AddressClient1EqualsMatchedAddressClient2 = !strncmp(Q->head->client->matchedAddress, actual->client->address, 15);
        int AddressEqualsClient2MatchedAddressClient1 = !strncmp(Q->head->client->address, actual->client->matchedAddress, 15);

        if( CurrentRoomEqualsMatchedRoom_Client1 && CurrentRoomEqualsMatchedRoom_Client2 &&
            ( AddressClient1EqualsMatchedAddressClient2 || AddressEqualsClient2MatchedAddressClient1) ) // Coppia non valida
        {
            coupleClients = findPairClientsFromQueue(Q, actual->next, actual);
        }
        else // Coppia valida
        {
            coupleClients = (Match*)malloc(sizeof(coupleClients));
            if(coupleClients == NULL){
                fprintf(stderr,"Errore allocazione match findPairClientsFromQueue()");
                return NULL;
            }

            nodoQueue *temp;

            /* Estrazione ed eliminazione 'actual->client' dalla coda Q (couplantClient1) */
            coupleClients->couplantClient1 = actual->client;
            temp = actual;
            actual = actual->next;
            actualprev->next = actual;
            free(temp);

            if(actual == NULL) // Se è vero, è stato appena eliminato l'ultimo l'elemento dalla coda Q
                Q->tail = actualprev;

            /* Estrazione ed eliminazione 'head->client' dalla coda (couplantClient2) */
            coupleClients->couplantClient2 = Q->head->client;
            temp = Q->head;
            Q->head = Q->head->next;
            free(temp);

            Q->numeroClients -= 2;
            if(Q->numeroClients == 0){Q->head = NULL, Q->tail = NULL;}
        }
    }
    return coupleClients;
}



/*=======================================================================*\
                            OPERAZIONI ROOMS
\*=======================================================================*/ 

void destroyRoom(void *arg)
{
    if(arg != NULL)
    {
        Room *Stanza = (Room*)arg;
        destroyQueue(Stanza->coda, Stanza->coda->head);
        safe_pthread_mutex_destroy(Stanza->mutex, "Errore pthreadmutexdestroy destroyRoom");
        safe_pthread_cond_destroy(Stanza->cond, "Errore pthreadconddestroy destroyRoom");

        free(Stanza->coda);
        free(Stanza->mutex);
        free(Stanza->cond);
    }
    
}



/*=======================================================================*\
                            OPERAZIONI CLIENTS
\*=======================================================================*/ 

/* Inizializza i dati di un client. Restituisce 0 se è OK, codice di errore altrimenti. */
void initClient(Client *newClient, int socketClient, char *indirizzo, pthread_mutexattr_t *restrict attr)
{
    // STATO INIZIALE CLIENT
    newClient->socketfd = socketClient; 
    strncpy(newClient->address, indirizzo, 15); 
    newClient->isConnected = true; 
    newClient->isMatched = false; 
    newClient->deletedFromQueue = false; 
    newClient->stopWaiting = false; 
    newClient->chatTimedout = false; 
    newClient->currentRoom = -1; 
    newClient->matchedRoom = -1; 

    if(newClient->mutex == NULL) fprintf(stderr,"ciao\n");

    safe_pthread_mutex_init(newClient->mutex, attr, "Errore pthreadmutexinit initClient");
    safe_pthread_cond_init(newClient->cond, NULL, "Errore pthreadcondinit initClient");
}

/* Distrugge una client. */
void destroyClient(void *arg)
{
    if(arg != NULL)
    {
        Client *client = (Client*)arg;
        fprintf(stderr, "Chiusura client %s nickname %s\n", client->address, client->nickname);
        safe_close(client->socketfd, "Errore chiusura socketfd destroyClient()");
        safe_pthread_mutex_destroy(client->mutex, "Errore pthread_mutex_destroy destroyClient()");
        safe_pthread_cond_destroy(client->cond, "Errore pthread_cond_destroy destroyClient()");
        free(client->mutex);
        free(client->cond);
        free(client);
    }
}



/*=======================================================================*\
                            OPERAZIONI I/O
\*=======================================================================*/
ssize_t safeRead(int fd, void *buf, size_t count)
{
    size_t bytesDaLeggere = count;
    ssize_t bytesLetti = 0;

    while(bytesDaLeggere > 0)
    {
        if((bytesLetti = read(fd, buf+(count-bytesDaLeggere), bytesDaLeggere)) < 0){
            return -1;
        }
        else if(bytesLetti == 0){
            break;
        }
        else{
            bytesDaLeggere -= bytesLetti;
        }
        fprintf(stderr,"Letto:%s\n", (char*)buf);
    }

    return (count - bytesDaLeggere);
}

ssize_t safeWrite(int fd, const void *buf, size_t count)
{
    size_t bytesDaScrivere = count;
    ssize_t bytesScritti = 0;

    while(bytesDaScrivere > 0)
    {
        if((bytesScritti = write(fd, buf+(count-bytesDaScrivere), bytesDaScrivere)) < 0){
            return -1;
        }
        else{
            bytesDaScrivere -= bytesScritti;
        }
        fprintf(stderr,"Scritto: %s\n", (char*)buf);
    }

    return (count - bytesDaScrivere);
}

void impostaTimerSocket(int socketfd, int seconds)
{
    int checkerror = 0;
    struct timeval time;
    time.tv_sec = seconds;
    time.tv_usec = 0;

    checkerror = setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(struct timeval));
    if(checkerror != 0){
        fprintf(stderr, "Errore impostazione timer socket\n");
    }
}

void write_to_client(Client *C, void *buf, size_t count, int error_handling_mode, const char* errorMsg)
{
    long unsigned int bytes = 0;

    bytes = safeWrite(C->socketfd, buf, count);
    if(bytes != count)
    {
        if(errno != EINTR)
        {
            perror(errorMsg);
            fprintf(stderr, "Address Client: %s\n", C->address);
            C->isConnected = false;

            if(error_handling_mode == EXIT_ON_ERROR)
                pthread_cancel(pthread_self()),
                fprintf(stderr,"\n");
        }
    }
}

void read_from_client(Client *C, void *buf, size_t count, int error_handling_mode, const char* errorMsg)
{
    long unsigned int bytes = 0;

    bytes = safeRead(C->socketfd, buf, count);
    if(bytes != count)
    {
        if(errno != EINTR)
        {
            perror(errorMsg);
            fprintf(stderr, "Address Client: %s\n", C->address);
            C->isConnected = false;

            if(error_handling_mode == EXIT_ON_ERROR)
                pthread_cancel(pthread_self()),
                fprintf(stderr,"\n");
        }
    }
}

/* Imposta la connessione per il server. Restituisce 0 se è OK, -1 altrimenti. */
int setupConnection(unsigned short int port, int lunghezza_coda)
{
    int socketfd, checkerror;
    struct sockaddr_in indirizzo;

    socketfd = socket(PF_INET, SOCK_STREAM, 0);
    if(socketfd == -1)
        perror("Errore creazione socket"),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");

    indirizzo.sin_family = AF_INET;
    indirizzo.sin_port = htons(port);
    indirizzo.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(indirizzo.sin_zero), '\0', 8);

    checkerror = bind(socketfd, (struct sockaddr*)&indirizzo, sizeof(indirizzo));
    if(checkerror == -1)
        perror("Errore bind socket"),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");

    checkerror = listen(socketfd, lunghezza_coda);
    if(checkerror == -1)
        perror("Errore bind socket"),
        pthread_cancel(pthread_self()),
        fprintf(stderr,"\n");

    return socketfd;
}
