#include "server.h"
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_NUM_ROOMS 5 // Il numero massimo di stanze del server
#define MAX_CLIENTS_ROOM 6 // Il numero massimo di client per stanza
#define PORT 9898
#define SERVER_BACKLOG 10 // Massimo numero di connessioni pendenti

/* Funzioni di avvio threads */
void *gestisciRoom(void *arg);
void *gestisciClient(void *arg);
void *gestisciChat(void *arg);
void *checkConnectionClient(void *arg);
void *checkChatTimedout(void *arg);

/* Signal handler */
void chatTimedout(int sig);

/* Variabili globali server */
ListNicknames *listaNicknames; // Puntatore globale per accesso alla lista di nicknames dei clients connessi al server
Room stanzeServer[MAX_NUM_ROOMS]; // Array di stanze del server globale

int main(void)
{
    int checkerror = 0; // variabile per controllo errori

    /* SET SIGUSR1 TO chatTimedout: usato per interrompere una chat aperta in gestisciChat dopo TOT secondi */
    if(signal(SIGUSR1, chatTimedout) == SIG_ERR) fprintf(stderr,"Errore signal SIGUSR1\n");

    /* Ignora segnale SIGPIPE */
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
        perror("Errore signal(SIGPIPE, SIG_IGN)");
        exit(EXIT_FAILURE);
    }

    /* Definizione nome stanze server */
    char *roomName[MAX_NUM_ROOMS]; // array di stringhe
    setRoomName(roomName); // definisce i nomi delle stanze del server

    /* Inizializzazione attributi thread */
    pthread_attr_t *attributi_thread;
    attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
    if(attributi_thread == NULL){
        fprintf(stderr, "Errore allocazione attributi thread main\n");
        exit(EXIT_FAILURE);
    }
    checkerror = pthread_attr_init(attributi_thread);
    check_strerror(checkerror, "Errore init attr thread main", 0);
    checkerror = pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED);
    check_strerror(checkerror, "Errore setdetachstate main", 0);

    /* Inizializzazione attributi mutex */
    pthread_mutexattr_t *attributi_mutex;
    attributi_mutex = (pthread_mutexattr_t*)malloc(sizeof(pthread_mutexattr_t));
    if(attributi_mutex == NULL){
        fprintf(stderr, "Errore allocazione mutexattr main\n");
        exit(EXIT_FAILURE);
    }
    checkerror = pthread_mutexattr_init(attributi_mutex);
    check_strerror(checkerror, "Errore mutexattrinit main", 0);
    checkerror = pthread_mutexattr_settype(attributi_mutex, PTHREAD_MUTEX_ERRORCHECK);
    check_strerror(checkerror, "Errore mutexattrsettype main", 0);

    /* Definizione stanze e creazione threads stanze, funzione di avvio gestisciRoom() */
    pthread_t threadID_room[MAX_NUM_ROOMS];
    for(int i=0; i<MAX_NUM_ROOMS; i++)
    {
            // inizializzazione stanza i-esima
            checkerror = initRoom(&stanzeServer[i], i, roomName[i], MAX_CLIENTS_ROOM, 0, attributi_mutex);
            check_strerror(checkerror, "Errore initRoom main", 0);

            // creazione thread i-esimo, funzione di avvio gestisciRoom()
            checkerror = pthread_create(&threadID_room[i], attributi_thread, gestisciRoom, (void*)&stanzeServer[i]);
            check_strerror(checkerror, "Errore creazione thread gestisciRoom", 0);
    }

    /* Inizializzazione lista nicknames dei clients */
    listaNicknames = (ListNicknames*)malloc(sizeof(ListNicknames));
    if(listaNicknames == NULL){
        fprintf(stderr, "Errore allocazione lista nicknames\n");
        exit(EXIT_FAILURE);
    }
    checkerror = initListNicknames(listaNicknames, attributi_mutex);
    check_strerror(checkerror, "Errore initListNicknames", 0);

    /* Inizializzazione dati client */
    struct sockaddr_in indirizzoClient;
    int socketClient, indirizzo_size;
    pthread_t clientTID;
    char *indirizzoClient_str; // indirizzo client tipo stringa
    Client *newClient;
    indirizzo_size = sizeof(struct sockaddr_in);

    /* Setup e apertura connessione */
    int socketServer;
    socketServer = setupConnection(PORT, SERVER_BACKLOG);
    check_perror(socketServer, "Errore setup connection main", -1);

    /* Accetta nuove connessioni */
    while(1){
        fprintf(stderr, "Server in ascolto, in attesa di connessioni...\n");

        // accetta nuova connessione
        socketClient = accept(socketServer, (struct sockaddr*)&indirizzoClient, (socklen_t*)&indirizzo_size);
        check_perror(socketClient, "Errore accept new connection", -1);

        // conversione indirizzo in stringa formato dotted
        indirizzoClient_str = inet_ntoa(indirizzoClient.sin_addr);

        // inizializzazione nuovo client
        newClient = (Client*)malloc(sizeof(Client));
        if(newClient == NULL){
            fprintf(stderr, "Errore allocazione nuovo client\n");
            exit(EXIT_FAILURE);
        }
        checkerror = initClient(newClient, socketClient, indirizzoClient_str, attributi_mutex);
        check_strerror(checkerror, "Errore initClient main", 0);

        // creazione thread funzione di avvio gestisciClient()
        checkerror = pthread_create(&clientTID, attributi_thread, gestisciClient, (void*)newClient);
        check_strerror(checkerror, "Errore creazione thread gestisciClient", 0);
    }

    fprintf(stderr, "Chiusura server.\n");

    /* Distruzione attributi thread */
    checkerror = pthread_attr_destroy(attributi_thread);
    check_strerror(checkerror, "Errore threadattrdestroy main", 0);
    free(attributi_thread);

    /* Distruzione attributi mutex */
    checkerror = pthread_mutexattr_destroy(attributi_mutex);
    check_strerror(checkerror, "Errore mutexattrdestroy main", 0);
    free(attributi_mutex);

    /* Distruzione lista nicknames */
    checkerror = destroyListNicknames(listaNicknames);
    check_strerror(checkerror, "Errore destroyListNicknames", 0);

    /* Chiusura socket Server */
    checkerror = close(socketServer);
    check_perror(checkerror, "Errore chiusura socket Server", -1);

    exit(EXIT_SUCCESS);
}




/* Funzione di avvio thread - stanza di un server */
void *gestisciRoom(void *arg)
{
    Room *Stanza = (Room*)arg;
    int checkerror = 0; // variabile per controllo errori

    fprintf(stderr, "Thread stanza avviato. Nome stanza: %s", Stanza->roomName);

    // utilizzata per evitare situazioni di dead lock
    bool notFound = false; // significato: non è stata trovata una coppia di client da mettere in comunicazione

    do
    {
        /* Resta in attesa fino a quando la coda non contiene almeno due clients */
        checkerror = pthread_mutex_lock(Stanza->mutex); // LOCK STANZA
        if(checkerror != 0) fprintf(stderr, "Errore mutexlock room server %d\n", Stanza->idRoom);

        while(Stanza->Q->numeroClients < 2 || notFound) 
        { 
            notFound = false;
            fprintf(stderr, "Stanza in attesa: %s", Stanza->roomName);
            checkerror = pthread_cond_wait(Stanza->cond, Stanza->mutex);
            if(checkerror != 0){
                 fprintf(stderr, "Errore condwait room server %d\n", Stanza->idRoom);
                 break;
            }
        }

        fprintf(stderr, "Stanza uscita dall'attesa: %s", Stanza->roomName);

        if(checkerror != 0){ // Si è verificato un errore
            break;
        }
        else
        { 
            /* Cerca una coppia di client da mettere in comunicazione */
            fprintf(stderr, "Cerca coppia client: %s", Stanza->roomName);

            Match *matchFound = NULL; // coppia di client
            matchFound = searchCouple(Stanza->Q, Stanza->Q->head->next, Stanza->Q->head, Stanza->Q->head, NULL); // cerca coppia client coda 

            if(matchFound == NULL) // Coppia client non trovata: ritorna in attesa
            { 
                fprintf(stderr, "Coppia client non trovata: %s", Stanza->roomName);
                notFound = true; // non trovata
            }
            else // Coppia client trovata
            { 
                notFound = false; // trovata

                Client *Client1 = matchFound->couplantClient1;
                Client *Client2 = matchFound->couplantClient2;

                fprintf(stderr, "Coppia client trovata: %s|%s. Stanza %s", Client1->nickname, Client2->nickname, Stanza->roomName);

                if(!Client1->isConnected && !Client2->isConnected){ // Entrambi i clients si sono disconnessi: ritorna in attesa
                    /* Comunica a gestisciClient() che i due clients disconnessi sono stati già eliminati dalla coda */
                    fprintf(stderr, "Client %s e Client %s si sono disconessi, ritorno in attesa stanza: %s", Client1->nickname, Client2->nickname, Stanza->roomName);
                    Client1->deletedFromQueue = true; // Client 1 cancellato dalla coda
                    Client2->deletedFromQueue = true; // Client 2 cancellato dalla coda
                    Stanza->numClients -= 2; // decrementa numero clients della stanza
                }
                else if(!Client1->isConnected){ // Client 1 si è disconnesso: ritorna in attesa
                    fprintf(stderr, "Client %s si è disconesso. Riaccoda Client %s, ritorno in attesa stanza: %s", Client1->nickname, Client2->nickname, Stanza->roomName);
                    /* Comunica a gestisciClient() che Client1 è stato già eliminato dalla coda */
                    Client1->deletedFromQueue = true; // Client 1 cancellato dalla coda
                    Stanza->numClients -= 1; // decrementa numero clients della stanza
                    enqueue(Stanza->Q, Client2); // riaccoda client 2
                }
                else if(!Client2->isConnected){ // Client 2 si è disconnesso: ritorna in attesa
                    fprintf(stderr, "Client %s si è disconesso. Riaccoda Client %s, ritorno in attesa stanza: %s", Client2->nickname, Client1->nickname, Stanza->roomName);
                    /* Comunica a gestisciClient() che Client2 è stato già eliminato dalla coda */
                    Client2->deletedFromQueue = true; // Client 2 cancellato dalla coda
                    Stanza->numClients -= 1; // decrementa numero clients della stanza
                    enqueue(Stanza->Q, Client1); // riaccoda client 1
                }
                else // Entrambi i client sono connessi: avvio chat
                { 
                    fprintf(stderr, "Coppia valida: %s|%s. Stanza %s", Client1->nickname, Client2->nickname, Stanza->roomName);

                    /* Notifica Client 1 e Client 2: non è possibile interrompere l'attesa, match trovato */
                    ssize_t bytesScritti;
                    /* Il server invia un feedback al CLIENT 1 "OK" */
                    bytesScritti = safeWrite(Client1->socketfd, "OK", 2);
                    if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                        fprintf(stderr, "Errore scrittura 'OK' client %s\n", Client1->nickname);
                        Client1->isConnected = false;
                    }

                    /* Il server invia un feedback al CLIENT 2 "OK" */
                    bytesScritti = safeWrite(Client2->socketfd, "OK", 2);
                    if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                        fprintf(stderr, "Errore scrittura 'OK' client %s\n", Client2->nickname);
                        Client2->isConnected = false;
                    }

                    /* Aggiorna info Client 1 e Client 2: matchedRoom */
                    Client1->matchedRoomID = Stanza->idRoom; 
                    Client2->matchedRoomID = Stanza->idRoom;

                    /* Inizializzazione attributi thread gestisciChat() */
                    pthread_t tid;
                    pthread_attr_t *attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
                    checkerror = pthread_attr_init(attributi_thread);
                    checkerror = pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED);
                    if(attributi_thread == NULL || checkerror != 0){
                        fprintf(stderr, "Errore allocazione attributi thread stanza %d\n", Stanza->idRoom);
                        break;
                    }

                    /* Avvia thread chat */
                    fprintf(stderr, "Creazione thread gestisciChat tra %s|%s, stanza %s", Client1->nickname, Client2->nickname, Stanza->roomName);
                    checkerror = pthread_create(&tid, attributi_thread, gestisciChat, (void*)matchFound);
                    if(checkerror != 0) fprintf(stderr, "Errore avvio chat room\n");

                    free(attributi_thread);
                }
            }
        }

        checkerror = pthread_mutex_unlock(Stanza->mutex); // UNLOCK STANZA
        if(checkerror != 0) fprintf(stderr, "Errore mutexunlock room server %d\n", Stanza->idRoom);

    }while(checkerror == 0);
    
    /* Uscita: distruggi stanza ed esci */
    fprintf(stderr, "Distruggi stanza %s", Stanza->roomName);
    checkerror = destroyRoom(Stanza);
    check_strerror(checkerror, "Errore destroyRoom\n", 0);

    return NULL;
}

/* Funzione di avvio thread - client appena connesso */
void *gestisciClient(void *arg)
{
    Client *thisClient = (Client*)arg;

    int checkerror = 0;
    ssize_t bytesLetti = 0, bytesScritti = 0;
    char buffer[16];

    fprintf(stderr, "Nuova connessione. Client: %d, Indirizzo: %s\n", thisClient->socketfd, thisClient->address);

    /* Lettura nickname del client. La lunghezza del nickname è di massimo
    16 caratteri. Il client deve inviare un messaggio di 16 bytes. Se il client
    viola questa condizione, la connessione verrà chiusa e il thread terminato. */
    impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
    memset(buffer, '\0', sizeof(buffer));
    bytesLetti = safeRead(thisClient->socketfd, buffer, 16);
    if(bytesLetti != 16){ // Errore: distruggi client e chiudi thread.
        fprintf(stderr, "[1] Errore lettura client %s\n", thisClient->address);
        checkerror = destroyClient(thisClient);
        if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
        return NULL;
    }
    impostaTimerSocket(thisClient->socketfd, 0); // elimina timer socket

    /* Copia nickname client */
    strncpy(thisClient->nickname, buffer, 16);
    fprintf(stderr, "Nickname: %s\n", thisClient->nickname);

    /* Controlla se il nickname è già presente nel server */
    bool nicknameExists = false;

    checkerror = pthread_mutex_lock(listaNicknames->mutex); // LOCK LISTNICKNAMES
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock existingNickname %s\n", strerror(checkerror));

    nicknameExists = listNicknames_existingNickname(listaNicknames->lista, thisClient->nickname, 16);

    checkerror = pthread_mutex_unlock(listaNicknames->mutex); // UNLOCK LISTNICKNAMES
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock existingNickname %s\n", strerror(checkerror));

    if(nicknameExists) // Nickname già esistente
    {
        /* Il server invia un feedback "EX" al client per indicare che il nickname è già presente nel server */
        bytesScritti = safeWrite(thisClient->socketfd, "EX", 2);
        if(bytesScritti != 2){ // Errore
            fprintf(stderr, "[2] Errore scrittura 'EX' client %s\n", thisClient->address);
        }
        checkerror = destroyClient(thisClient); // distruggi client e chiudi thead
        if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
        fprintf(stderr, "Client %s chiusura: nickname già esistente\n", thisClient->address);
        return NULL;
    }
    else // Il nickname non esiste 
    {
        /* Il server invia un feedback al client "OK" */
        bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
        if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
            fprintf(stderr, "[3] Errore scrittura 'OK' client %s\n", thisClient->nickname);
            checkerror = destroyClient(thisClient);
            if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
            return NULL;
        }
    }

    /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
    questa condizione, la connessione viene chiusa e il thread terminato. */
    memset(buffer, '\0', sizeof(buffer));
    impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
    bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
    if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
        fprintf(stderr, "[4] Errore lettura feedback client %s\n", thisClient->nickname);
        checkerror = destroyClient(thisClient);
        if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
        return NULL;
    }
    impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer socket

    /* Inserimento nickname thisClient nella lista dei nicknames */
    checkerror = pthread_mutex_lock(listaNicknames->mutex); // LOCK LISTNICKNAMES
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock insertOnHead %s\n", strerror(checkerror));

    listaNicknames->lista = listNicknames_insertOnHead(listaNicknames->lista, thisClient->nickname, 16);
    listaNicknames->numeroClientsServer += 1;

    checkerror = pthread_mutex_unlock(listaNicknames->mutex); // UNLOCK LISTNICKNAMES
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock insertOnHead %s\n", strerror(checkerror));

    /* Leggi operazione da effettuare dal client*/
    do
    {
        bool checkInput; // Variabile controllo input client
        int input = 0; // Valore inserito dal client
        thisClient->actualRoomID = -1; // reset: il client non appartiene a nessuna stanza
        thisClient->stopWaiting = false; // reset: il client non ha interrotto l'attesa
        thisClient->deletedFromQueue = false; // reset: il client non appartiene a nessuna coda

        do // Ciclo controllo input client
        {
            checkInput = false;
            /* Il server deve ricevere '1' oppure '2'. Corrispondono ad operazioni da eseguire. */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
            bytesLetti = safeRead(thisClient->socketfd, buffer, 1);
            if(bytesLetti != 1){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[5] Errore lettura input client %s %d\n", thisClient->nickname, errno);
                thisClient->isConnected = false;
                break;
            }
            impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer socket

            /* Il server invia al client "IN" se l'input non è valido */
            if(strncmp("1", buffer, 1) != 0 && strncmp("2", buffer, 1) != 0) // Input non valido, riprova
            { 
                bytesScritti = safeWrite(thisClient->socketfd, "IN", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[6] Errore scrittura 'IN' client %s\n", thisClient->nickname);
                    thisClient->isConnected = false;
                    break;
                }
            }
            else checkInput = true; // -> input VALIDO
        }while(!checkInput); // Continua finché l'input non è valido

        if(!checkInput) break; // Errore, esci e chiudi client

        input = buffer[0] - '0'; // Salvataggio input inserito dal client

        /* Il server invia un feedback al client "OK" */
        bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
        if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
            fprintf(stderr, "[7] Errore scrittura 'OK' client %s\n", thisClient->nickname);
            thisClient->isConnected = false;
            break;
        }

        /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
        questa condizione, la connessione viene chiusa e il thread terminato. */
        memset(buffer, '\0', sizeof(buffer));
        impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
        bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
        if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
            fprintf(stderr, "[8] Errore lettura feedback client %s\n", thisClient->nickname);
            thisClient->isConnected = false;
            break;
        }
        impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer socket

        /* Il client ha scelto di visualizzare le stanze */
        if(input == 1)
        {
            /* Il server invia il numero totale delle stanze al client */
            memset(buffer, '\0', sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%d", MAX_NUM_ROOMS);

            bytesScritti = safeWrite(thisClient->socketfd, buffer, 1);
            if(bytesScritti != 1){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[9] Errore scrittura numero stanze client %s\n", thisClient->nickname);
                thisClient->isConnected = false;
                break;
            }

            /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
            questa condizione, la connessione viene chiusa e il thread terminato. */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
            bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
            if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[10] Errore lettura feedback client %s\n", thisClient->nickname);
                thisClient->isConnected = false;
                break;
            }
            impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer SOCKET

            /* Il server invia informazioni sulle stanze al client */
            bool error = false;
            for(int i=0; i<MAX_NUM_ROOMS; i++)
            {
                // INVIO NOME STANZA I-ESIMA
                checkerror = pthread_mutex_lock(stanzeServer[i].mutex); // LOCK STANZA
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                bytesScritti = safeWrite(thisClient->socketfd, stanzeServer[i].roomName, 16);
                if(bytesScritti != 16){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[11] Errore scrittura nome stanze %d-iesima client %s\n", i, thisClient->nickname);
                    error = true; // Setta errore
                    thisClient->isConnected = false;
                    break;
                }

                checkerror = pthread_mutex_unlock(stanzeServer[i].mutex); // UNLOCK STANZA
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                questa condizione, la connessione viene chiusa e il thread terminato. */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[12] Errore lettura feedback client %s\n", thisClient->nickname);
                    error = true; // Setta errore
                    thisClient->isConnected = false;
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

                // INVIO NUMERO MASSIMO CLIENTS STANZA I-ESIMA
                memset(buffer, '\0', sizeof(buffer));
                snprintf(buffer, sizeof(buffer), "%d", stanzeServer[i].maxClients);

                checkerror = pthread_mutex_lock(stanzeServer[i].mutex); // LOCK STANZA
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                bytesScritti = safeWrite(thisClient->socketfd, buffer, 1);
                if(bytesScritti != 1){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[13] Errore scrittura numero max stanza client %s\n", thisClient->nickname);
                    error = true; // Setta errore
                    thisClient->isConnected = false;
                    break;
                }

                checkerror = pthread_mutex_unlock(stanzeServer[i].mutex); // UNLOCK STANZA
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                questa condizione, la connessione viene chiusa e il thread terminato. */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[14] Errore lettura feedback client %s\n", thisClient->nickname);
                    error = true; // Setta errore
                    thisClient->isConnected = false;
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

                // INVIO NUMERO CLIENTS ATTUALI STANZA I-ESIMA
                memset(buffer, '\0', sizeof(buffer));
                snprintf(buffer, sizeof(buffer), "%d", stanzeServer[i].numClients);

                checkerror = pthread_mutex_lock(stanzeServer[i].mutex); // LOCK STANZA
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                bytesScritti = safeWrite(thisClient->socketfd, buffer, 1);
                if(bytesScritti != 1){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[15] Errore scrittura numero clients stanza client %s\n", thisClient->nickname);
                    error = true; // Setta errore
                    thisClient->isConnected = false;
                    break;
                }

                checkerror = pthread_mutex_unlock(stanzeServer[i].mutex); // UNLOCK STANZA
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                questa condizione, la connessione viene chiusa e il thread terminato. */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[16] Errore lettura feedback client %s\n", thisClient->nickname);
                    error = true; // Setta errore
                    thisClient->isConnected = false;
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer socket
            } // END for

            if(error || checkerror != 0) break; // Errore -> distruggi client ed esci

            /* Il server invia un feedback al client "OK" */
            bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
            if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[17] Errore scrittura 'OK' client %s\n", thisClient->nickname);
                thisClient->isConnected = false;
                break;
            }

            /* Il client deve scegliere una stanza */
            checkInput = false;
            do {
                /* Il server deve ricevere un numero tra 0 e MAX_NUM_ROOMS-1 per identificare una stanza */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 1);
                if(bytesLetti != 1){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[18] Errore lettura feedback client %s\n", thisClient->nickname);
                    thisClient->isConnected = false;
                    error = true;
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer socket

                input = (buffer[0] - '0') - 1; // Salvataggio input client

                /* Il server controlla l'input */
                if(input >= 0 && input <= MAX_NUM_ROOMS-1) // Input non valido
                {
                    checkInput = true; // input valido
                }
                else // Input valido
                {
                    /* Il server invia al client "IN" se l'input non è valido */
                    bytesScritti = safeWrite(thisClient->socketfd, "IN", 2);
                    if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                        fprintf(stderr, "[19] Errore scrittura 'IN' client %s\n", thisClient->nickname);
                        thisClient->isConnected = false;
                        error = true;
                        break;
                    }
                }
            } while(!checkInput); // Continua finché l'input non è valido

            if(error) break; // Errore -> distruggi client ed esci

            /* Il server invia un feedback al client "OK" */
            bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
            if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[20] Errore scrittura 'OK' client %s\n", thisClient->nickname);
                thisClient->isConnected = false;
                break;
            }

            /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
            questa condizione, la connessione viene chiusa e il thread terminato. */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
            bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
            if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[21] Errore lettura feedback client %s\n", thisClient->nickname);
                thisClient->isConnected = false;
                break;
            }
            impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer socket

            /* Controlla se la stanza selezionata dal client è piena */
            bool isFull = false;
            checkerror = pthread_mutex_lock(stanzeServer[input].mutex); // LOCK STANZA SCELTA
            if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

            if(stanzeServer[input].numClients == stanzeServer[input].maxClients) isFull = true;

            checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK STANZA SCELTA
            if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

            /* Il server invia un feedback al client "OK" oppure "FU" */
            if(!isFull) // Stanza non piena
            { 
                /* Il server invia un feedback al client "OK": la stanza non è piena  */
                bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[22] Errore scrittura 'OK' client %s\n", thisClient->nickname);
                    thisClient->isConnected = false;
                    break;
                }
            }
            else // Stanza piena
            {
                /* Il server invia un feedback al client "FU": stanza piena */
                bytesScritti = safeWrite(thisClient->socketfd, "FU", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[23] Errore scrittura 'FU' client %s\n", thisClient->nickname);
                    thisClient->isConnected = false;
                    break;
                }
            }

            /* Se la stanza non è piena il server continua, altrimenti ritorna all'inizio (menu principale) */
            if(!isFull)
            {
                do // ciclo do while: ritorna in attesa nella stessa stanza. Può capitare solo quando la chat va in timedout.
                {
                    /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                    questa condizione, la connessione viene chiusa e il thread terminato. */
                    memset(buffer, '\0', sizeof(buffer));
                    impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
                    bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                    if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                        fprintf(stderr, "[24] Errore lettura feedback client %s\n", thisClient->nickname);
                        thisClient->isConnected = false;
                        break;
                    }
                    impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer socket

                    /* Creazione thread checkConnectionClient(): controlla se il client chiude la connessione durante l'attesa */
                    fprintf(stderr,"Thread checkConnectionClient creato, client %s\n", thisClient->nickname);
                    pthread_t tid;
                    checkerror = pthread_create(&tid, NULL, checkConnectionClient, (void*)thisClient);
                    if(checkerror != 0){
                        fprintf(stderr,"Client %s, errore creazione thread checkConnectionClient: %s\n", thisClient->nickname, strerror(checkerror));
                        thisClient->isConnected = false;
                        break;
                    }


                    /* Inserimento thisClient nella coda della stanza scelta e risveglia stanza */
                    checkerror = pthread_mutex_lock(stanzeServer[input].mutex); // LOCK STANZA SCELTA
                    if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                    enqueue(stanzeServer[input].Q, thisClient); // inserimento in coda
                    fprintf(stderr, "Client %s inserito in coda nella stanza %s", thisClient->nickname, stanzeServer[input].roomName);

                    if(!thisClient->chatTimedout) // Se l'ultima chat non è andata in timedout vuol dire che è la prima volta che entra in questa stanza
                    {
                        fprintf("Client %s aumenta numero clients stanza %s", thisClient->nickname, stanzeServer[input].roomName);
                        stanzeServer[input].numClients += 1; // aumento numero clients della stanza
                    }
                    
                    thisClient->chatTimedout = false; // resetta variabile: l'ultima chat non è andata in timedout
                    thisClient->actualRoomID = stanzeServer[input].idRoom; // assegna actual room al client
                    checkerror = pthread_cond_signal(stanzeServer[input].cond); // risveglia stanza
                    if(checkerror != 0) fprintf(stderr, "Errore condsignal stanzaServerQueue %s\n", strerror(checkerror));

                    checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK STANZA SCELTA
                    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));


                    /* Il server mette in attesa questo thread (e il client) */
                    checkerror = pthread_mutex_lock(thisClient->mutex); // LOCK THIS CLIENTS
                    if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));
                    
                    /* continua fino a quando il client non ha trovato un match, il client non si è disconnesso e 
                        il client non ha deciso di interrompere l'attesa */
                    while(!thisClient->isMatched && thisClient->isConnected && !thisClient->stopWaiting){
                        fprintf(stderr,"In attesa di un match, client %s.\n", thisClient->nickname); 
                        checkerror = pthread_cond_wait(thisClient->cond, thisClient->mutex);
                        if(checkerror != 0){ // Errore*
                            fprintf(stderr, "[25] Errore condwait client %s\n", thisClient->nickname);
                            thisClient->isConnected = false;
                            break;
                        }
                    }

                    checkerror = pthread_mutex_unlock(thisClient->mutex); // UNLOCK THIS CLIENT
                    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                    fprintf(stderr, "Uscito dall'attesa, client %s.\n", thisClient->nickname);

                    /* Controlla se il client si è disconnesso oppure ha interrotto l'attesa */
                    if(!thisClient->isConnected) // Errore*: il client si è disconnesso o qualcosa è andato storto
                    { 
                        fprintf(stderr, "Client %s disconesso.\n", thisClient->nickname);

                        if(!thisClient->deletedFromQueue) // Se non è stato già cancellato, elimina il client che si è disconnesso dalla coda della stanza
                        {
                            fprintf(stderr,"Client %s disconnesso e non cancellato dalla coda: cancella.\n", thisClient->nickname);
                            checkerror = pthread_mutex_lock(stanzeServer[input].mutex); // LOCK STANZA SCELTA
                            if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzeServer client disconesso %s\n", strerror(checkerror));

                            // elimina client dalla coda
                            stanzeServer[input].Q->head = deleteNodeQueue(stanzeServer[input].Q, stanzeServer[input].Q->head, NULL, thisClient->nickname);                                                          
                            thisClient->deletedFromQueue = true; // client cancellato dalla coda

                            checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK STANZA SCELTA
                            if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzeServer client disconesso %s\n", strerror(checkerror));
                        }

                        // Sveglia thread gestisciChat per eliminarlo 
                        checkerror = pthread_cond_signal(thisClient->cond);
                        if(checkerror != 0) fprintf(stderr, "Errore condsignal client->chat %s\n", strerror(checkerror));

                        // Terminazione thread checkConnectionClient
                        checkerror = pthread_join(tid, NULL);
                        if(checkerror != 0){ // Errore
                            fprintf(stderr,"Errore %s pthread_join client %s\n", strerror(checkerror), thisClient->nickname);
                        }
                    }
                    else if(thisClient->stopWaiting) // Il client ha interrotto l'attesa
                    {
                        fprintf(stderr, "Client %s ha interrotto l'attesa: cancella client dalla coda.\n", thisClient->nickname);

                        /* Il server invia un feedback al client "ST": risveglia client, stop waiting */
                        fprintf(stderr, "Messaggio 'ST' sblocco inviato al client %s.\n", thisClient->nickname);
                        bytesScritti = safeWrite(thisClient->socketfd, "ST", 2);
                        if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                            fprintf(stderr, "[26] Errore scrittura 'ST' client %s\n", thisClient->nickname);
                            thisClient->isConnected = false;
                            break;
                        }

                        checkerror = pthread_mutex_lock(stanzeServer[input].mutex); // LOCK STANZA SCELTA
                        if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzeServer client stopwaiting %s\n", strerror(checkerror));

                        // elimina client dalla coda
                        stanzeServer[input].Q->head = deleteNodeQueue(stanzeServer[input].Q, stanzeServer[input].Q->head, NULL, thisClient->nickname);                                                         
                        thisClient->deletedFromQueue = true; // client cancellato dalla coda

                        checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK STANZA SCELTA
                        if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzeServer client stopwaiting %s\n", strerror(checkerror));

                        // Terminazione thread checkConnectionClient
                        checkerror = pthread_join(tid, NULL);
                        if(checkerror != 0){ // Errore
                            fprintf(stderr,"Errore %s pthread_join client %s\n", strerror(checkerror), thisClient->nickname);
                        }
                    }
                    /* Continua se il client non si è disconesso e non ha interrotto l'attesa */
                    else
                    {
                        /* Il server invia un feedback al client "OK": risveglia client, chat avviata */
                        fprintf(stderr, "Messaggio 'OK' sblocco inviato al client %s.\n", thisClient->nickname);
                        bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
                        if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                            fprintf(stderr, "[27] Errore scrittura 'OK' client %s\n", thisClient->nickname);
                            thisClient->isConnected = false;
                            break;
                        }

                        /* Aspetta la terminazione del thread checkConnectionClient */
                        /* Il client dovrebbe inviare 'OK' per terminare il thread checkConnectionClient() */
                        fprintf(stderr,"Aspetto thread checkConnectionClient client %s.\n", thisClient->nickname);

                        checkerror = pthread_join(tid, NULL);
                        if(checkerror != 0){ // Errore
                            fprintf(stderr,"[28] Errore pthread_join client %s\n", thisClient->nickname);
                            thisClient->isConnected = false;     
                        }

                        fprintf(stderr,"Thread checkConnectionClient terminato client %s.\n", thisClient->nickname);

                        /* Risveglia thread gestisciChat associato */
                        checkerror = pthread_cond_signal(thisClient->cond);
                        if(checkerror != 0) fprintf(stderr, "Errore condsignal client->chat %s\n", strerror(checkerror));

                        /* Il server mette in attesa il client fino alla fine della chat */
                        fprintf(stderr, "Thread Client %s in attesa (chat).\n", thisClient->nickname);
                        checkerror = pthread_mutex_lock(thisClient->mutex); // LOCK THIS CLIENT
                        if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));

                        // continua fino a quando il client è impegnato in una chat ed è connesso
                        while(thisClient->isMatched && thisClient->isConnected)
                        {
                            fprintf(stderr,"Client %s in attesa chat.\n", thisClient->nickname);
                            checkerror = pthread_cond_wait(thisClient->cond, thisClient->mutex);
                            if(checkerror != 0){ // Errore*
                                fprintf(stderr, "[29] Errore condwait client %s\n", thisClient->nickname);
                                thisClient->isConnected = false;
                            }
                        }

                        checkerror = pthread_mutex_unlock(thisClient->mutex); // UNLOCK THIS CLIENT
                        if(checkerror != 0) fprintf(stderr, "Errore mutexunlock Client %s\n", strerror(checkerror));
            
                        fprintf(stderr, "Chat terminata per il client %s.\n", thisClient->nickname);

                        /* Il client è ancora connesso */
                        if(thisClient->isConnected)
                        {
                            fprintf(stderr, "Client %s ancora connesso.\n", thisClient->nickname);
                            /* Il server invia un feedback al client "OK" */
                            bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
                            if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                                fprintf(stderr, "[30] Errore scrittura 'OK' client %s\n", thisClient->nickname);
                                thisClient->isConnected = false;
                            }

                            /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                                questa condizione, la connessione viene chiusa e il thread terminato. */
                            memset(buffer, '\0', sizeof(buffer));
                            impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 20 secondi
                            bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                            if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                                fprintf(stderr, "[31] Errore lettura feedback client %s\n", thisClient->nickname);
                                thisClient->isConnected = false;
                            }
                            impostaTimerSocket(thisClient->socketfd, 0); // elimina timer socket
                        }
                    }
                }while(thisClient->isConnected && thisClient->chatTimedout && !thisClient->stopWaiting); /* rimani in questo blocco (che rappresenta una stanza) fino a quando:
                                                                                                            il client è connesso,
                                                                                                            la chat del client è andata in timedout (quindi nuova chat da cercare),
                                                                                                            il client non ha interrotto l'attesa. */
            }
        }
        else if(input == 2) // Il client ha scelto di uscire dal server
        { 
            fprintf(stderr, "Il client %s esce dal server.\n", thisClient->nickname);
            thisClient->isConnected = false;
        }
        else // ERROR case
        {
            fprintf(stderr, "Errore input client %s\n", thisClient->nickname);
            thisClient->isConnected = false;
        }

        /* Uscita client dalla stanza, decrementa numero clients stanza */
        if(thisClient->actualRoomID != -1) // Se appartiene ad una stanza (-1 non appartiene a nessuna stanza)
        {
            checkerror = pthread_mutex_lock(stanzeServer[thisClient->actualRoomID].mutex); // LOCK STANZA SCELTA
            if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanze %s\n", strerror(checkerror));

            fprintf(stderr, "Client %s è uscito dalla stanza %s", thisClient->nickname, stanzeServer[thisClient->actualRoomID].roomName);
            stanzeServer[thisClient->actualRoomID].numClients -= 1;

            checkerror = pthread_mutex_unlock(stanzeServer[thisClient->actualRoomID].mutex); // LOCK STANZA SCELTA
            if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanze %s\n", strerror(checkerror));
        }

    }while(thisClient->isConnected); // continua fino a quando il client è connesso (ritorna a 'Scegli operazione')


    /* EXIT: Elimina nickname dalla lista e distruggi client */
    checkerror = pthread_mutex_lock(listaNicknames->mutex); // LOCK LISTNICKNAMES
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock insertOnHead %s\n", strerror(checkerror));

    listaNicknames->lista = listNicknames_deleteNode(listaNicknames->lista, thisClient->nickname);
    listaNicknames->numeroClientsServer -= 1;

    checkerror = pthread_mutex_unlock(listaNicknames->mutex); // UNLOCK LISTNICKNAMES
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock insertOnHead %s\n", strerror(checkerror));

    // Distruggi client ed esci
    fprintf(stderr, "Chiusura client %s nickname %s\n", thisClient->address, thisClient->nickname);
    checkerror = destroyClient(thisClient);
    if(checkerror != 0) fprintf(stderr, "Errore destroyClient %s\n", strerror(checkerror));
    return NULL;
}

/* Funzione di avvio thread: controlla se il client ha chiuso la connessione mentre era in attesa oppure ha interrotto l'attesa */
void *checkConnectionClient(void *arg)
{
    Client *thisClient = (Client*)arg;
    char buffer[2];
    int checkerror = 0;
    ssize_t bytesLetti = 0;

    /* Leggi dal client "OK" oppure "ST" */
    bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
    if(bytesLetti != 2 || (strncmp(buffer, "OK", 2) != 0 && strncmp(buffer, "ST", 2) != 0)){
        fprintf(stderr, "Errore checkConnectionClient %s\n", thisClient->address);
        thisClient->isConnected = false; // Il client si è disconnesso
    }

    /* Il client deciso di interrompere l'attesa */
    if(strncmp(buffer, "ST", 2) == 0)
    {
        thisClient->stopWaiting = true; // stop waiting client
        fprintf(stderr, "Il client %s ha interrotto l'attesa.\n", thisClient->nickname);
    } 

    /* Risveglia thread gestisciClient() associato a thisClient */
    fprintf(stderr,"Thread checkConnectionClient terminato, risveglia client %s\n", thisClient->nickname);
    checkerror = pthread_cond_signal(thisClient->cond);
    if(checkerror != 0) fprintf(stderr, "Errore condsignal checkConnectionClient %s\n", strerror(checkerror));

    return NULL;
}

/* Funzione di avvio thread: gestisce l'intera sessione chat tra due client accoppiati */
void *gestisciChat(void *arg)
{
    Match *pairClients = (Match*)arg;

    char buffer[1024];
    char stopMsg[5] = "/STOP"; // Messaggio: stop chat 
    char chatTimedoutMsg[5] = "TIMEC"; // Messaggio: chat timedout
    char chatInactiveMsg[5] = "IDLEC"; // Messaggio: chat inattiva
    int checkerror = 0;
    ssize_t bytesLetti = 0;
    ssize_t bytesScritti = 0;

    fd_set fds; // set file descriptor select
    int nfds; // numero file descriptor select
    struct timeval tv; // timeout select

    pthread_t TIDchatTimedout; // TID thread checkChatTimedout
    pthread_attr_t *attributi_thread;
    pthread_t *TIDthisThread; // TID di questo thread

    /* Risveglia i client accoppiati e aggiorna informazioni */
    checkerror = pthread_mutex_lock(pairClients->couplantClient1->mutex); // LOCK Client1
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock chat Client1 %s (%s)\n", pairClients->couplantClient1->nickname, strerror(checkerror));
    checkerror = pthread_mutex_lock(pairClients->couplantClient2->mutex); // LOCK Client2
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock chat Client2 %s (%s)\n", pairClients->couplantClient2->nickname, strerror(checkerror));

    // Aggiorna info CLIENT 1 CLIENT 2
    Client *Client1 = pairClients->couplantClient1;
    Client *Client2 = pairClients->couplantClient2;
    Client1->isMatched = true; 
    Client2->isMatched = true; 
    strncpy(Client1->matchedAddress, Client2->address, 15);
    strncpy(Client2->matchedAddress, Client1->address, 15);

    // RISVEGLIA CLIENT 1
    checkerror = pthread_cond_signal(Client1->cond); 
    if(checkerror != 0) fprintf(stderr, "Errore condsignal chat Client1 %s (%s)\n", Client1->nickname, strerror(checkerror));
    // ASPETTA CONFERMA CLIENT 1
    checkerror = pthread_cond_wait(Client1->cond, Client1->mutex); 
    if(checkerror != 0) fprintf(stderr, "Errore condwait chat Client1 %s (%s)\n", Client1->nickname, strerror(checkerror));
    // RISVEGLIA CLIENT 2
    checkerror = pthread_cond_signal(Client2->cond); 
    if(checkerror != 0) fprintf(stderr, "Errore condsignal chat Client2 %s (%s)\n", Client2->nickname, strerror(checkerror));
    // ASPETTA CONFERMA CLIENT 2
    checkerror = pthread_cond_wait(Client2->cond, Client2->mutex); 
    if(checkerror != 0) fprintf(stderr, "Errore condwait chat Client2 %s (%s)\n", Client2->nickname, strerror(checkerror));

    checkerror = pthread_mutex_unlock(Client1->mutex); // UNLOCK Client1
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock chat Client1 %s (%s)\n", Client1->nickname, strerror(checkerror));
    checkerror = pthread_mutex_unlock(Client2->mutex); // UNLOCK Client2
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock chat Client2 %s (%s)\n", Client2->nickname, strerror(checkerror));
    /* ------------------------------------------------------- */


    /* Invio nicknames del Client 1 al Client 2 e viceversa */
    // INVIA NICKNAME DI CLIENT 1 AL CLIENT 2
    bytesScritti = safeWrite(Client1->socketfd, Client2->nickname, 16);
    if(bytesScritti != 16){ // Errore: distruggi client e chiudi thread.
        fprintf(stderr, "Errore invio nickname Client 1 %s a Client 2%s.\n", Client1->nickname, Client2->nickname);
        Client1->isConnected = false;
    }

    // INVIA NICKNAME DI CLIENT 2 AL CLIENT 1
    bytesScritti = safeWrite(Client2->socketfd, Client1->nickname, 16);
    if(bytesScritti != 16){ // Errore: distruggi client e chiudi thread.
        fprintf(stderr, "Errore invio nickname Client 2 %s a Client 1 %s.\n", Client2->nickname, Client1->nickname);
        Client2->isConnected = false;
    }
    /* ----------------------------------------------------- */


    /* Determinazione numero massimo di file descriptor per la select */
    int sfd1 = Client1->socketfd;
    int sfd2 = Client2->socketfd;
    if(sfd1 >= sfd2) nfds = sfd1 + 1;
    else nfds = sfd2 + 1;

    /* Controlla se i due client sono ancora connessi */
    bool stopChat = false;
    if(Client1->isConnected && Client2->isConnected)
    {
        /* Inizializzazione attributi thread checkChatTimedout */
        attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
        if(attributi_thread == NULL) fprintf(stderr, "Errore allocazione attributi thread checkChatTimedout\n");

        checkerror = pthread_attr_init(attributi_thread);
        if(checkerror != 0) fprintf(stderr,"Errore pthreadinit checkStopWaiting.\n");
        checkerror = pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED);
        if(checkerror != 0) fprintf(stderr,"Errore pthreadsetdetachstate checkStopWaiting.\n");

        /* Creazione thread checkChatTimedout: dopo 60 secondi interrompe la chat comportando la ricerca di una nuova */
        TIDthisThread = (pthread_t*)malloc(sizeof(pthread_t)); 
        *TIDthisThread = pthread_self();
        checkerror = pthread_create(&TIDchatTimedout, attributi_thread, checkChatTimedout, TIDthisThread); // passaggio TID di questo thread
        if(checkerror != 0) fprintf(stderr,"Errore pthreadcreate checkStopWaiting.\n");

        fprintf(stderr, "Chat avviata: %s|%s.\n", Client1->nickname, Client2->nickname);
    }
    else // Almeno uno dei due client si è disconnesso, chiudi chat. 
    {
        stopChat = true;
    }

    /* Avvia chat */
    while(!stopChat && Client1->isConnected && Client2->isConnected)
    {
        FD_ZERO(&fds);
        FD_SET(sfd1, &fds); // socket file descriptor Client 1
        FD_SET(sfd2, &fds);  // socket file descriptor Client 2
    
        tv.tv_sec = 35; // circa 35 secondi massimo select

        /* SELECT con controllo */
        checkerror = select(nfds, &fds, NULL, NULL, &tv);
        if(checkerror <= 0) // ERRORE SELECT (Chat timedout, select timedout oppure ERRORE)
        {
            if(checkerror == 0) // Select timedout
            {
                fprintf(stderr,"SELECT timedout chat: %s|%s.\n", Client1->nickname, Client2->nickname);
                strncpy(buffer, chatInactiveMsg, 5); // setta messaggio chat inattiva
            } 
            else if(errno == EINTR) // Chat timedout
            {
                fprintf(stderr,"Chat timedout %s|%s.\n", Client1->nickname, Client2->nickname);
                strncpy(buffer, chatTimedoutMsg, 5); // setta messaggio chat timedout
                Client1->chatTimedout = true;
                Client2->chatTimedout = true;
            }
            else fprintf(stderr,"ERROR SELECT chat: %s|%s.\n", Client1->nickname, Client2->nickname);

            // scrivi messaggio di uscita al Client 1
            bytesScritti = safeWrite(sfd1, buffer, 1024);
            if(bytesScritti != 1024){ // Errore scrittura
                fprintf(stderr, "SELECT Errore chat scrittura client %s\n", Client1->address);
                Client1->isConnected = false;
            }
            // scrivi messaggio di uscita al Client 2
            bytesScritti = safeWrite(sfd2, buffer, 1024);
            if(bytesScritti != 1024){ // Errore scrittura
                fprintf(stderr, "SELECT Errore chat scrittura client %s\n", Client2->address);
                Client2->isConnected = false;
            }

            stopChat = true; // chiudi chat 
        }
        else // SELECT WAS SUCCESSFUL
        {
            for(int fd=0; fd < nfds; fd++)
            {
                if (FD_ISSET(fd, &fds))
                {
                    if(fd == sfd1) // CLIENT 1
                    {
                        /* Leggi messaggio dal Client 1 */
                        bytesLetti = safeRead(sfd1, buffer, 1024);

                        if(bytesLetti != 1024) // Errore READ oppure CHAT TIMEDOUT
                        { 
                            stopChat = true; // chiudi chat
                            memset(buffer, '\0', sizeof(buffer));
                            if(errno == EINTR) // Chat timedout 
                            {
                                strncpy(buffer, chatTimedoutMsg, 5); // setta messaggio chat timedout
                                // invia messaggio chat timedout al Client 1
                                bytesScritti = safeWrite(sfd1, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client1->address);
                                    Client1->isConnected = false;
                                }
                                // invia messaggio chat timedout al Client 2
                                bytesScritti = safeWrite(sfd2, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client2->address);
                                    Client2->isConnected = false;
                                }
                                // Chat timedout per client 1 e client 2
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break; // esci dal for
                            }   
                            else // Errore READ
                            {
                                fprintf(stderr, "[1c] Errore chat lettura client %s\n", Client1->address);
                                Client1->isConnected = false;
                                // invia messaggio di uscita al client 2
                                strncpy(buffer, stopMsg, 5); // setta messaggio di uscita
                            } 
                        }
                        else if(strncmp(buffer, "/STOP", 1024) == 0) // Lettura messaggio di STOP
                        {
                            // invia messaggio di uscita al client 2
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, stopMsg, 5); // setta messaggio di uscita
                            stopChat = true; // chiudi chat
                        }

                        /* Scrivi messaggio al Client 2 */
                        bytesScritti = safeWrite(sfd2, buffer, 1024);

                        if(bytesScritti != 1024) // Errore WRITE oppure CHAT TIMEDOUT
                        { 
                            stopChat = true; // chiudi chat
                            memset(buffer, '\0', sizeof(buffer));
                            if(errno == EINTR) // CHAT TIMEDOUT
                            {
                                strncpy(buffer, chatTimedoutMsg, 5); // setta messaggio chat timedout
                                // invia messaggio chat timedout al Client 1
                                bytesScritti = safeWrite(sfd1, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client1->address);
                                    Client1->isConnected = false;
                                }
                                // invia messaggio chat timedout al Client 2
                                bytesScritti = safeWrite(sfd2, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client2->address);
                                    Client2->isConnected = false;
                                }
                                // Chat timedout per client 1 e client 2
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break; // esci dal for
                            }   
                            else // Errore WRITE
                            {
                                fprintf(stderr, "[2c] Errore chat scrittura client %s\n", Client2->address);
                                Client2->isConnected = false;
                                if(Client1->isConnected){ // Se il client 1 è ancora connesso
                                    // scrivi messaggio di uscita al client 1
                                    strncpy(buffer, stopMsg, 5); // setta messaggio di uscita
                                    bytesScritti = safeWrite(sfd1, buffer, 1024);
                                    if(bytesScritti != 1024){ // Errore scrittura
                                        fprintf(stderr, "[3c] Errore chat scrittura client %s\n", Client2->address);
                                        Client1->isConnected = false;
                                    }
                                }
                            }
                            
                        }

                        memset(buffer, '\0', 1024);
                    }
                    else if(fd == sfd2) // CLIENT 2
                    {
                        /* Leggi messaggio dal Client 2 */
                        bytesLetti = safeRead(sfd2, buffer, 1024);

                        if(bytesLetti != 1024) // Errore READ oppure CHAT TIMEDOUT
                        {
                            stopChat = true; // chiudi chat
                            memset(buffer, '\0', sizeof(buffer));
                            if(errno == EINTR) // CHAT TIMEDOUT 
                            {
                                strncpy(buffer, chatTimedoutMsg, 5); // setta messaggio chat timedout
                                // invia messaggio chat timedout al Client 1
                                bytesScritti = safeWrite(sfd1, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client1->address);
                                    Client1->isConnected = false;
                                }
                                // invia messaggio chat timedout al Client 2
                                bytesScritti = safeWrite(sfd2, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client2->address);
                                    Client2->isConnected = false;
                                }
                                // Chat timedout per client 1 e client 2
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break; // esci dal for
                            }   
                            else // Errore READ
                            {
                                fprintf(stderr, "[4c] Errore chat lettura client %s\n", Client2->address);
                                Client2->isConnected = false;
                                // setta messaggio di uscita al client 1
                                strncpy(buffer, stopMsg, 5); 
                            } 
                            
                        }
                        else if(strncmp(buffer, "/STOP", 1024) == 0) // Lettura messaggio di STOP
                        {
                            // invia messaggio di uscita al client 1
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, stopMsg, 5); // setta messaggio di uscita
                            stopChat = true;
                        }

                        /* Scrivi messaggio al Client 1 */
                        bytesScritti = safeWrite(sfd1, buffer, 1024);

                        if(bytesScritti != 1024) // Errore WRITE oppure CHAT TIMEDOUT
                        { 
                            stopChat = true; // chiudi chat
                            memset(buffer, '\0', sizeof(buffer));
                            if(errno == EINTR) // CHAT TIMEDOUT
                            {
                                strncpy(buffer, chatTimedoutMsg, 5); // setta messaggio chat timedout
                                // invia messaggio chat timedout al Client 1
                                bytesScritti = safeWrite(sfd1, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client1->address);
                                    Client1->isConnected = false;
                                }
                                // invia messaggio chat timedout al Client 2
                                bytesScritti = safeWrite(sfd2, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "Errore chat scrittura client %s\n", Client2->address);
                                    Client2->isConnected = false;
                                }
                                // Chat timedout per client 1 e client 2
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break; // esci dal for
                            } 
                            else // Errore WRITE
                            {
                                fprintf(stderr, "[5c] Errore chat scrittura client %s\n", Client1->address);
                                Client1->isConnected = false;
                                if(Client2->isConnected){ // Se il client 2 è ancora connesso
                                    // scrivi messaggio di uscita al client 2
                                    strncpy(buffer, stopMsg, 5); // setta messaggio di uscita
                                    bytesScritti = safeWrite(sfd2, buffer, 1024);
                                    if(bytesScritti != 1024){ // Errore scrittura
                                        fprintf(stderr, "[6c] Errore chat scrittura client %s\n", Client2->address);
                                        Client2->isConnected = false;
                                    }
                                }
                            }  
                            
                        }
                        memset(buffer, '\0', 1024);
                    }
                }
            }
        }
    } 

    /* Cancella thread checkChatTimedout */
    if(!Client1->chatTimedout && !Client2->chatTimedout) // Se è vera questa condizione vuol dire che il thread checkChatTimedout è ancora in vita
    {
        fprintf(stderr, "Cancello thread checkChatTimedout.\n");
        checkerror = pthread_cancel(TIDchatTimedout);
        if(checkerror != 0) fprintf(stderr,"Errore pthreadcancel checkChatTimedout %s\n", strerror(checkerror));
    }

    free(attributi_thread);
    free(TIDthisThread);

    /* ESCI: aggiorna info e risveglia threads clients */
    fprintf(stderr, "Chat chiusa tra %s e %s.\n", Client1->nickname, Client2->nickname);
    Client1->isMatched = false;
    Client2->isMatched = false;
    checkerror = pthread_cond_signal(Client1->cond); // risveglia Thread Client 1
    if(checkerror != 0) fprintf(stderr, "Errore condsignal chat Client1 %s (%s)\n", Client1->address, strerror(checkerror));
    checkerror = pthread_cond_signal(Client2->cond); // risveglia Thread Client 2
    if(checkerror != 0) fprintf(stderr, "Errore condsignal chat Client2 %s (%s)\n", Client2->address, strerror(checkerror));
    return NULL;
}


/* Dopo 60 secondi chiude la chat aperta in gestisciChat() inviando un segnale SIGUSR1 al thread. 
   La ricezione del segnale da parte del thread gestisciChat provocherà l'interruzione della select, 
   read o write e in particolare provocherà l'interruzione della chat */
void *checkChatTimedout(void *arg)
{
    pthread_t tid = *((pthread_t*)arg);
    int checkerror = 0;

    sleep(61);
    fprintf(stderr,"Invio segnale SIGUSR1.\n");
    checkerror = pthread_kill(tid, SIGUSR1);
    if(checkerror != 0) fprintf(stderr,"Errore thread_kill %s\n", strerror(checkerror));

    return NULL;
}

/* Signal handler per SIGUSR 1: usato per interrompere la chat in gestisciChat() */
void chatTimedout(int sig)
{
    if(sig == SIGUSR1)
    {
        fprintf(stderr, "CHAT TIMEDOUT.\n");
    }
    
}

