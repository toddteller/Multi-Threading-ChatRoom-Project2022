#define _GNU_SOURCE // pthread_timedjoin_np()
#include "server.h"
#include <errno.h>
#include <signal.h>


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
void *checkConnectionClient(void *arg); // controlla se il client ha chiuso la connessione mentre è in attesa

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
    while(1);

    checkerror = destroyRoom(Stanza);
    checkT(checkerror, "Errore destroyRoom\n", 0);

    return NULL;
}

/* Funzione di avvio thread - client appena connesso */
void *gestisciClient(void *arg)
{
    Client *thisClient = (Client*)arg;
    int checkerror;

    fprintf(stderr, "Nuova connessione\n");
    fprintf(stderr, "Client: %d\n", thisClient->socketfd);
    fprintf(stderr, "Indirizzo: %s\n", thisClient->address);

    ssize_t bytesLetti, bytesScritti;
    char buffer[16];

    /* Lettura nickname del client. La lunghezza del nickname è di massimo
    16 caratteri. Il client deve inviare un messaggio di 16 bytes. Se il client
    viola questa condizione, la connessione verrà chiusa e il thread terminato. */
    impostaTimerSocket(thisClient->socketfd, 10); // aspetta max 10 secondi
    bytesLetti = safeRead(thisClient->socketfd, buffer, 16);
    if(bytesLetti != 16){ // Errore: distruggi client e chiudi thread.
        fprintf(stderr, "[0] Errore lettura client %s\n", thisClient->address);
        checkerror = destroyClient(thisClient);
        if(checkerror != 0) fprintf(stderr, "Errore destroyClient\n");
        return NULL;
    }
    impostaTimerSocket(thisClient->socketfd, 0); // elimina timer

    strncpy(thisClient->nickname, buffer, 16);
    fprintf(stderr, "Client %s Nickname: %s\n", thisClient->address, thisClient->nickname);

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
    int input; // Valore inserito dal client

    do{
        do {
            checkInput = false;
            input = -1;
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

            do {
                checkInput = false;
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
                stanzeServer[input].Q->numeroClients += 1; // aumento numero clients in coda
                checkerror = pthread_cond_signal(stanzeServer[input].cond); // risveglia stanza
                if(checkerror != 0) fprintf(stderr, "Errore condsignal stanzaServerQueue %s\n", strerror(checkerror));

                checkerror = pthread_mutex_unlock(stanzeServer[input].mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock stanzaServer %s\n", strerror(checkerror));

                /* Il server mette in attesa il client */
                checkerror = pthread_mutex_lock(thisClient->mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));

                // continua fino a quando il client non ha trovato un match o il client si è disconnesso
                while(!thisClient->isMatched && thisClient->isConnected){ 
                    checkerror = pthread_cond_wait(thisClient->cond, thisClient->mutex);
                    if(checkerror != 0){ // Errore*
                        fprintf(stderr, "[1] Errore condwait client %s\n", thisClient->address);
                        thisClient->isConnected = false;
                        break;
                    }
                }

                checkerror = pthread_mutex_unlock(thisClient->mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));

                if(!thisClient->isConnected) break; // Errore*: il client si è disconnesso o qualcosa è andato storto

                /* Il server invia un feedback al client "OK": chat avviata */
                bytesScritti = safeWrite(thisClient->socketfd, "OK", 2);
                if(bytesScritti != 2){ // Errore: distruggi client e chiudi thread.
                    fprintf(stderr, "[12] Errore scrittura 'FU' client %s\n", thisClient->address);
                    break;
                }

                /* Aspetta la terminazione del thread checkConnectionClient al MASSIMO PER 10 SECONDI */
                struct timespec ts;
                ts.tv_nsec = 0;
                ts.tv_sec = time(&ts.tv_sec);
                if(ts.tv_sec == (time_t)-1) fprintf(stderr, "Errore settaggio timer client %s\n", thisClient->address);
                ts.tv_sec += 10;

                checkerror = pthread_timedjoin_np(tid, NULL, &ts);
                if(checkerror != 0){ // Errore
                    fprintf(stderr,"Errore pthread_timedjoin_np client %s\n", thisClient->address);
                    break;
                }

                /* Il server mette in attesa il client fino alla fine della chat */
                checkerror = pthread_mutex_lock(thisClient->mutex); // LOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));

                while(thisClient->isMatched){
                    checkerror = pthread_cond_wait(thisClient->cond, thisClient->mutex);
                    if(checkerror != 0){ // Errore*
                        fprintf(stderr, "[2] Errore condwait client %s\n", thisClient->address);
                        thisClient->isConnected = false;
                        break;
                    }
                }

                checkerror = pthread_mutex_lock(thisClient->mutex); // UNLOCK
                if(checkerror != 0) fprintf(stderr, "Errore mutexlock Client %s\n", strerror(checkerror));

                if(!thisClient->isConnected) break; // Errore*: Il client si è disconnesso o qualcosa è andato storto
            }

        }
        else if(input == 2){ // Il client ha scelto di uscire dal server
            fprintf(stderr, "Il client %s esce dal server\n", thisClient->address);
        }
        else{
            fprintf(stderr, "Errore input client %s\n", thisClient->address);
        }
    }while(input == 1);


    /* EXIT: Elimina nickname dalla lista e distruggi client */
    checkerror = pthread_mutex_lock(listaNicknames->mutex); // LOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock insertOnHead %s\n", strerror(checkerror));

    listaNicknames->lista = listNicknames_deleteNode(listaNicknames->lista, thisClient->nickname);
    listaNicknames->numeroClientsServer -= 1;

    checkerror = pthread_mutex_unlock(listaNicknames->mutex); // UNLOCK
    if(checkerror != 0) fprintf(stderr, "Errore mutexlock insertOnHead %s\n", strerror(checkerror));

    // Distruggi client ed esci
    fprintf(stderr, "Chiusura client %s nickname %s\n", thisClient->address, thisClient->nickname);
    checkerror = destroyClient(thisClient);
    if(checkerror != 0) fprintf(stderr, "Errore destroyClient %s\n", strerror(checkerror));
    return NULL;
}

/* Controlla se il client ha chiuso la connessione mentre è in attesa */
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
    checkerror = pthread_cond_signal(thisClient->cond);
    if(checkerror != 0) fprintf(stderr, "Errore condsignal checkConnectionClient %s\n", strerror(checkerror));

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
