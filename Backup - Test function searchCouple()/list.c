#include "list.h"

// Inserisce un nodo in testa alla lista
List *listNicknames_insertOnHead(List *L, char *nickname, size_t sizeNickname){
    List *newNode = (List*)malloc(sizeof(List));
    if(newNode == NULL)
        fprintf(stderr, "Errore malloc listNicknames_insertOnHead\n"),
        exit(EXIT_FAILURE);

    strncpy(newNode->nickname, nickname, sizeNickname);
    newNode->next = L;
    return newNode;
}

// Inserisce un nodo in coda alla lista
List *listNicknames_insertOnQueue(List *L, char *nickname, size_t sizeNickname){
    if(L != NULL){
        L->next = listNicknames_insertOnQueue(L->next, nickname, sizeNickname);
    }
    else{
        List *newNode = (List*)malloc(sizeof(List));
        if(newNode == NULL)
            fprintf(stderr, "Errore malloc listNicknames_insertOnQueue\n"),
            exit(EXIT_FAILURE);

        strncpy(newNode->nickname, nickname, sizeNickname);
        newNode->next = NULL;
        L = newNode;
    }
    return L;
}

// Elimina un nodo dalla lista
List *listNicknames_deleteNode(List *L, char *nickname){
    if(L != NULL){
        if(strcmp(nickname, L->nickname) == 0){
            List *tmp;
            tmp = L;
            L = L->next;
            free(tmp);
        }
        else{
            L->next = listNicknames_deleteNode(L->next, nickname);
        }
    }
    return L;
}

// Distrugge la lista
List *listNicknames_destroy(List *L){
    if(L != NULL){
        L->next = listNicknames_destroy(L->next);
        free(L);
    }
    return NULL;
}

// Stampa la lista
void listNicknames_print(List *L){
    if(L != NULL){
        printf("%s", L->nickname);
        listNicknames_print(L->next);
    }
}

// Controlla se la stringa "nickname" è già presente nella lista
bool listNicknames_existingNickname(List *L, char *nickname){
    bool isPresent = false;
    if(L != NULL){
        if(strcmp(nickname, L->nickname) == 0){
            isPresent = true;
        }
        else{
            isPresent = listNicknames_existingNickname(L->next, nickname);
        }
    }
    return isPresent;
}



void enqueue(Queue *Q, Client *client)
{
    nodoQueue *newnode = malloc(sizeof(nodoQueue));
    if(newnode == NULL)
        fprintf(stderr, "Errore malloc enqueue()\n"),
        exit(EXIT_FAILURE);

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

void printQueue(nodoQueue *head)
{
    if(head != NULL)
    {
        printf("%s", head->client->nickname);
        printQueue(head->next);
    }
}

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
    if(actual != NULL && Q->numeroClients >= 2) // Se non siamo arrivati alla fine della coda e il numero di client nella coda è almeno 2
    {
        int actualRoom_id = head->client->actualRoom_id; // ID Stanza attuale

        // cond_a = 1 sse l'id della stanza attuale è uguale all'id della stanza dell'ultimo match di 'actual->client'
        int cond_a = (actualRoom_id == actual->client->matchedRoom_id);
        // cond_b = 1 sse l'id della stanza attuale è uguale all'id della stanza dell'ultimo match di 'head->client'
        int cond_b = (actualRoom_id == head->client->matchedRoom_id);
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
                fprintf(stderr,"Errore allocazione match searchCouple()"), exit(EXIT_FAILURE);

            nodoQueue *temp;

            /* Estrazione ed eliminazione 'actual->client' dalla coda (couplantClient1) */
            matchFound->couplantClient1 = actual->client;
            temp = actual;
            actual = actual->next;
            actualprev->next = actual;
            free(temp);

            if(actual == NULL) // Se è vero, è stato appena eliminato l'ultimo l'elemento dalla coda
                Q->tail = actualprev; // -> aggiornamento puntatore 'Q->tail' della coda

            /* Estrazione ed eliminazione 'head->client' dalla coda (couplantClient2) */
            matchFound->couplantClient2 = head->client;
            temp = head;
            head = head->next;
            free(temp);

            if(headprev != NULL) // Se 'head' non è l'elemento in testa alla coda (cioè diverso da 'Q->head')
                headprev->next = head; // -> aggiornamento puntatore 'next' di 'headprev'
             else // Se 'head' è l'elemento in testa alla coda (cioè uguale a 'Q->head')
                Q->head = head; // -> aggiornamento puntatore 'Q->head' della coda

            if(head == NULL && Q->head != NULL) // Se è vero, è stato di nuovo eliminato l'ultimo l'elemento dalla coda
                Q->tail = headprev; // -> aggiornamento puntatore 'Q->tail' della coda

            Q->numeroClients -= 2; // Aggiornamento numero clients presenti nella coda 'Q'
        }
    }
    return matchFound;
}
