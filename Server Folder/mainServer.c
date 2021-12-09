#include "server.h"
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_NUM_ROOMS 5 // Il numero massimo di stanze del server
#define MAX_CLIENTS_ROOM 6 // Il numero massimo di client per stanza
#define PORT 9898
#define SERVER_BACKLOG 10

/* Funzioni controllo errori */
void checkT(int valueToCheck, const char *s, int successValue); // per threads
void checkS(int valueToCheck, const char *s, int unsuccessValue); // per socket

/* Funzioni di avvio threads */
void *gestisciRoom(void *arg);
void *gestisciClient(void *arg);
void *gestisciChat(void *arg);
void *checkConnectionClient(void *arg); // controlla se il client ha chiuso la connessione mentre era in attesa

// Puntatore globale per accesso alla lista di nicknames dei clients del server
ListNicknames *listaNicknames;
Room stanzeServer[MAX_NUM_ROOMS]; // Array di stanze del server globale

int main(void)
{
    int checkerror = 0; // variabile per controllo errori

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
    checkT(checkerror, "Errore init attr thread main", 0);
    checkerror = pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED);
    checkT(checkerror, "Errore setdetachstate main", 0);

    /* Inizializzazione attributi mutex */
    pthread_mutexattr_t *attributi_mutex;
    attributi_mutex = (pthread_mutexattr_t*)malloc(sizeof(pthread_mutexattr_t));
    if(attributi_mutex == NULL){
        fprintf(stderr, "Errore allocazione mutexattr main\n");
        exit(EXIT_FAILURE);
    }
    checkerror = pthread_mutexattr_init(attributi_mutex);
    checkT(checkerror, "Errore mutexattrinit main", 0);
    checkerror = pthread_mutexattr_settype(attributi_mutex, PTHREAD_MUTEX_ERRORCHECK);
    checkT(checkerror, "Errore mutexattrsettype main", 0);

    /* Definizione stanze e creazione threads stanze, funzione di avvio gestisciRoom() */
    pthread_t threadID_room[MAX_NUM_ROOMS];
    for(int i=0; i<MAX_NUM_ROOMS; i++){
            // inizializzazione stanza i-esima
            checkerror = initRoom(&stanzeServer[i], i, roomName[i], MAX_CLIENTS_ROOM, 0, attributi_mutex);
            checkT(checkerror, "Errore initRoom main", 0);

            // creazione thread i-esimo, funzione di avvio gestisciRoom()
            checkerror = pthread_create(&threadID_room[i], attributi_thread, gestisciRoom, (void*)&stanzeServer[i]);
            checkT(checkerror, "Errore creazione thread gestisciRoom", 0);
    }

    /* Setup e apertura connessione */
    int socketServer;
    socketServer = setupConnection(PORT, SERVER_BACKLOG);
    checkS(socketServer, "Errore setup connection main", -1);

    /* Inizializzazione lista nicknames dei clients */
    listaNicknames = (ListNicknames*)malloc(sizeof(ListNicknames));
    if(listaNicknames == NULL){
        fprintf(stderr, "Errore allocazione lista nicknames\n");
        exit(EXIT_FAILURE);
    }
    checkerror = initListNicknames(listaNicknames, attributi_mutex);
    checkT(checkerror, "Errore initListNicknames", 0);

    /* Inizializzazione dati client */
    struct sockaddr_in indirizzoClient;
    int socketClient, indirizzo_size;
    pthread_t clientTID;
    char *indirizzoClient_str; // indirizzo client tipo stringa
    Client *newClient;
    indirizzo_size = sizeof(struct sockaddr_in);

    /* Accetta nuove connessioni */
    while(1){
        fprintf(stderr, "Server in ascolto, in attesa di connessioni...\n");

        // accetta nuova connessione
        socketClient = accept(socketServer, (struct sockaddr*)&indirizzoClient, (socklen_t*)&indirizzo_size);
        checkS(socketClient, "Errore accept new connection", -1);

        // conversione indirizzo in stringa formato dotted
        indirizzoClient_str = inet_ntoa(indirizzoClient.sin_addr);

        // inizializzazione nuovo client
        newClient = (Client*)malloc(sizeof(Client));
        if(newClient == NULL){
            fprintf(stderr, "Errore allocazione nuovo client\n");
            exit(EXIT_FAILURE);
        }
        checkerror = initClient(newClient, socketClient, indirizzoClient_str, attributi_mutex);
        checkT(checkerror, "Errore initClient main", 0);

        // creazione thread funzione di avvio gestisciClient()
        checkerror = pthread_create(&clientTID, attributi_thread, gestisciClient, (void*)newClient);
        checkT(checkerror, "Errore creazione thread gestisciClient", 0);
    }

    fprintf(stderr, "Chiusura server.\n");

    /* Distruzione attributi thread */
    checkerror = pthread_attr_destroy(attributi_thread);
    checkT(checkerror, "Errore threadattrdestroy main", 0);
    free(attributi_thread);

    /* Distruzione attributi mutex */
    checkerror = pthread_mutexattr_destroy(attributi_mutex);
    checkT(checkerror, "Errore mutexattrdestroy main", 0);
    free(attributi_mutex);

    /* Distruzione lista nicknames */
    checkerror = destroyListNicknames(listaNicknames);
    checkT(checkerror, "Errore destroyListNicknames", 0);

    /* Chiusura socket Server */
    checkerror = close(socketServer);
    checkS(checkerror, "Errore chiusura socket Server", -1);

    exit(EXIT_SUCCESS);
}


/* Funzione di avvio thread - stanza di un server */
void *gestisciRoom(void *arg)
{
    Room *Stanza = (Room*)arg;
    int checkerror = 0;

    fprintf(stderr, "Thread stanza avviato. Nome stanza: %s", Stanza->roomName);

    // utilizzata per evitare situazioni di dead lock
    bool notFound = false; // significato: non è stata trovata una coppia di client da mettere in comunicazione

    do
    {
        /* ROOM: Resta in attesa fino a quando la coda non contiene almeno due clients */
        checkerror = pthread_mutex_lock(Stanza->mutex); // LOCK
        if(checkerror != 0) fprintf(stderr, "Errore mutexlock room server %d\n", Stanza->idRoom);

        while(Stanza->Q->numeroClients < 2 || notFound){ 
            fprintf(stderr, "Stanza in attesa: %s", Stanza->roomName);
            notFound = false;
            checkerror = pthread_cond_wait(Stanza->cond, Stanza->mutex);
            if(checkerror != 0){ // Errore
                 fprintf(stderr, "Errore condwait room server %d\n", Stanza->idRoom);
                 checkerror = -1;
                 break;
            }
        }

        fprintf(stderr, "Stanza uscita dall'attesa: %s", Stanza->roomName);

        if(checkerror == -1){ // Si è verificato un errore
            fprintf(stderr,"Errore stanza %d, chiusura.\n", Stanza->idRoom);
            break; // Errore: checkerror == -1
        }
        else{ // Non si è verificato nessun errore

            /* Cerca coppia client */
            fprintf(stderr, "Cerca coppia client.\n");
            Match *matchFound = NULL; // coppia di client
            nodoQueue *head, *headprev, *actual, *actualprev;
            int count = 0;
            head = Stanza->Q->head;
            headprev = NULL;
            actual = Stanza->Q->head->next;
            actualprev = Stanza->Q->head;
        
            while(count < Stanza->Q->numeroClients-1 && matchFound == NULL)
            {
                matchFound = searchCouple(Stanza->Q, actual, actualprev, head, headprev);
                headprev = head;
                head = head->next;
                actualprev = actual;
                actual = actual->next;
                count += 1;
                printf("ciclo %d\n", count);
            }

            if(matchFound == NULL){ // Coppia client non trovata: ritorna in attesa
                notFound = true;
            }
            else{ // Coppia client trovata
                fprintf(stderr, "Coppia client trovata.\n");
                notFound = false;
                Client *Client1 = matchFound->couplantClient1;
                Client *Client2 = matchFound->couplantClient2;

                if(!Client1->isConnected && !Client2->isConnected){ // Entrambi i clients si sono disconnessi: ritorna in attesa
                    Client1->deletedFromQueue = true; // Client 1 cancellato dalla coda
                    Client2->deletedFromQueue = true; // Client 2 cancellato dalla coda
                    Stanza->numClients -= 2; // decrementa numero clients della stanza
                }
                else if(!Client1->isConnected){ // Client 1 si è disconnesso: ritorna in attesa
                    Client1->deletedFromQueue = true; // Client 1 cancellato dalla coda
                    Stanza->numClients -= 1; // decrementa numero clients della stanza
                    enqueue(Stanza->Q, Client2); // rimetti client 2 in coda
                }
                else if(!Client2->isConnected){ // Client 2 si è disconnesso: ritorna in attesa
                    Client2->deletedFromQueue = true; // Client 2 cancellato dalla coda
                    Stanza->numClients -= 1; // decrementa numero clients della stanza
                    enqueue(Stanza->Q, Client1); // rimetti client 1 in coda
                }
                else{ // Entrambi i client sono connessi: avvio chat

                    fprintf(stderr, "Coppia valida.\n");

                    /* Aggiorna matchedRoom dei clients */
                    Client1->matchedRoom_id = Stanza->idRoom;
                    Client2->matchedRoom_id = Stanza->idRoom;

                    /* Inizializzazione attributi thread gestisciChat() */
                    pthread_t tid;
                    pthread_attr_t *attributi_thread;
                    attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
                    checkerror = pthread_attr_init(attributi_thread);
                    checkerror = pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED);
                    if(attributi_thread == NULL || checkerror != 0){
                        fprintf(stderr, "Errore allocazione attributi thread stanza\n");
                        break;
                    }

                    /* Avvia thread chat */
                    checkerror = pthread_create(&tid, attributi_thread, gestisciChat, (void*)matchFound);
                    if(checkerror != 0) fprintf(stderr, "Errore avvio chat room\n");
                }
            }
        }

        checkerror = pthread_mutex_unlock(Stanza->mutex); // UNLOCK
        if(checkerror != 0) fprintf(stderr, "Errore mutexunlock room server %d\n", Stanza->idRoom);

    } while (checkerror != -1);
    
    /* Uscita: distruggi stanza ed esci */
    fprintf(stderr, "Distruggi stanza %s", Stanza->roomName);
    checkerror = destroyRoom(Stanza);
    checkT(checkerror, "Errore destroyRoom\n", 0);

    return NULL;
}

/* Funzione di avvio thread - client appena connesso */
void *gestisciClient(void *arg)
{
    Client *thisClient = (Client*)arg;
    int checkerror = 0;
    ssize_t bytesLetti = 0, bytesScritti = 0;
    char buffer[16];

    fprintf(stderr, "Nuova connessione, Client: %d, Indirizzo: %s\n", thisClient->socketfd, thisClient->address);

    /* Lettura nickname del client. La lunghezza del nickname è di massimo
    16 caratteri. Il client deve inviare un messaggio di 16 bytes. Se il client
    viola questa condizione, la connessione verrà chiusa e il thread terminato. */
    impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
    memset(buffer, '\0', sizeof(buffer));
    bytesLetti = safeRead(thisClient->socketfd, buffer, 16);
    if(bytesLetti != 16){ // Errore: distruggi client e chiudi thread.
        fprintf(stderr, "[0] Errore lettura client %s\n", thisClient->address);
        checkerror = destroyClient(thisClient);
        if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
        return NULL;
    }
    impostaTimerSocket(thisClient->socketfd, 0); // elimina timer

    /* Copia nickname */
    strncpy(thisClient->nickname, buffer, 16);
    fprintf(stderr, "Nickname: %s\n", thisClient->nickname);

    /* Controlla se il nickname è già presente nel server */
    bool nicknameExists = false;

    checkerror = pthread_mutex_lock(listaNicknames->mutex); // LOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock existingNickname %s\n", strerror(checkerror));

    nicknameExists = listNicknames_existingNickname(listaNicknames->lista, thisClient->nickname, 16);

    checkerror = pthread_mutex_unlock(listaNicknames->mutex); // UNLOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock existingNickname %s\n", strerror(checkerror));

    if(nicknameExists){
        /* Il server invia un feedback "EX" al client per indicare che il
        nickname è già presente nel server */
        bytesScritti = safeWrite(thisClient->socketfd, "EX", 2);
        if(bytesScritti != 2){ // Errore
            fprintf(stderr, "[0] Errore scrittura 'EX' client %s\n", thisClient->address);
        }
        checkerror = destroyClient(thisClient); // distruggi client e chiudi thead
        if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
        fprintf(stderr, "Client %s chiusura: nickname già esistente\n", thisClient->address);
        return NULL;
    }else{
        /* Il server invia un feedback al client "OK" */
        bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
        if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
            fprintf(stderr, "[0] Errore scrittura 'OK' client %s\n", thisClient->address);
            checkerror = destroyClient(thisClient);
            if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
            return NULL;
        }
    }

    /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
    questa condizione, la connessione viene chiusa e il thread terminato. */
    memset(buffer, '\0', sizeof(buffer));
    impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
    bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
    if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
        fprintf(stderr, "[1] Errore lettura feedback client %s\n", thisClient->address);
        checkerror = destroyClient(thisClient);
        if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
        return NULL;
    }
    impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

    /* Inserimento nickname thisClient nella lista dei nicknames */
    checkerror = pthread_mutex_lock(listaNicknames->mutex); // LOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock insertOnHead %s\n", strerror(checkerror));

    listaNicknames->lista = listNicknames_insertOnHead(listaNicknames->lista, thisClient->nickname, 16);
    listaNicknames->numeroClientsServer += 1;

    checkerror = pthread_mutex_unlock(listaNicknames->mutex); // UNLOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock insertOnHead %s\n", strerror(checkerror));

    bool checkInput; // Variabile controllo input client
    int input = 0; // Valore inserito dal client

    do{
        thisClient->actualRoom_id = -1; // per il momento il client non appartiene a nessuna stanza
        do {
            checkInput = false;
            /* Il server deve ricevere '1' oppure '2'. Corrispondono ad operazioni da eseguire. */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
            bytesLetti = safeRead(thisClient->socketfd, buffer, 1);
            if(bytesLetti != 1){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[2] Errore lettura input client %s\n", thisClient->address);
                break;
            }
            impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

            if(strncmp("1", buffer, 1) != 0 && strncmp("2", buffer, 1) != 0){ // Input non valido, riprova
                /* Il server invia al client "IN" se l'input non è valido */
                bytesScritti = safeWrite(thisClient->socketfd, "IN", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[2] Errore scrittura 'IN' client %s\n", thisClient->address);
                    break;
                }
            }
            else checkInput = true; // input valido
        } while(!checkInput);

        if(!checkInput) break; // Errore, esci e chiudi client

        /* Il server invia un feedback al client "OK" */
        bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
        if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
            fprintf(stderr, "[3] Errore scrittura 'OK' client %s\n", thisClient->address);
            break;
        }

        input = buffer[0] - '0'; // Salvataggio input inserito dal client

        /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
        questa condizione, la connessione viene chiusa e il thread terminato. */
        memset(buffer, '\0', sizeof(buffer));
        impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
        bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
        if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
            fprintf(stderr, "[3] Errore lettura feedback client %s\n", thisClient->address);
            break;
        }
        impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

        if(input == 1){ // Il client ha scelto di visualizzare le stanze e scegliere una stanza per avviare un chat

            /* Il server invia il numero totale delle stanze al client */
            memset(buffer, '\0', sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%d", MAX_NUM_ROOMS);

            bytesScritti = safeWrite(thisClient->socketfd, buffer, 1);
            if(bytesScritti != 1){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[4] Errore scrittura numero stanze client %s\n", thisClient->address);
                break;
            }

            /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
            questa condizione, la connessione viene chiusa e il thread terminato. */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
            bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
            if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[4] Errore lettura feedback client %s\n", thisClient->address);
                break;
            }
            impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

            /* Il server invia informazioni sulle stanze al client */
            for(int i=0; i<MAX_NUM_ROOMS; i++){

                // INVIO NOME STANZA I-ESIMA
                checkerror = pthread_mutex_lock(stanzeServer[i].mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                bytesScritti = safeWrite(thisClient->socketfd, stanzeServer[i].roomName, 16);
                if(bytesScritti != 16){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[5] Errore scrittura nome stanze %d -iesima client %s\n", i, thisClient->address);
                    checkInput = false; // Setta errore
                    break;
                }

                checkerror = pthread_mutex_unlock(stanzeServer[i].mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                questa condizione, la connessione viene chiusa e il thread terminato. */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[5] Errore lettura feedback client %s\n", thisClient->address);
                    checkInput = false; // Setta errore
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

                // INVIO NUMERO MASSIMO CLIENTS STANZA I-ESIMA
                memset(buffer, '\0', sizeof(buffer));
                snprintf(buffer, sizeof(buffer), "%d", stanzeServer[i].maxClients);

                checkerror = pthread_mutex_lock(stanzeServer[i].mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                bytesScritti = safeWrite(thisClient->socketfd, buffer, 1);
                if(bytesScritti != 1){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[6] Errore scrittura numero max stanza client %s\n", thisClient->address);
                    checkInput = false; // Setta errore
                    break;
                }

                checkerror = pthread_mutex_unlock(stanzeServer[i].mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                questa condizione, la connessione viene chiusa e il thread terminato. */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[6] Errore lettura feedback client %s\n", thisClient->address);
                    checkInput = false; // Setta errore
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

                // INVIO NUMERO CLIENTS ATTUALI STANZA I-ESIMA
                memset(buffer, '\0', sizeof(buffer));
                snprintf(buffer, sizeof(buffer), "%d", stanzeServer[i].numClients);

                checkerror = pthread_mutex_lock(stanzeServer[i].mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                bytesScritti = safeWrite(thisClient->socketfd, buffer, 1);
                if(bytesScritti != 1){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[7] Errore scrittura numero clients stanza client %s\n", thisClient->address);
                    checkInput = false; // Setta errore
                    break;
                }

                checkerror = pthread_mutex_unlock(stanzeServer[i].mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                questa condizione, la connessione viene chiusa e il thread terminato. */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[7] Errore lettura feedback client %s\n", thisClient->address);
                    checkInput = false; // Setta errore
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer
            } // end for

            if(!checkInput) break; // Errore

            /* Il server invia un feedback al client "OK" */
            bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
            if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[8] Errore scrittura 'OK' client %s\n", thisClient->address);
                break;
            }

            checkInput = false;
            do {
                /* Il server deve ricevere un numero tra 0 e MAX_NUM_ROOMS-1 per identificare una stanza */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); // aspetta max 10 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 1);
                if(bytesLetti != 1){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[8] Errore lettura feedback client %s\n", thisClient->address);
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

                input = buffer[0] - '0'; // Salvataggio input client
                /* Il server controlla l'input */
                if(input >= 0 && input <= MAX_NUM_ROOMS-1){
                    checkInput = true; // input valido
                }else{
                    /* Il server invia al client "IN" se l'input non è valido */
                    bytesScritti = safeWrite(thisClient->socketfd, "IN", 2);
                    if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                        fprintf(stderr, "[9] Errore scrittura 'IN' client %s\n", thisClient->address);
                        break;
                    }
                }
            } while(!checkInput);

            if(!checkInput) break; // Errore

            /* Il server invia un feedback al client "OK" */
            bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
            if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[10] Errore scrittura 'OK' client %s\n", thisClient->address);
                break;
            }

            input = (buffer[0] - '0') - 1; // input inserito dal client

            /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
            questa condizione, la connessione viene chiusa e il thread terminato. */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
            bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
            if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                fprintf(stderr, "[9] Errore lettura feedback client %s\n", thisClient->address);
                break;
            }
            impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

            /* Controlla se la stanza selezionata dal client è piena */
            bool isFull = false;
            checkerror = pthread_mutex_lock(stanzeServer[input].mutex); // LOCK
            if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

            if(stanzeServer[input].numClients == stanzeServer[input].maxClients) isFull = true;

            checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK
            if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

            /* Il server invia un feedback al client "OK" oppure "FU" */
            if(!isFull){ 
                /* Il server invia un feedback al client "OK": la stanza non è piena  */
                bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[11] Errore scrittura 'OK' client %s\n", thisClient->address);
                    break;
                }
            }
            else{
                /* Il server invia un feedback al client "FU": stanza piena */
                bytesScritti = safeWrite(thisClient->socketfd, "FU", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[12] Errore scrittura 'FU' client %s\n", thisClient->address);
                    break;
                }
            }

            /* Se la stanza non è piena il server continua, altrimenti ritorna all'inizio */
            if(!isFull){
                /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                questa condizione, la connessione viene chiusa e il thread terminato. */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
                bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[10] Errore lettura feedback client %s\n", thisClient->address);
                    break;
                }
                impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer

                /* Creazione thread checkConnectionClient(): controlla se il client chiude la connessione durante l'attesa */
                fprintf(stderr,"Thread checkConnectionClient creato, client %s.\n", thisClient->nickname);
                pthread_t tid;
                checkerror = pthread_create(&tid, NULL, checkConnectionClient, (void*)thisClient);
                if(checkerror != 0){
                    fprintf(stderr,"Client %s, errore creazione thread checkConnectionClient: %s\n", thisClient->address, strerror(checkerror));
                    break;
                }

                /* Inserimento thisClient nella coda della stanza scelta e risveglia stanza */
                checkerror = pthread_mutex_lock(stanzeServer[input].mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                enqueue(stanzeServer[input].Q, thisClient); // inserimento in coda
                fprintf(stderr, "Client %s inserito in coda.\n", thisClient->nickname);
                stanzeServer[input].numClients += 1; // aumento numero clients della stanza
                thisClient->actualRoom_id = stanzeServer[input].idRoom; // assegna actual room al client
                checkerror = pthread_cond_signal(stanzeServer[input].cond); // risveglia stanza
                if(checkerror != 0) fprintf(stderr, "Errore condsignal stanzaServerQueue %s\n", strerror(checkerror));

                checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                /* Il server mette in attesa questo thread (e il client) */
                checkerror = pthread_mutex_lock(thisClient->mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));
                
                // continua fino a quando il client non ha trovato un match o il client si è disconnesso
                while(!thisClient->isMatched && thisClient->isConnected){
                    fprintf(stderr,"In attesa di un match, client %s.\n", thisClient->nickname); 
                    checkerror = pthread_cond_wait(thisClient->cond, thisClient->mutex);
                    if(checkerror != 0){ // Errore*
                        fprintf(stderr, "[1] Errore condwait client %s\n", thisClient->address);
                        thisClient->isConnected = false;
                        break;
                    }
                }

                checkerror = pthread_mutex_unlock(thisClient->mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzaServer %s\n", strerror(checkerror));

                fprintf(stderr, "Uscito dall'attesa, client %s.\n", thisClient->nickname);
                /* Controlla se il client si è disconnesso */
                if(!thisClient->isConnected){ // Errore*: il client si è disconnesso o qualcosa è andato storto
                    fprintf(stderr, "Client %s non connesso.\n", thisClient->nickname);
                    // Se non è stato già cancellato, elimina il client che si è disconnesso dalla coda della stanza
                    if(!thisClient->deletedFromQueue){
                        fprintf(stderr,"Client %s disconnesso e non cancellato dalla coda\n", thisClient->nickname);
                        checkerror = pthread_mutex_lock(stanzeServer[input].mutex); // LOCK
                        if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzeServer client disconesso %s\n", strerror(checkerror));

                        // elimina client dalla coda
                        stanzeServer[input].Q->head = deleteNodeQueue(stanzeServer[input].Q, stanzeServer[input].Q->head, 
                                                                      stanzeServer[input].Q->tail, NULL, thisClient->nickname);
                        stanzeServer[input].numClients -=1; // decrementa numero clients stanza
                        thisClient->deletedFromQueue = true; // client cancellato dalla coda

                        checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK
                        if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanzeServer client disconesso %s\n", strerror(checkerror));
                    }

                    // Sveglia thread gestisciChat per eliminarlo 
                    checkerror = pthread_cond_signal(thisClient->cond); // risveglia stanza
                    if(checkerror != 0) fprintf(stderr, "Errore condsignal client->chat %s\n", strerror(checkerror));

                    checkerror = pthread_join(tid, NULL);
                    if(checkerror != 0){ // Errore
                        fprintf(stderr,"[1] Errore %s pthread_timedjoin_np client %s\n", strerror(checkerror), thisClient->address);
                    }
                    break;
                }

                /* Il server invia un feedback al client "OK": risveglia client, chat avviata */
                fprintf(stderr, "Messaggio 'OK' sblocco inviato al client %s.\n", thisClient->nickname);
                bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[13] Errore scrittura 'OK' client %s\n", thisClient->address);
                    break;
                }

                /* Aspetta la terminazione del thread checkConnectionClient */
                /* Il client dovrebbe inviare 'OK' per terminare il thread checkConnectionClient() */
                fprintf(stderr,"Aspetto thread checkConnectionClient client %s.\n", thisClient->nickname);

                checkerror = pthread_join(tid, NULL);
                if(checkerror != 0){ // Errore
                    fprintf(stderr,"[2] Errore pthread_join client %s\n", thisClient->address);
                    thisClient->isConnected = false;     
                }

                fprintf(stderr,"thread checkConnectionClient terminato client %s.\n", thisClient->nickname);

                /* Risveglia thread gestisciChat() associato */
                checkerror = pthread_cond_signal(thisClient->cond);
                if(checkerror != 0) fprintf(stderr, "Errore condsignal client->chat %s\n", strerror(checkerror));

                /* Il server mette in attesa il client fino alla fine della chat */
                fprintf(stderr, "Metti in attesa chat client %s.\n", thisClient->nickname);
                checkerror = pthread_mutex_lock(thisClient->mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));

                while(thisClient->isMatched && thisClient->isConnected){
                    fprintf(stderr,"Client %s in attesa.\n", thisClient->nickname);
                    checkerror = pthread_cond_wait(thisClient->cond, thisClient->mutex);
                    if(checkerror != 0){ // Errore*
                        fprintf(stderr, "[2] Errore condwait client %s\n", thisClient->address);
                        thisClient->isConnected = false;
                    }
                }
                fprintf(stderr,"Client %s uscito dall'attesa\n", thisClient->nickname);

                checkerror = pthread_mutex_unlock(thisClient->mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexunlock Client %s\n", strerror(checkerror));

                fprintf(stderr, "Chat terminata per il client %s.\n", thisClient->nickname);

                /* Il client è ancora connesso */
                if(thisClient->isConnected)
                {
                    fprintf(stderr, "Client ancora connesso %s\n", thisClient->nickname);
                    /* Il server invia un feedback al client "OK" */
                    bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
                    if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                        fprintf(stderr, "[14] Errore scrittura 'OK' client %s\n", thisClient->address);
                        thisClient->isConnected = false;
                    }

                    /* Il server deve ricevere un feedback "OK" dal client. Se il client viola
                        questa condizione, la connessione viene chiusa e il thread terminato. */
                    memset(buffer, '\0', sizeof(buffer));
                    impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
                    bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
                    if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){ // Errore: distruggi client e chiudi thread.
                        fprintf(stderr, "[16] Errore lettura feedback client %s\n", thisClient->address);
                        thisClient->isConnected = false;
                    }
                    impostaTimerSocket(thisClient->socketfd, 0); //  elimina timer
                }
            }
        }
        else if(input == 2) // Il client ha scelto di uscire dal server
        { 
            fprintf(stderr, "Il client %s esce dal server\n", thisClient->address);
            thisClient->isConnected = false;
        }
        else // error case
        {
            fprintf(stderr, "Errore input client %s\n", thisClient->address);
            thisClient->isConnected = false;
        }

        /* Uscita client dalla stanza, decrementa numero clients stanza */
        if(thisClient->actualRoom_id != -1)
        {
            checkerror = pthread_mutex_lock(stanzeServer[thisClient->actualRoom_id].mutex); // LOCK
            if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanze %s\n", strerror(checkerror));

            stanzeServer[thisClient->actualRoom_id].numClients -= 1;

            checkerror = pthread_mutex_unlock(stanzeServer[thisClient->actualRoom_id].mutex); // LOCK
            if(checkerror != 0) fprintf(stderr, "Errore mutexunlock stanze %s\n", strerror(checkerror));
        }

    }while(thisClient->isConnected);


    /* EXIT: Elimina nickname dalla lista e distruggi client */
    checkerror = pthread_mutex_lock(listaNicknames->mutex); // LOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock insertOnHead %s\n", strerror(checkerror));

    listaNicknames->lista = listNicknames_deleteNode(listaNicknames->lista, thisClient->nickname);
    listaNicknames->numeroClientsServer -= 1;

    checkerror = pthread_mutex_unlock(listaNicknames->mutex); // UNLOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexunlock insertOnHead %s\n", strerror(checkerror));

    // Distruggi client ed esci
    fprintf(stderr, "Chiusura client %s nickname %s\n", thisClient->address, thisClient->nickname);
    checkerror = destroyClient(thisClient);
    if(checkerror != 0) fprintf(stderr, "Errore destroyClient %s\n", strerror(checkerror));
    return NULL;
}

/* Controlla se il client ha chiuso la connessione mentre era in attesa */
void *checkConnectionClient(void *arg)
{
    Client *thisClient = (Client*)arg;
    char buffer[2];
    int checkerror = 0;
    ssize_t bytesLetti = 0;

    bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
    if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){
        fprintf(stderr, "Errore checkConnectionClient %s\n", thisClient->address);
        thisClient->isConnected = false;
    }

    /* Risveglia thread gestisciClient() associato a thisClient */
    fprintf(stderr,"Thread checkConnectionClient terminato, risveglia client %s\n", thisClient->nickname);
    checkerror = pthread_cond_broadcast(thisClient->cond);
    if(checkerror != 0) fprintf(stderr, "Errore condsignal checkConnectionClient %s\n", strerror(checkerror));

    return NULL;
}

void *gestisciChat(void *arg)
{
    Match *pairClients = (Match*)arg;
    char buffer[1024];
    char stop[5] = "/STOP";
    int checkerror = 0;
    ssize_t bytesLetti = 0;
    ssize_t bytesScritti = 0;
    fd_set fds; // set file descriptor select
    int nfds; // numero file descriptor select
    struct timeval tv; // timeout select
    tv.tv_usec = 0;

    Client *Client1 = pairClients->couplantClient1;
    Client *Client2 = pairClients->couplantClient2;
    
    /* Risveglia i client accoppiati e aggiorna informazioni */
    checkerror = pthread_mutex_lock(Client1->mutex); // LOCK Client1
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock chat Client1 %s (%s)\n", Client1->nickname, strerror(checkerror));
    checkerror = pthread_mutex_lock(Client2->mutex); // LOCK Client2
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock chat Client2 %s (%s)\n", Client2->nickname, strerror(checkerror));

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

    /* Determinazione numero massimo di file descriptor per la select */
    int sfd1 = Client1->socketfd;
    int sfd2 = Client2->socketfd;
    if(sfd1 >= sfd2) nfds = sfd1 + 1;
    else nfds = sfd2 + 1;

    /* Avvia chat */
    bool stopChat = false;
    if(Client1->isConnected && Client2->isConnected){
        fprintf(stderr, "Chat avviata: %s e %s.\n", Client1->nickname, Client2->nickname);
    }else {stopChat = true;}
    
    while(!stopChat && Client1->isConnected && Client2->isConnected)
    {
        FD_ZERO(&fds);
        FD_SET(sfd1, &fds);
        FD_SET(sfd2, &fds);  
    
        tv.tv_sec = 30; // 30 secondi massimo select

        /* SELECT con controllo */
        checkerror = select(nfds, &fds, NULL, NULL, &tv);
        if(checkerror <= 0) // errore select
        {
            if(checkerror != 0) fprintf(stderr,"Errore select\n");
            else fprintf(stderr,"select timedout\n");
            strncpy(buffer, stop, 5); // setta messaggio di uscita
            // scrivi messaggio di uscita al Client 1
            bytesScritti = safeWrite(sfd1, buffer, 1024);
            if(bytesScritti != 1024){ // Errore scrittura
                fprintf(stderr, "[select] Errore chat scrittura client %s\n", Client1->address);
                Client1->isConnected = false;
            }
            // scrivi messaggio di uscita al Client 2
            bytesScritti = safeWrite(sfd2, buffer, 1024);
            if(bytesScritti != 1024){ // Errore scrittura
                fprintf(stderr, "[1] Errore chat scrittura client %s\n", Client2->address);
                Client2->isConnected = false;
            }
            stopChat = true;
        }
        else // select
        {
            for(int fd=0; fd < nfds; fd++)
            {
                if (FD_ISSET(fd, &fds))
                {
                    if(fd == sfd1) // CLIENT 1
                    {
                        /* Leggi messaggio dal Client 1 */
                        bytesLetti = safeRead(sfd1, buffer, 1024);
                        fprintf(stderr, "Messaggio ricevuto da %s: %s", Client1->nickname, buffer);
                        if(bytesLetti != 1024){ // Errore lettura
                            fprintf(stderr, "[1] Errore chat lettura client %s\n", Client1->address);
                            Client1->isConnected = false;
                            // invia messaggio di uscita al client 2
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, stop, 5); // setta messaggio di uscita
                        }
                        else if(strncmp(buffer, "/STOP", 1024) == 0) // Lettura messaggio di stop
                        {
                            // invia messaggio di uscita al client 2
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, stop, 5); // setta messaggio di uscita
                            stopChat = true;
                        }

                        /* Scrivi messaggio al Client 2 */
                        bytesScritti = safeWrite(sfd2, buffer, 1024);
                        if(bytesScritti != 1024){ // Errore scrittura
                            fprintf(stderr, "[1] Errore chat scrittura client %s\n", Client2->address);
                            Client2->isConnected = false;
                            if(!stopChat && Client1->isConnected){ // Se il client 1 è ancora connesso e la chat ancora attiva
                                // scrivi messaggio di uscita al client 1
                                memset(buffer, '\0', sizeof(buffer));
                                strncpy(buffer, stop, 5); // setta messaggio di uscita
                                bytesScritti = safeWrite(sfd1, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "[1.1] Errore chat scrittura client %s\n", Client2->address);
                                    Client1->isConnected = false;
                                }
                            }
                        }

                        memset(buffer, '\0', 1024);
                    }
                    else if(fd == sfd2) // CLIENT 2
                    {
                        /* Leggi messaggio dal Client 2 */
                        bytesLetti = safeRead(sfd2, buffer, 1024);
                        fprintf(stderr, "Messaggio ricevuto da %s: %s", Client2->nickname, buffer);
                        if(bytesLetti != 1024){ // Errore lettura
                            fprintf(stderr, "[1] Errore chat lettura client %s\n", Client2->address);
                            Client2->isConnected = false;
                            // setta messaggio di uscita al client 1
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, stop, 5);
                        }
                        else if(strncmp(buffer, "/STOP", 1024) == 0) // Lettura messaggio di stop
                        {
                            // invia messaggio di uscita al client 1
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, stop, 5); // setta messaggio di uscita
                            stopChat = true;
                        }

                        /* Scrivi messaggio al Client 1*/
                        bytesScritti = safeWrite(sfd1, buffer, 1024);
                        if(bytesScritti != 1024){ // Errore scrittura
                            fprintf(stderr, "[2] Errore chat scrittura client %s\n", Client1->address);
                            Client1->isConnected = false;
                            if(!stopChat && Client2->isConnected){ // Se il client 2 è ancora connesso e la chat ancora attiva
                                // scrivi messaggio di uscita al client 2
                                memset(buffer, '\0', sizeof(buffer));
                                strncpy(buffer, stop, 5); // setta messaggio di uscita
                                bytesScritti = safeWrite(sfd2, buffer, 1024);
                                if(bytesScritti != 1024){ // Errore scrittura
                                    fprintf(stderr, "[2.1] Errore chat scrittura client %s\n", Client2->address);
                                    Client2->isConnected = false;
                                }
                            }
                        }

                        memset(buffer, '\0', 1024);
                    }
                }
            }
        }
    } 

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

/* Funzione controllo errori */
void checkT(int valueToCheck, const char *s, int successValue){
    if(valueToCheck != successValue){
        fprintf(stderr, "%s: %s\n", s, strerror(valueToCheck));
        exit(EXIT_FAILURE);
    }
}

/* Funzione controllo errori */
void checkS(int valueToCheck, const char *s, int unsuccessValue){
    if(valueToCheck == unsuccessValue){
        perror(s);
        exit(EXIT_FAILURE);
    }
}
