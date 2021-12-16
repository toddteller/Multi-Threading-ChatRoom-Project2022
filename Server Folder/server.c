#include "server.h"

/* Imposta la connessione per il server. Restituisce 0 se è OK, -1 altrimenti. */
int setupConnection(unsigned short int port, int lunghezza_coda)
{
    int socketfd, checkerror;
    struct sockaddr_in indirizzo;

    socketfd = socket(PF_INET, SOCK_STREAM, 0);
    if(socketfd == -1) return -1;

    indirizzo.sin_family = AF_INET;
    indirizzo.sin_port = htons(port);
    indirizzo.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(indirizzo.sin_zero), '\0', 8);

    checkerror = bind(socketfd, (struct sockaddr*)&indirizzo, sizeof(indirizzo));
    if(checkerror == -1) return -1;

    checkerror = listen(socketfd, lunghezza_coda);
    if(checkerror == -1) return -1;

    return socketfd;
}

/* ----------------------- OPERAZIONI LISTA NICKNAMES ---------------------- */
/* Inizializza la lista di nicknames dei clients. Restituisce 0 se è OK, codice di errore altrimenti.*/
int initListNicknames(ListNicknames *L, pthread_mutexattr_t *restrict attr)
{
    if(L == NULL){
        fprintf(stderr, "Memoria lista nicknames non allocata initListNicknames()\n");
        return 1;
    }

    int checkerror = 0; // variabile per controllo errori

    // Inizializzazione mutex listaNicknames
    L->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if(L->mutex == NULL){
        fprintf(stderr, "Errore allocazione memoria mutex initListNicknames()\n");
        return 1;
    }
    if((checkerror = pthread_mutex_init(L->mutex, attr)) != 0){
        return checkerror;
    }

    L->numeroClientsServer = 0;
    return 0;
}

/* Distrugge la lista di nicknames dei clients Restituisce 0 se è OK, codice di errore altrimenti. */
int destroyListNicknames(ListNicknames *L)
{
    if(L == NULL) return 0;

    int checkerror; // variabile per controllo errori

    L->lista = listNicknames_destroy(L->lista);

    if((checkerror = pthread_mutex_destroy(L->mutex)) != 0){
        return checkerror;
    }
    free(L->mutex);

    return 0;
}
/* ------------------------------------------------------------------------- */


/* --------------------------- OPERAZIONI QUEUE --------------------------- */
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

/* Funzione ricorsiva: cerca e restituisce (se esiste, restituisce NULL altrimenti) una coppia di client dalla coda 'Q' che rispetta determinate condizioni.
Implementa il servizio richiesto dalla traccia del progetto, cioè un client non può essere assegnato ad una medesima controparte nella stessa stanza
in due assegnazioni consecutive.
Parametri di input:
- Q è la coda;
- actual: rappresenta il nodo attualmente esaminato dalla funzione ricorsiva;
- actualprev: rappresenta il nodo precedente ad 'actual';
- head: rappresenta il nodo che contiene il client per cui stiamo cercando un altro client per la chat;
- headprev: rappresenta il nodo precedente ad 'headprev'.*/
Match *searchCouple(Queue *Q, nodoQueue *actual, nodoQueue *actualprev, nodoQueue *head, nodoQueue *headprev)
{
    Match *matchFound = NULL;
    if(actual != NULL && Q->numeroClients >= 2) // Se non siamo arrivati alla fine della coda Q e il numero di client nella coda Q è almeno 2
    {
        int actualRoomID = head->client->actualRoomID; // ID Stanza attuale

        // cond_a = 1 sse l'id della stanza attuale è uguale all'id della stanza dell'ultimo match di 'actual->client'
        int cond_a = (actualRoomID == actual->client->matchedRoomID);
        // cond_b = 1 sse l'id della stanza attuale è uguale all'id della stanza dell'ultimo match di 'head->client'
        int cond_b = (actualRoomID == head->client->matchedRoomID);
        // cond_c = 1 sse l'indirizzo dell'ultimo client con cui si è collegato 'head->client' è uguale all'indirizzo di 'actual->client'
        int cond_c = !strncmp(head->client->matchedAddress, actual->client->address, 15);
        // cond_d = 1 sse l'indirizzo dell'ultimo client con cui si è collegato 'actual->client' è uguale all'indirizzo di 'head->client'
        int cond_d = !strncmp(head->client->address, actual->client->matchedAddress, 15);

        if(cond_a && cond_b && (cond_c || cond_d)) // Coppia non valida
        {
            matchFound = searchCouple(Q, actual->next, actual, head, headprev);
        }
        else // Coppia valida
        {
            matchFound = (Match*)malloc(sizeof(matchFound));
            if(matchFound == NULL)
                fprintf(stderr,"Errore allocazione match searchCouple()");

            nodoQueue *temp;

            /* Estrazione ed eliminazione 'actual->client' dalla coda Q (couplantClient1) */
            matchFound->couplantClient1 = actual->client;
            temp = actual;
            actual = actual->next;
            actualprev->next = actual;
            free(temp);

            if(actual == NULL) // Se è vero, è stato appena eliminato l'ultimo l'elemento dalla coda Q
                Q->tail = actualprev; // -> aggiornamento puntatore 'Q->tail' della coda Q

            /* Estrazione ed eliminazione 'head->client' dalla coda (couplantClient2) */
            matchFound->couplantClient2 = head->client;
            temp = head;
            head = head->next;
            free(temp);

            if(headprev != NULL) // Se 'head' non è l'elemento in testa alla coda Q (cioè diverso da 'Q->head')
                headprev->next = head; // -> aggiornamento puntatore 'next' di 'headprev'
             else // Se 'head' è l'elemento in testa alla coda Q (cioè uguale a 'Q->head')
                Q->head = head; // -> aggiornamento puntatore 'Q->head' della coda Q

            if(head == NULL && Q->head != NULL) // Se è vero, è stato di nuovo eliminato l'ultimo l'elemento dalla coda
                Q->tail = headprev; // -> aggiornamento puntatore 'Q->tail' della coda Q

            Q->numeroClients -= 2;
            if(Q->numeroClients == 0){Q->head = NULL, Q->tail = NULL;}
        }
    }
    return matchFound;
}
/* ------------------------------------------------------------------------ */


/* ------------------------- OPERAZIONI SULLE ROOM ------------------------ */
/* Definisce i nomi delle stanze del server. */
void setRoomName(char **roomName)
{
    roomName[0] = "Food Room\n";
    roomName[1] = "Music Room\n";
    roomName[2] = "Sports Room\n";
    roomName[3] = "Nerd Room\n";
    roomName[4] = "Business Room\n";
}

/* Inizializza una stanza. Restituisce 0 in caso di successo, codice di errore altrimenti. */
int initRoom(Room *Stanza, int idRoom, char *nameRoom, int maxClients, int numClients, pthread_mutexattr_t *restrict attr)
{
    if(Stanza == NULL){ // Errore: non è stata allocata la memoria
        fprintf(stderr, "Errore initRoom variabile stanza uguale a NULL");
        return 1;
    }

    int checkerror = 0; // variabile per controllo errori

    // Inizializzazione dati stanza
    Stanza->idRoom = idRoom;
    Stanza->maxClients = maxClients;
    Stanza->numClients = numClients;
    memset(Stanza->roomName, '\0', sizeof(Stanza->roomName));
    strncpy(Stanza->roomName, nameRoom, 16);

    // Inizializzazione coda stanza
    Stanza->Q = (Queue*)malloc(sizeof(Queue));
    if(Stanza->Q == NULL){
        fprintf(stderr, "Errore allocazione queue initRoom()\n");
        return 1;
    }

    Stanza->Q->numeroClients = 0;

    // Inizializzazione mutex e cond stanza
    Stanza->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if(Stanza->mutex == NULL){
        fprintf(stderr, "Errore allocazione memoria mutex initRoom()\n");
        return 1;
    }

    if((checkerror = pthread_mutex_init(Stanza->mutex, attr)) != 0){
        return checkerror;
    }

    Stanza->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    if(Stanza->cond == NULL){
        fprintf(stderr, "Errore allocazione memoria cond initRoom()\n");
        return 1;
    }

    if((checkerror = pthread_cond_init(Stanza->cond, NULL)) != 0){
        return checkerror;
    }

    return 0;
}

/* Distrugge una stanza. Restituisce 0 in caso di successo, codice di errore altrimenti.*/
int destroyRoom(Room *Stanza)
{
    if(Stanza == NULL)
        return 0;

    int checkerror = 0; // variabile per controllo errori

    destroyQueue(Stanza->Q, Stanza->Q->head);
    free(Stanza->Q);

    if((checkerror = pthread_mutex_destroy(Stanza->mutex)) != 0){
        return checkerror;
    }
    free(Stanza->mutex);

    if((checkerror = pthread_cond_destroy(Stanza->cond)) != 0){
        return checkerror;
    }
    free(Stanza->cond);

    return 0;
}
/* ------------------------------------------------------------------------ */


/* ------------------------ OPERAZIONI SUI CLIENTS ------------------------ */
/* Inizializza i dati di un client. Restituisce 0 se è OK, codice di errore altrimenti. */
int initClient(Client *newClient, int socketClient, char *indirizzo, pthread_mutexattr_t *restrict attr)
{
    if(newClient == NULL){
        fprintf(stderr, "Memoria client non allocata initClient()\n");
        return 1;
    }

    int checkerror; // variabile per controllo errori

    // STATO INIZIALE CLIENT
    memset(newClient, 0, sizeof(Client));
    newClient->socketfd = socketClient; // socket file descriptor client 
    strncpy(newClient->address, indirizzo, 15); // indirizzo client
    newClient->isConnected = true; // è connesso
    newClient->isMatched = false; // non è matchato con nessun client 
    newClient->deletedFromQueue = false; // non è stato cancellato dalla coda
    newClient->stopWaiting = false; // non ha interrotto l'attesa (client appena creato)
    newClient->chatTimedout = false; // la chat non è andata in timedout (client appena creato)
    newClient->actualRoomID = -1; // non appartiene a nessuna stanza (client appena creato)
    newClient->matchedRoomID = -1; // non è stato matchato in nessuna stanza (client appena creato)

    // Inizializzazione mutex e cond client
    newClient->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if(newClient->mutex == NULL){
        fprintf(stderr, "Errore allocazione memoria mutex initClient()\n");
        return 1;
    }

    if((checkerror = pthread_mutex_init(newClient->mutex, attr)) != 0){
        return checkerror;
    }

    newClient->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    if(newClient->cond == NULL){
        fprintf(stderr, "Errore allocazione memoria cond initClient()\n");
        return 1;
    }

    if((checkerror = pthread_cond_init(newClient->cond, NULL)) != 0){
        return checkerror;
    }

    return 0;
}

/* Distrugge una client. Restituisce 0 in caso di successo, codice di errore altrimenti.*/
int destroyClient(Client *client)
{
    if(client == NULL)
        return 0;

    int checkerror = 0; // variabile per controllo errori

    if((checkerror = close(client->socketfd)) != 0){
        fprintf(stderr, "Errore chiusura socketfd %d destroyClient()\n", client->socketfd);
        return checkerror;
    }
    if((checkerror = pthread_mutex_destroy(client->mutex)) != 0){
        return checkerror;
    }
    free(client->mutex);

    if((checkerror = pthread_cond_destroy(client->cond)) != 0){
        return checkerror;
    }
    free(client->cond);
    free(client);

    return 0;
}
/* ------------------------------------------------------------------------ */


/* ------------------------ OPERAZIONI INPUT/OUTPUT ------------------------ */
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

/* Funzione controllo errori con strerror */
void check_strerror(int valueToCheck, const char *s, int successValue){
    if(valueToCheck != successValue){
        fprintf(stderr, "%s: %s\n", s, strerror(valueToCheck));
        exit(EXIT_FAILURE);
    }
}

/* Funzione controllo errori con perror */
void check_perror(int valueToCheck, const char *s, int unsuccessValue){
    if(valueToCheck == unsuccessValue){
        perror(s);
        exit(EXIT_FAILURE);
    }
}
/* ------------------------------------------------------------------------- */
