#include "server.h"

/* Variabili globali server */
AVLNicknames *alberoAVL_nicknameClients;        
Room stanzeServer[MAX_NUM_ROOMS];   
volatile sig_atomic_t serverON;     
int socketServer;                   

/* Signal handler */
void chatTimedout(int sig);
void serverOFF(int sig);

/* Funzioni di avvio threads */
void *gestisciRoom(void *arg);
void *gestisciClient(void *arg);
void *gestisciChat(void *arg);
void *checkConnectionClient(void *arg);
void *checkChatTimedout(void *arg);

int main(void)
{
    serverON = true;

    /* Impostazione signal handler */
     // > SIGUSR1 usato per interrompere una chat aperta nel thread gestisciChat() dopo TOT secondi
     // > SIGPIPE ignorato
     // > SIGINT usato per spegnere il server correttamente
    safe_signal(SIGUSR1, chatTimedout, "Errore signal(SIGUSR1, chatTimedout)");
    safe_signal(SIGPIPE, SIG_IGN, "Errore signal(SIGPIPE, SIG_IGN)");
    safe_signal(SIGINT, serverOFF, "Errore signal(SIGINT, serverOFF)");

    /* Inizializzazione attributi thread */
    pthread_attr_t *attributi_thread;
    attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
    if(attributi_thread == NULL) {fprintf(stderr, "Errore allocazione memoria attributi thread main\n"), pthread_exit(NULL);}
    pthread_cleanup_push(free, (void*)attributi_thread);

    safe_pthread_attr_init(attributi_thread, "Errore pthreadattrinit thread main");
    safe_pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED, "Errore setdetachstate main");

    /* Inizializzazione attributi mutex */
    pthread_mutexattr_t *attributi_mutex;
    attributi_mutex = (pthread_mutexattr_t*)malloc(sizeof(pthread_mutexattr_t));
    if(attributi_mutex == NULL) {fprintf(stderr, "Errore allocazione memoria mutexattr main\n"), pthread_exit(NULL);}
    pthread_cleanup_push(free, (void*)attributi_mutex);

    safe_pthread_mutexattr_init(attributi_mutex, "Errore pthraedmutexattrinit main");
    safe_pthread_mutexattr_settype(attributi_mutex, PTHREAD_MUTEX_ERRORCHECK, "Errore mutexattrsettype main");

    /* Inizializzazione struttura dati albero AVL nicknames di tutti i clients connessi */
    alberoAVL_nicknameClients = (AVLNicknames*)malloc(sizeof(AVLNicknames));
    if(alberoAVL_nicknameClients == NULL) {fprintf(stderr, "Errore allocazione memoria AVL nicknames\n"), pthread_exit(NULL);}
    pthread_cleanup_push(free, (void*)alberoAVL_nicknameClients);

    alberoAVL_nicknameClients->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if(alberoAVL_nicknameClients->mutex == NULL) {fprintf(stderr, "Errore allocazione memoria mutex AVL nicknames\n"), pthread_exit(NULL);}
    pthread_cleanup_push(free, (void*)alberoAVL_nicknameClients->mutex);

    safe_pthread_mutex_init(alberoAVL_nicknameClients->mutex, attributi_mutex, "Errore pthreadmutexinit AVL Nicknames");
    alberoAVL_nicknameClients->numeroClientsServer = 0;


    /* Definizione nome stanze server */
    static const char *ROOM_NAMES[MAX_NUM_ROOMS] = {
        "Food Room\n", "Music Room\n", "Sports Room\n", "Nerd Room\n", "Business Room\n",
    };       

    /* Definizione stanze e creazione threads stanze, funzione di avvio gestisciRoom() */
    pthread_t ROOM_THREAD_ID[MAX_NUM_ROOMS];

    for(int i=0; i<MAX_NUM_ROOMS; i++)
    {
            // Allocazione memoria coda stanza i-esima
            stanzeServer[i].coda = (Queue*)malloc(sizeof(Queue));
            if(stanzeServer[i].coda == NULL) {fprintf(stderr, "Errore allocazione memoria coda stanza %d\n", i), pthread_exit(NULL);}
            pthread_cleanup_push(free, (void*)stanzeServer[i].coda);

            // Allocazione memoria mutex stanza i-esima
            stanzeServer[i].mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
            if(stanzeServer[i].mutex == NULL) {fprintf(stderr, "Errore allocazione memoria mutex stanza %d\n", i), pthread_exit(NULL);}
            pthread_cleanup_push(free, (void*)stanzeServer[i].mutex);

            // Allocazione memoria cond stanza i-esima
            stanzeServer[i].cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
            if(stanzeServer[i].cond == NULL) {fprintf(stderr, "Errore allocazione memoria mutex stanza %d\n", i), pthread_exit(NULL);}
            pthread_cleanup_push(free, (void*)stanzeServer[i].cond);

            // Inizializzazione dati stanza i-esima
            stanzeServer[i].idRoom = i;
            stanzeServer[i].maxClients = MAX_CLIENTS_PER_ROOM;
            stanzeServer[i].numeroClients = 0;
            stanzeServer[i].coda->numeroClients = 0;
            memset(stanzeServer[i].roomName, '\0', sizeof(stanzeServer[i].roomName));
            strncpy(stanzeServer[i].roomName, ROOM_NAMES[i], 16);
            
            // Inizializzazione mutex e cond stanza i-esima
            safe_pthread_mutex_init(stanzeServer[i].mutex, attributi_mutex, "Errore pthreadmutexattrinit stanza");
            safe_pthread_cond_init(stanzeServer[i].cond, NULL, "Errore pthreadcondinit stanza");

            // Creazione thread i-esimo, funzione di avvio gestisciRoom()
            safe_pthread_create(&ROOM_THREAD_ID[i], attributi_thread, gestisciRoom, (void*)&stanzeServer[i], "Errore creazione thread gestisciRoom");

            pthread_cleanup_pop(0); // POP free(stanzeServer[i]->cond)  [not execute]
            pthread_cleanup_pop(0); // POP free(stanzeServer[i]->mutex) [not execute]
            pthread_cleanup_pop(0); // POP free(stanzeServer[i]->coda)  [not execute]
    }

    /* Inizializzazione dati client */
    struct sockaddr_in clientAddress;
    int socketClient, clientAddress_size;
    pthread_t TID_gestisciClient;
    char *clientAddress_convertedToDotFormatString;
    Client *newClient;
    clientAddress_size = sizeof(struct sockaddr_in);

    /* Setup e apertura connessione */
    socketServer = setupConnection(SERVER_PORT, SERVER_BACKLOG);

    /* Accetta connessioni */
    while(serverON)
    {
        fprintf(stderr, "> Server in ascolto, in attesa di connessioni...\n");

        socketClient = accept(socketServer, (struct sockaddr*)&clientAddress, (socklen_t*)&clientAddress_size);

        if(socketClient == SOCKET_ERROR)
        {
            if(errno == SERVER_OFF) break;  // SIGINT ricevuto
            else {pthread_kill(pthread_self(), SIGINT); break;}
        }

        clientAddress_convertedToDotFormatString = inet_ntoa(clientAddress.sin_addr);

        /* Inizializzazione client */
        newClient = (Client*)malloc(sizeof(Client));
        if(newClient == NULL) {fprintf(stderr, "Errore allocazione nuovo client\n"); pthread_kill(pthread_self(), SIGINT); break;}
        pthread_cleanup_push(free, (void*)newClient);

        newClient->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
        if(newClient->mutex == NULL) {fprintf(stderr, "Errore allocazione memoria mutex initClient()\n"); pthread_kill(pthread_self(), SIGINT); break;}
        pthread_cleanup_push(free, (void*)newClient->mutex);

        newClient->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
        if(newClient->cond == NULL){fprintf(stderr, "Errore allocazione memoria mutex initClient()\n"); pthread_kill(pthread_self(), SIGINT); break;}
        pthread_cleanup_push(free, (void*)newClient->cond);

        initClient(newClient, socketClient, clientAddress_convertedToDotFormatString, attributi_mutex);

        /* Creazione thread funzione di avvio gestisciClient() */
        safe_pthread_create(&TID_gestisciClient, attributi_thread, gestisciClient, (void*)newClient, "Errore creazione thread gestisciClient");

        pthread_cleanup_pop(0); // POP free(newClient->cond)  [not execute]
        pthread_cleanup_pop(0); // POP free(newClient->mutex) [not execute]
        pthread_cleanup_pop(0); // POP free(newClient)        [not execute]
    }

    /* Distruzione attributi thread */
    fprintf(stderr, "> Distruzione attributi thread\n");
    safe_pthread_attr_destroy(attributi_thread, "Errore pthreadattrdestroy main");

    /* Distruzione attributi mutex */
    fprintf(stderr, "> Distruzione attributi mutex\n");
    safe_pthread_mutexattr_destroy(attributi_mutex, "Errore pthreadmutexattrdestroy main");
    
    /* Distruzione AVL Nicknames */
    fprintf(stderr, "> Distruzione AVL Nicknames\n");
    alberoAVL_nicknameClients->root = alberoAVL_deleteTree(alberoAVL_nicknameClients->root);

    pthread_cleanup_pop(1); // free(alberoAVL_nicknameClients->mutex)
    pthread_cleanup_pop(1); // free(alberoAVL_nicknameClients)
    pthread_cleanup_pop(1); // free(attributi_mutex)
    pthread_cleanup_pop(1); // free(attributi_thread)

    sleep(1);
    fprintf(stderr, "\n!> Chiusura server <!\n");

    pthread_exit(NULL);
}


/* Funzione di avvio thread - gestione stanza di un server */
void *gestisciRoom(void *arg)
{
    Room *Stanza = (Room*)arg;
    pthread_cleanup_push(destroyRoom, (void*)Stanza);

    fprintf(stderr, "> Thread stanza %d avviato. Nome stanza: %s", Stanza->idRoom, Stanza->roomName);

    bool matchNotFound = false;

    do
    {
        /* Stanza in attesa di clients da mettere in comunicazione chat */
        safe_pthread_mutex_lock(Stanza->mutex, "Errore mutexlock stanza");
        pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)Stanza->mutex);

        while((Stanza->coda->numeroClients < 2 || matchNotFound) && serverON) 
        { 
            matchNotFound = false;
            fprintf(stderr, "> Stanza in attesa passiva: %s", Stanza->roomName);
            safe_pthread_cond_wait(Stanza->cond, Stanza->mutex, "Errore condwait stanza");
        }

        fprintf(stderr, "> Stanza uscita dall'attesa: %s", Stanza->roomName);

        if(serverON)
        { 
            fprintf(stderr, "> Cerca coppia client per una chat: %s", Stanza->roomName);

            Match *coupleClients = NULL;
            coupleClients = findPairClientsFromQueue(Stanza->coda, Stanza->coda->head->next, Stanza->coda->head);

            if(coupleClients == NULL)
            {
                fprintf(stderr, "> Coppia client non trovata: %s", Stanza->roomName);
                matchNotFound = true;
            }
            else 
            { 
                matchNotFound = false; 

                Client *Client1 = coupleClients->couplantClient1;
                Client *Client2 = coupleClients->couplantClient2;

                fprintf(stderr, "> Coppia client trovata: %s|%s. Stanza %s", Client1->nickname, Client2->nickname, Stanza->roomName);

                if(!Client1->isConnected && !Client2->isConnected)
                { 
                    fprintf(stderr, "> Client %s e Client %s si sono disconessi, ritorno in attesa stanza: %s", Client1->nickname, Client2->nickname, Stanza->roomName);
                    Client1->deletedFromQueue = true;   
                    Client2->deletedFromQueue = true;   
                    Stanza->numeroClients -= 2;      
                }
                else if(!Client1->isConnected)
                {     
                    fprintf(stderr, "> Client %s si è disconesso. Riaccoda Client %s, ritorno in attesa stanza: %s", Client1->nickname, Client2->nickname, Stanza->roomName);
                    Client1->deletedFromQueue = true;   
                    Stanza->numeroClients -= 1;            
                    enqueue(Stanza->coda, Client2);     
                }
                else if(!Client2->isConnected) 
                {    
                    fprintf(stderr, "> Client %s si è disconesso. Riaccoda Client %s, ritorno in attesa stanza: %s", Client2->nickname, Client1->nickname, Stanza->roomName);
                    Client2->deletedFromQueue = true;   
                    Stanza->numeroClients -= 1;            
                    enqueue(Stanza->coda, Client1);  
                }
                else // Entrambi i client sono connessi: avvio chat
                { 
                    fprintf(stderr, "> Coppia valida: %s|%s. Stanza %s", Client1->nickname, Client2->nickname, Stanza->roomName);

                    write_to_client(Client1, "OK", 2, NOT_EXIT_ON_ERROR, "Errore scrittura 'OK' Client 1 (Room)");
                    write_to_client(Client2, "OK", 2, NOT_EXIT_ON_ERROR, "Errore scrittura 'OK' Client 2 (Room)");
 
                    Client1->matchedRoom = Stanza->idRoom; 
                    Client2->matchedRoom = Stanza->idRoom;

                    /* Inizializzazione attributi thread gestisciChat() */
                    pthread_t TID;
                    pthread_attr_t *attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
                    if(attributi_thread == NULL) {fprintf(stderr, "Errore allocazione attributi thread stanza %d\n", Stanza->idRoom); pthread_exit(NULL);}
                    pthread_cleanup_push(free, (void*)attributi_thread);

                    safe_pthread_attr_init(attributi_thread, "Errore pthread_attr_init stanza");
                    safe_pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED, "Errore pthread_attr_setdetachstate stanza"); 

                    /* Creazione thread funzione di avvio gestisciChat() */
                    fprintf(stderr, "> Creazione thread gestisciChat tra %s|%s, stanza %s", Client1->nickname, Client2->nickname, Stanza->roomName);
                    safe_pthread_create(&TID, attributi_thread, gestisciChat, (void*)coupleClients, "Errore creazione thread gestisciChat");

                    pthread_cleanup_pop(1); // free(attributi_thread)
                }
            }
        }

        safe_pthread_mutex_unlock(Stanza->mutex, "Errore mutex_unlock stanza");      
        pthread_cleanup_pop(0); // POP pthread_mutex_unlock(Stanza->mutex) [not execute]

    }while(serverON);
    
    fprintf(stderr, "> Distruggi stanza %s", Stanza->roomName);
    pthread_cleanup_pop(1); // destroyRoom(Stanza)

    pthread_exit(NULL);
}


/* Funzione di avvio thread - gestione client connesso */
void *gestisciClient(void *arg)
{
    Client *thisClient = (Client*)arg;
    pthread_cleanup_push(destroyClient, (void*)thisClient);

    char buffer[16];

    fprintf(stderr, "> Nuova connessione. Client: %d, Indirizzo: %s\n", thisClient->socketfd, thisClient->address);

    /* Lettura nickname client */
    impostaTimerSocket(thisClient->socketfd, 20);
    memset(buffer, '\0', sizeof(buffer));
    read_from_client(thisClient, buffer, 16, EXIT_ON_ERROR, "[1] Errore lettura client");
    impostaTimerSocket(thisClient->socketfd, 0);

    /* Copia nickname client */
    strncpy(thisClient->nickname, buffer, 16);
    fprintf(stderr, "> Nickname nuovo client: %s\n", thisClient->nickname);

    /* Controlla se il nickname è già presente nel server */
    bool nicknameExists = false;
    int uniqueNumberNickname = 0;

    safe_pthread_mutex_lock(alberoAVL_nicknameClients->mutex, "Errore mutexlock alberoAVL_nicknameClients check exists");  
    pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)alberoAVL_nicknameClients->mutex);

    uniqueNumberNickname = sumOfCharactersStringASCIICode(thisClient->nickname);                              
    nicknameExists = alberoAVL_nicknameExists(alberoAVL_nicknameClients->root, uniqueNumberNickname);   

    safe_pthread_mutex_unlock(alberoAVL_nicknameClients->mutex, "Errore mutexunlock AVL_NICKNAMES check exists");    
    pthread_cleanup_pop(0);

    if(nicknameExists)
    {
        /* Invia "EX" per informare che il nickname è già presente nel server */
        write_to_client(thisClient, "EX", 2, EXIT_ON_ERROR, "[2] Errore scrittura 'EX' client");
        pthread_exit(NULL);
    }
    else  
    {
        write_to_client(thisClient, "OK", 2, EXIT_ON_ERROR, "[3] Errore scrittura 'OK' client");
    }

    /* Ricevi un feedback "OK" dal client */
    memset(buffer, '\0', sizeof(buffer));
    impostaTimerSocket(thisClient->socketfd, 20); 
    read_from_client(thisClient, buffer, 2, EXIT_ON_ERROR, "[4] Errore lettura feedback client");
    impostaTimerSocket(thisClient->socketfd, 0); 
    if(strncmp(buffer, "OK", 2) != 0)
        fprintf(stderr, "[4.1] 'OK' non ricevuto Client %s\n", thisClient->nickname),
        pthread_exit(NULL);

    /* Inserimento nickname nella struttura dati */
    safe_pthread_mutex_lock(alberoAVL_nicknameClients->mutex, "Errore mutexlock insert nickname");      
    pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)alberoAVL_nicknameClients->mutex);

    alberoAVL_nicknameClients->root = alberoAVL_insertNickname(alberoAVL_nicknameClients->root, thisClient->nickname, uniqueNumberNickname);
    alberoAVL_nicknameClients->numeroClientsServer += 1;

    safe_pthread_mutex_unlock(alberoAVL_nicknameClients->mutex, "Errore mutexunlock insert nickname");  
    pthread_cleanup_pop(0);

    /* Leggi operazione da effettuare dal client */
    do
    {
        bool validClientInput; 
        int inputEnteredClient = 0; 

        do
        {
            validClientInput = false;

            /* Ricevi '1' oppure '2' */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 20);
            read_from_client(thisClient, buffer, 1, NOT_EXIT_ON_ERROR, "[5] Errore lettura input client");
            impostaTimerSocket(thisClient->socketfd, 0); 

            if(!thisClient->isConnected) break;

            /* Controllo input client */
            if( strncmp("1", buffer, VISUALIZZA_STANZE) != 0 && 
                strncmp("2", buffer, ESCI_DAL_SERVER) != 0  )
            { 
                /* Invia "IN" se l'input non è valido */
                write_to_client(thisClient, "IN", 2, NOT_EXIT_ON_ERROR, "[6] Errore scrittura 'IN' client");

                if(!thisClient->isConnected) break;
            }
            else validClientInput = true; 

        }while(!validClientInput);

        if(!thisClient->isConnected) break;

        inputEnteredClient = buffer[0] - '0'; 

        write_to_client(thisClient, "OK", 2, NOT_EXIT_ON_ERROR, "[7] Errore scrittura 'OK' client");

        if(!thisClient->isConnected) break;

        /* Ricevi "OK" dal client */
        memset(buffer, '\0', sizeof(buffer));
        impostaTimerSocket(thisClient->socketfd, 20); 
        read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[8] Errore scrittura 'OK' client");
        impostaTimerSocket(thisClient->socketfd, 0); 
        if(strncmp(buffer, "OK", 2) != 0 || !thisClient->isConnected){
            fprintf(stderr, "[8.1] 'OK' non ricevuto Client %s\n", thisClient->nickname);
            thisClient->isConnected = false;
            break;
        }

        if(inputEnteredClient == VISUALIZZA_STANZE)
        {
            /* Invia il numero totale delle stanze al client */
            memset(buffer, '\0', sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%d", MAX_NUM_ROOMS);
            write_to_client(thisClient, buffer, 1, NOT_EXIT_ON_ERROR, "[9] Errore scrittura numero stanze client");

            if(!thisClient->isConnected) break;

            /* Ricevi "OK" dal client */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 20); 
            read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[10] Errore lettura feedback client");
            impostaTimerSocket(thisClient->socketfd, 0); 
            if(strncmp(buffer, "OK", 2) != 0 || !thisClient->isConnected){
                fprintf(stderr, "[10.1] 'OK' non ricevuto Client %s\n", thisClient->nickname);
                thisClient->isConnected = false;
                break;
            }

            for(int i=0; i<MAX_NUM_ROOMS; i++)
            {
                /* Invia nome stanza i-esima la client */
                safe_pthread_mutex_lock(stanzeServer[i].mutex, "Errore mutexlock stanzaServer gestisciClient");
                pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)stanzeServer[i].mutex);

                write_to_client(thisClient, stanzeServer[i].roomName, 16, NOT_EXIT_ON_ERROR, "[11] Errore scrittura nome stanza client");

                safe_pthread_mutex_unlock(stanzeServer[i].mutex, "Errore mutexunlock stanzaServer gestisciClient"); 
                pthread_cleanup_pop(0);

                if(!thisClient->isConnected) break;

                /* Ricevi "OK" dal client */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); 
                read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[12] Errore lettura feedback client");
                impostaTimerSocket(thisClient->socketfd, 0); 
                if(strncmp(buffer, "OK", 2) != 0 || !thisClient->isConnected){
                    fprintf(stderr, "[12.1] 'OK' non ricevuto Client %s\n", thisClient->nickname);
                    thisClient->isConnected = false;
                    break;
                }

                /* Invia numero massimo clients della stanza i-esima */
                memset(buffer, '\0', sizeof(buffer));
                snprintf(buffer, sizeof(buffer), "%d", stanzeServer[i].maxClients);

                safe_pthread_mutex_lock(stanzeServer[i].mutex, "Errore mutexlock stanzaServer gestisciClient");   
                pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)stanzeServer[i].mutex);

                write_to_client(thisClient, buffer, 1, NOT_EXIT_ON_ERROR, "[13] Errore scrittura numero massimo clients");

                safe_pthread_mutex_unlock(stanzeServer[i].mutex, "Errore mutexunlock stanzaServer gestisciClient"); 
                pthread_cleanup_pop(0);

                if(!thisClient->isConnected) break;

                /* Ricevi "OK" dal client */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); 
                read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[14] Errore lettura feedback client");
                impostaTimerSocket(thisClient->socketfd, 0); 
                if(strncmp(buffer, "OK", 2) != 0 || !thisClient->isConnected){
                    fprintf(stderr, "[14.1] 'OK' non ricevuto Client %s\n", thisClient->nickname);
                    thisClient->isConnected = false;
                    break;
                }

                /* Invia numero clients attualmente presenti nella stanza i-esima */
                memset(buffer, '\0', sizeof(buffer));
                snprintf(buffer, sizeof(buffer), "%d", stanzeServer[i].numeroClients);

                safe_pthread_mutex_lock(stanzeServer[i].mutex, "Errore mutexlock stanzaServer gestisciClient");     
                pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)stanzeServer[i].mutex);

                write_to_client(thisClient, buffer, 1, NOT_EXIT_ON_ERROR, "[15] Errore scrittura numero clients stanza");

                safe_pthread_mutex_unlock(stanzeServer[i].mutex, "Errore mutexunlock stanzaServer gestisciClient"); 
                pthread_cleanup_pop(0);

                if(!thisClient->isConnected) break;

                /* Ricevi "OK" dal client */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); 
                read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[15.1] Errore lettura feedback client");
                impostaTimerSocket(thisClient->socketfd, 0); 
                if(strncmp(buffer, "OK", 2) != 0 || !thisClient->isConnected){
                    fprintf(stderr, "[14.1] 'OK' non ricevuto Client %s\n", thisClient->nickname);
                    thisClient->isConnected = false;
                    break;
                }
            } 

            if(!thisClient->isConnected) break;

            write_to_client(thisClient, "OK", 2, NOT_EXIT_ON_ERROR, "[17] Errore scrittura 'OK' client");

            if(!thisClient->isConnected) break;

            /* Il client deve scegliere una stanza */
            validClientInput = false;

            do {
                /* Ricevi un numero tra 0 e MAX_NUM_ROOMS-1 per identificare una stanza */
                memset(buffer, '\0', sizeof(buffer));
                impostaTimerSocket(thisClient->socketfd, 20); 
                read_from_client(thisClient, buffer, 1, NOT_EXIT_ON_ERROR, "[18] Errore lettura inputEnteredClient numero stanza client");
                impostaTimerSocket(thisClient->socketfd, 0); 

                if(!thisClient->isConnected) break;

                inputEnteredClient = (buffer[0] - '0') - 1; // Salvataggio input client

                /* Controllo input client */
                if(inputEnteredClient >= 0 && inputEnteredClient <= MAX_NUM_ROOMS-1)
                {
                    validClientInput = true; 
                }
                else
                {
                    /* Invia "IN" se l'input non è valido */
                    write_to_client(thisClient, "IN", 2, NOT_EXIT_ON_ERROR, "[19] Errore scrittura 'IN' client");

                    if(!thisClient->isConnected) break;
                }

            } while(!validClientInput);

            if(!thisClient->isConnected) break;

            write_to_client(thisClient, "OK", 2, NOT_EXIT_ON_ERROR, "[20] Errore scrittura 'OK' client");

            if(!thisClient->isConnected) break;

            /* Ricevi "OK" dal client */
            memset(buffer, '\0', sizeof(buffer));
            impostaTimerSocket(thisClient->socketfd, 20); 
            read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[21] Errore lettura feedback client");
            impostaTimerSocket(thisClient->socketfd, 0);
            if(strncmp(buffer, "OK", 2) != 0 || !thisClient->isConnected){
                fprintf(stderr, "[21.1] 'OK' non ricevuto Client %s\n", thisClient->nickname);
                thisClient->isConnected = false;
                break;
            }

            /* Controlla se la stanza selezionata dal client è piena */
            bool roomIsFull = false;

            safe_pthread_mutex_lock(stanzeServer[inputEnteredClient].mutex, "Errore mutexlock stanzaServer");      
            pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)stanzeServer[inputEnteredClient].mutex);

            if(stanzeServer[inputEnteredClient].numeroClients == stanzeServer[inputEnteredClient].maxClients) roomIsFull = true;

            safe_pthread_mutex_unlock(stanzeServer[inputEnteredClient].mutex, "Errore mutexunlock stanzaServer" );
            pthread_cleanup_pop(0);

            if(roomIsFull) 
            {
                /* Invia "FU" al client: stanza piena */
                write_to_client(thisClient, "FU", 2, NOT_EXIT_ON_ERROR, "[23] Errore scrittura 'FU' client");

                if(!thisClient->isConnected) break;
            }
            else 
            {
                write_to_client(thisClient, "OK", 2, NOT_EXIT_ON_ERROR, "[24] Errore scrittura 'OK' client");

                if(!thisClient->isConnected) break;

                do
                {
                    /* Ricevi "OK" dal client */
                    memset(buffer, '\0', sizeof(buffer));
                    impostaTimerSocket(thisClient->socketfd, 20); 
                    read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[25] Errore lettura feedback client");
                    impostaTimerSocket(thisClient->socketfd, 0);
                    if(strncmp(buffer, "OK", 2) != 0 || (!thisClient->isConnected)){
                        fprintf(stderr, "[25.1] 'OK' non ricevuto Client %s\n", thisClient->nickname);
                        thisClient->isConnected = false;
                        break;
                    }

                    int STANZA_SCELTA_CLIENT = inputEnteredClient;

                    /* Creazione thread funzione di avvio checkConnectionClient() */
                    fprintf(stderr,"> Creazione thread checkConnectionClient client %s\n", thisClient->nickname);
                    pthread_t tid;
                    safe_pthread_create(&tid, NULL, checkConnectionClient, (void*)thisClient, "Errore creazione thread checkConnectionClient");

                    /* Inserimento thisClient nella coda della stanza scelta */
                    safe_pthread_mutex_lock(stanzeServer[STANZA_SCELTA_CLIENT].mutex, "Errore mutexlock stanzaServer inserimento in coda"); 
                    pthread_cleanup_push((void*)pthread_mutex_unlock, stanzeServer[STANZA_SCELTA_CLIENT].mutex);

                    enqueue(stanzeServer[STANZA_SCELTA_CLIENT].coda, thisClient);
                    fprintf(stderr, "> Client %s inserito in coda nella stanza %s", thisClient->nickname, stanzeServer[STANZA_SCELTA_CLIENT].roomName);

                    // Se l'ultima chat non è andata in timedout vuol dire che è la prima volta che entra in questa stanza
                    if(!thisClient->chatTimedout)
                        fprintf(stderr, "> Client %s aumenta numero clients stanza %s", thisClient->nickname, stanzeServer[STANZA_SCELTA_CLIENT].roomName),
                        stanzeServer[STANZA_SCELTA_CLIENT].numeroClients += 1;
                    
                    thisClient->chatTimedout = false;
                    thisClient->currentRoom = stanzeServer[STANZA_SCELTA_CLIENT].idRoom;

                    safe_pthread_cond_signal(stanzeServer[STANZA_SCELTA_CLIENT].cond, "Errore condsignal stanzaServerQueue"); 
            
                    safe_pthread_mutex_unlock(stanzeServer[STANZA_SCELTA_CLIENT].mutex, "Errore mutexunlock stanzaServer");        
                    pthread_cleanup_pop(0);

                    safe_pthread_mutex_lock((pthread_mutex_t*)thisClient->mutex, "Errore mutexlock Client");          
                    pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)thisClient->mutex);
                    
                    /* In attesa di una chat */
                    while(!thisClient->isMatched && thisClient->isConnected && !thisClient->stopWaiting)
                    {
                        fprintf(stderr,"> In attesa di un match, client %s.\n", thisClient->nickname); 
                        safe_pthread_cond_wait(thisClient->cond, thisClient->mutex, "[25] Errore condwait client");
                    }

                    safe_pthread_mutex_unlock(thisClient->mutex, "Errore mutexunlock stanzaServer");
                    pthread_cleanup_pop(0);

                    fprintf(stderr, "> Uscito dall'attesa, client %s.\n", thisClient->nickname);

                    /* Controlla se il client si è disconnesso oppure ha interrotto l'attesa */
                    if(!thisClient->isConnected) 
                    { 
                        fprintf(stderr, "> Client %s disconesso.\n", thisClient->nickname);

                        if(!thisClient->deletedFromQueue)
                        {
                            fprintf(stderr,"> Client %s disconnesso e non cancellato dalla coda -> cancella.\n", thisClient->nickname);
                            safe_pthread_mutex_lock(stanzeServer[STANZA_SCELTA_CLIENT].mutex, "Errore mutexlock stanzeServer client disconesso");
                            pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)stanzeServer[STANZA_SCELTA_CLIENT].mutex);

                            stanzeServer[inputEnteredClient].coda->head = deleteNodeQueue(stanzeServer[STANZA_SCELTA_CLIENT].coda, 
                                                                                    stanzeServer[STANZA_SCELTA_CLIENT].coda->head, 
                                                                                    NULL, thisClient->nickname);                                                          
                            thisClient->deletedFromQueue = true;

                            safe_pthread_mutex_unlock(stanzeServer[inputEnteredClient].mutex, "Errore mutexunlock stanzeServer client disconesso"); 
                            pthread_cleanup_pop(0);
                        }

                        // WAKE UP Thread gestisciChat()
                        safe_pthread_cond_signal(thisClient->cond, "Errore condsignal client->chat");

                        // Terminazione thread checkConnectionClient()
                        safe_pthread_join(tid, NULL, "Errore pthread_join client");

                        thisClient->deletedFromQueue = false;
                    }
                    else if(thisClient->stopWaiting)
                    {
                        fprintf(stderr, "> Client %s ha interrotto l'attesa: cancella client dalla coda.\n", thisClient->nickname);

                        /* Invia "ST" al client: il client ha interrotto l'attesa*/
                        fprintf(stderr, "Messaggio 'ST' sblocco inviato al client %s.\n", thisClient->nickname);
                        write_to_client(thisClient, "ST", 2, NOT_EXIT_ON_ERROR, "[26] Errore scrittura 'ST' client");

                        safe_pthread_mutex_lock(stanzeServer[inputEnteredClient].mutex, "Errore mutexlock stanzeServer client stopwaiting");
                        pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)stanzeServer[inputEnteredClient].mutex);

                        /* Elimina client dalla coda della stanza scelta */
                        stanzeServer[inputEnteredClient].coda->head = deleteNodeQueue(stanzeServer[inputEnteredClient].coda, 
                                                                                stanzeServer[inputEnteredClient].coda->head, 
                                                                                NULL, thisClient->nickname);                                                         
                        thisClient->deletedFromQueue = true;

                        safe_pthread_mutex_unlock(stanzeServer[inputEnteredClient].mutex, "Errore mutexunlock stanzeServer client stopwaiting"); 
                        pthread_cleanup_pop(0);

                        if(!thisClient->isConnected) break;

                        /* Terminazione checkConnectionClient() */       
                        safe_pthread_join(tid, NULL, "Errore pthread_join client");

                        thisClient->stopWaiting = false; 
                        thisClient->deletedFromQueue = false;
                    }
                    else /* Il client non si è disconnesso e non ha interrotto l'attesa */
                    {
                        write_to_client(thisClient, "OK", 2, NOT_EXIT_ON_ERROR, "[27] Errore scrittura 'OK' client");

                        if(!thisClient->isConnected) break;

                        /* Terminazione del thread checkConnectionClient */
                        fprintf(stderr,"Aspetto thread checkConnectionClient client %s.\n", thisClient->nickname);
                        safe_pthread_join(tid, NULL, "[28] Errore pthread_join client");

                        fprintf(stderr,"> Thread checkConnectionClient terminato client %s.\n", thisClient->nickname);

                        // WAKE UP Thread gestisciChat()
                        safe_pthread_cond_signal(thisClient->cond, "Errore condsignal client->chat");

                        safe_pthread_mutex_lock(thisClient->mutex, "Errore mutexlock Client");
                        pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)thisClient->mutex);

                        /* Rimani in attesa: chat avviata */
                        while(thisClient->isMatched && thisClient->isConnected)
                        {
                            fprintf(stderr,"> Client %s in attesa chat.\n", thisClient->nickname);
                            safe_pthread_cond_wait(thisClient->cond, thisClient->mutex, "[29] Errore condwait client");
                        }

                        safe_pthread_mutex_unlock(thisClient->mutex, "Errore mutexunlock Client"); 
                        pthread_cleanup_pop(0);
            
                        fprintf(stderr, "> Chat terminata per il client %s.\n", thisClient->nickname);

                        if(thisClient->isConnected)
                        {
                            fprintf(stderr, "> Client %s ancora connesso.\n", thisClient->nickname);

                            write_to_client(thisClient, "OK", 2, NOT_EXIT_ON_ERROR, "[30] Errore scrittura 'OK' client");

                            if(!thisClient->isConnected) break;

                            /* Ricevi "OK" dal client */
                            memset(buffer, '\0', sizeof(buffer));
                            impostaTimerSocket(thisClient->socketfd, 20); 
                            read_from_client(thisClient, buffer, 2, NOT_EXIT_ON_ERROR, "[31] Errore lettura feedback client");
                            impostaTimerSocket(thisClient->socketfd, 0); 
                            if(strncmp(buffer, "OK", 2) != 0 || !thisClient->isConnected){
                                fprintf(stderr, "[31] 'OK' non ricevuto Client %s\n", thisClient->nickname);
                                thisClient->isConnected = false;
                                break;
                            }
                        }
                    }
                }while(thisClient->isConnected && thisClient->chatTimedout && !thisClient->stopWaiting); 
            }
        }
        else if(inputEnteredClient == ESCI_DAL_SERVER)
        { 
            fprintf(stderr, "Il client %s esce dal server.\n", thisClient->nickname);
            thisClient->isConnected = false;
        }
        else /* ERROR case */
        {
            fprintf(stderr, "Errore inputEnteredClient client %s\n", thisClient->nickname);
            thisClient->isConnected = false;
        }

        /* Uscita client dalla stanza, decrementa numero clients stanza */
        if(thisClient->currentRoom != NO_ROOM) 
        {
            safe_pthread_mutex_lock(stanzeServer[thisClient->currentRoom].mutex, "Errore mutexlock stanze"); 
            pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)stanzeServer[thisClient->currentRoom].mutex);

            fprintf(stderr, "> Client %s è uscito dalla stanza %s", thisClient->nickname, stanzeServer[thisClient->currentRoom].roomName);
            stanzeServer[thisClient->currentRoom].numeroClients -= 1;
            
            safe_pthread_mutex_unlock(stanzeServer[thisClient->currentRoom].mutex, "Errore mutexunlock stanze");
            pthread_cleanup_pop(0);

	        thisClient->currentRoom = NO_ROOM; 
        }

    }while(thisClient->isConnected);

    /* EXIT: Elimina nickname dalla struttura dati e distruggi client */
    safe_pthread_mutex_lock(alberoAVL_nicknameClients->mutex, "Errore mutexlock deleteNicknameAVL"); 
    pthread_cleanup_push((void*)pthread_mutex_unlock, (void*)alberoAVL_nicknameClients->mutex);

    alberoAVL_nicknameClients->root = alberoAVL_deleteNickname(alberoAVL_nicknameClients->root, uniqueNumberNickname);
    alberoAVL_nicknameClients->numeroClientsServer -= 1;

    safe_pthread_mutex_unlock(alberoAVL_nicknameClients->mutex, "Errore mutexunlock insertAVL"); 
    pthread_cleanup_pop(0);

    pthread_cleanup_pop(1); // destroy(Client)

    pthread_exit(NULL);
}

/* Funzione di avvio thread: controlla se il client ha chiuso la connessione mentre era in attesa oppure ha interrotto l'attesa */
void *checkConnectionClient(void *arg)
{
    Client *thisClient = (Client*)arg;

    char buffer[2];
    ssize_t bytesLetti = 0;

    /* Leggi dal client "OK" oppure "ST" */
    bytesLetti = safeRead(thisClient->socketfd, buffer, 2);
    if(bytesLetti != 2 || (strncmp(buffer, "OK", 2) != 0 && strncmp(buffer, "ST", 2) != 0))
        fprintf(stderr, "Errore checkConnectionClient %s\n", thisClient->address),
        thisClient->isConnected = false; // Il client si è disconnesso

    if(strncmp(buffer, "ST", 2) == 0) /* Letto "ST": il client deciso di interrompere l'attesa */
        fprintf(stderr, "> Il client %s ha interrotto l'attesa.\n", thisClient->nickname),
        thisClient->stopWaiting = true;

    /* WAKE UP thread gestisciClient() associato a thisClient */
    fprintf(stderr,"> Thread checkConnectionClient terminato, risveglia client %s\n", thisClient->nickname);
    safe_pthread_cond_signal(thisClient->cond, "Errore condsignal checkConnectionClient\n");

    pthread_exit(NULL);
}

/* Funzione di avvio thread: gestisce l'intera sessione chat tra due client accoppiati */
void *gestisciChat(void *arg)
{
    Match *pairClients = (Match*)arg;
    Client *Client1 = pairClients->couplantClient1;
    Client *Client2 = pairClients->couplantClient2;

    char buffer[1024];

    char StopMsg[5] = "/STOP";          
    char ChatTimedoutMsg[5] = "TIMEC";  
    char ChatInactiveMsg[5] = "IDLEC";  

    int checkerror = 0;
    ssize_t bytesLetti = 0;
    ssize_t bytesScritti = 0;

    fd_set FileDescriptorSET; 
    int NumeroMassimoFDSelect; 
    struct timeval TempoMassimoWaitSelect; 

    
    safe_pthread_mutex_lock(Client1->mutex, "Errore mutexlock chat Client1");
    pthread_cleanup_push((void*)pthread_mutex_unlock, Client1->mutex);
    safe_pthread_mutex_lock(Client2->mutex, "Errore mutexlock chat Client2");
    pthread_cleanup_push((void*)pthread_mutex_unlock, Client2->mutex);

    /* Aggiorna info Client 1 e Client 2 */
    Client1->isMatched = true; 
    Client2->isMatched = true; 
    memset(Client1->matchedAddress, '\0', sizeof(Client1->matchedAddress));
    memset(Client2->matchedAddress, '\0', sizeof(Client1->matchedAddress));
    strncpy(Client1->matchedAddress, Client2->address, 15);
    strncpy(Client2->matchedAddress, Client1->address, 15);

    /* Risveglia gestisciClient() 1 e aspetta una conferma */
    safe_pthread_cond_signal(Client1->cond, "Errore condsignal chat Client1"); 
    safe_pthread_cond_wait(Client1->cond, Client1->mutex, "Errore condwait chat Client1"); 

    /* Risveglia gestisciClient 2 e aspetta una conferma */
    safe_pthread_cond_signal(Client2->cond, "Errore condsignal chat Client2"); 
    safe_pthread_cond_wait(Client2->cond, Client2->mutex, "Errore condwait chat Client2"); 

    safe_pthread_mutex_unlock(Client1->mutex, "Errore mutexunlock chat Client1");
    pthread_cleanup_pop(0);
    safe_pthread_mutex_unlock(Client2->mutex, "Errore mutexunlock chat Client2");
    pthread_cleanup_pop(0);

    pthread_cleanup_push((void*)pthread_cond_signal, (void*)Client1->cond);
    pthread_cleanup_push((void*)pthread_cond_signal, (void*)Client2->cond);

    /* Invio nicknames del Client 1 al Client 2 e viceversa */
    write_to_client(Client1, Client2->nickname, 16, NOT_EXIT_ON_ERROR, "Errore invio nickname Client 1 a Client 2");
    write_to_client(Client2, Client1->nickname, 16, NOT_EXIT_ON_ERROR, "Errore invio nickname Client 2 a Client 1");


    pthread_t *TIDthisThread;   
    pthread_t TIDchatTimedout;  
    pthread_attr_t *attributi_thread;

    bool stopChat = false;

    if(Client1->isConnected && Client2->isConnected)
    {
        /* Inizializzazione attributi thread checkChatTimedout */
        attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
        safe_pthread_attr_init(attributi_thread, "Errore pthreadinit checkStopWaiting.");
        safe_pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED, "Errore pthreadsetdetachstate checkStopWaiting");

        /* Creazione thread checkChatTimedout: dopo 60 secondi interrompe la chat comportando la ricerca di una nuova */
        TIDthisThread = (pthread_t*)malloc(sizeof(pthread_t));
        *TIDthisThread = pthread_self();

        safe_pthread_create(&TIDchatTimedout, attributi_thread, checkChatTimedout, TIDthisThread, "Errore pthreadcreate checkStopWaiting");

        fprintf(stderr, "Chat avviata: %s|%s.\n", Client1->nickname, Client2->nickname);
    }
    else stopChat = true;

    pthread_cleanup_push(free, (void*)attributi_thread);
    pthread_cleanup_push(free, (void*)TIDthisThread);

    /* Determinazione numero massimo di file descriptor per la select */
    int SocketFDClient1 = Client1->socketfd;
    int SocketFDClient2 = Client2->socketfd;
    if(SocketFDClient1 >= SocketFDClient2) NumeroMassimoFDSelect = SocketFDClient1 + 1;
    else NumeroMassimoFDSelect = SocketFDClient2 + 1;
    
    /* Avvia chat */
    while(!stopChat && Client1->isConnected && Client2->isConnected)
    {
        FD_ZERO(&FileDescriptorSET);
        FD_SET(SocketFDClient1, &FileDescriptorSET); 
        FD_SET(SocketFDClient2, &FileDescriptorSET);  
    
        TempoMassimoWaitSelect.tv_sec = 35;

        /* SELECT con controllo */
        checkerror = select(NumeroMassimoFDSelect, &FileDescriptorSET, NULL, NULL, &TempoMassimoWaitSelect);

        if(checkerror <= 0)
        {
            if(checkerror == SELECT_TIMEDOUT)
            {
                fprintf(stderr,"> SELECT TIMEDOUT: %s|%s.\n", Client1->nickname, Client2->nickname);
                strncpy(buffer, ChatInactiveMsg, 5);
            } 
            else if(errno == CHAT_TIMEDOUT)
            {
                fprintf(stderr,"> CHAT TIMEDOUT: %s|%s.\n", Client1->nickname, Client2->nickname);
                strncpy(buffer, ChatTimedoutMsg, 5); 
                Client1->chatTimedout = true;
                Client2->chatTimedout = true;
            }
            else
            {
                fprintf(stderr,"> ERROR SELECT chat: %s|%s.\n", Client1->nickname, Client2->nickname);
                strncpy(buffer, StopMsg, 5);
            }
                
            write_to_client(Client1, buffer, 1024, NOT_EXIT_ON_ERROR, "SELECT Errore chat scrittura client 1");
            write_to_client(Client2, buffer, 1024, NOT_EXIT_ON_ERROR, "SELECT Errore chat scrittura client 2");

            stopChat = true;
        }
        else // SELECT WAS SUCCESSFUL
        {
            for(int FileDescriptorReady=0; FileDescriptorReady<NumeroMassimoFDSelect; FileDescriptorReady++)
            {
                if(FD_ISSET(FileDescriptorReady, &FileDescriptorSET))
                {

                    if(FileDescriptorReady == SocketFDClient1)
                    {
                        /* Leggi messaggio dal Client 1 */
                        bytesLetti = safeRead(SocketFDClient1, buffer, 1024);

                        if(bytesLetti != 1024)
                        { 
                            stopChat = true;
                            memset(buffer, '\0', sizeof(buffer));

                            if(errno == CHAT_TIMEDOUT)
                            {
                                strncpy(buffer, ChatTimedoutMsg, 5);
                                write_to_client(Client1, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                write_to_client(Client2, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");  
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break;
                            }   
                            else // Error READ
                            {
                                fprintf(stderr, "[1c] Errore chat lettura client %s\n", Client1->address);
                                Client1->isConnected = false;
                                strncpy(buffer, StopMsg, 5);
                            } 
                        }
                        else if(strncmp(buffer, "/STOP", 1024) == 0)
                        {
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, StopMsg, 5);
                            stopChat = true;
                        }

                        /* Scrivi messaggio al Client 2 */
                        bytesScritti = safeWrite(SocketFDClient2, buffer, 1024);

                        if(bytesScritti != 1024)
                        { 
                            stopChat = true;
                            memset(buffer, '\0', sizeof(buffer));

                            if(errno == CHAT_TIMEDOUT)
                            {
                                strncpy(buffer, ChatTimedoutMsg, 5);
                                write_to_client(Client1, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                write_to_client(Client2, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break; 
                            }   
                            else // Error WRITE
                            {
                                fprintf(stderr, "[2c] Errore chat scrittura client %s\n", Client2->address);
                                Client2->isConnected = false;

                                if(Client1->isConnected)
                                {
                                    strncpy(buffer, StopMsg, 5);
                                    write_to_client(Client1, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                }
                            }
                            
                        }

                        memset(buffer, '\0', 1024);
                    }
                    else if(FileDescriptorReady == SocketFDClient2)
                    {
                        /* Leggi messaggio dal Client 2 */
                        bytesLetti = safeRead(SocketFDClient2, buffer, 1024);

                        if(bytesLetti != 1024) 
                        {
                            stopChat = true; 
                            memset(buffer, '\0', sizeof(buffer));

                            if(errno == CHAT_TIMEDOUT)  
                            {
                                strncpy(buffer, ChatTimedoutMsg, 5); 
                                write_to_client(Client1, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                write_to_client(Client2, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break;
                            }   
                            else // Error READ
                            {
                                fprintf(stderr, "[4c] Errore chat lettura client %s\n", Client2->address);
                                Client2->isConnected = false;
                                strncpy(buffer, StopMsg, 5); 
                            } 
                            
                        }
                        else if(strncmp(buffer, "/STOP", 1024) == 0) 
                        {
                            memset(buffer, '\0', sizeof(buffer));
                            strncpy(buffer, StopMsg, 5);
                            stopChat = true;
                        }

                        /* Scrivi messaggio al Client 1 */
                        bytesScritti = safeWrite(SocketFDClient1, buffer, 1024);

                        if(bytesScritti != 1024)
                        { 
                            stopChat = true;
                            memset(buffer, '\0', sizeof(buffer));

                            if(errno == CHAT_TIMEDOUT)
                            {
                                strncpy(buffer, ChatTimedoutMsg, 5);
                                write_to_client(Client1, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                write_to_client(Client2, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                Client1->chatTimedout = true; 
                                Client2->chatTimedout = true;
                                break; 
                            } 
                            else // Error WRITE
                            {
                                fprintf(stderr, "[5c] Errore chat scrittura client %s\n", Client1->address);
                                Client1->isConnected = false;

                                if(Client2->isConnected)
                                { 
                                    strncpy(buffer, StopMsg, 5);
                                    write_to_client(Client2, buffer, 1024, NOT_EXIT_ON_ERROR, "Errore chat scrittura client");
                                }
                            }  
                            
                        }
                        memset(buffer, '\0', 1024);
                    }
                }
            }
        }
    } 

    
    if(!Client1->chatTimedout && !Client2->chatTimedout)
    {   
        /* Cancella thread checkChatTimedout */
        fprintf(stderr, "> Cancello thread checkChatTimedout.\n");
        checkerror = pthread_cancel(TIDchatTimedout);
        if(checkerror != 0) fprintf(stderr,"Errore pthreadcancel checkChatTimedout %s\n", strerror(checkerror));
    }

    /* ESCI: aggiorna info e risveglia threads gestisciClient() */
    fprintf(stderr, "> Chat chiusa tra %s e %s.\n", Client1->nickname, Client2->nickname);
    Client1->isMatched = false;
    Client2->isMatched = false;

    pthread_cleanup_pop(1); // free(TIDthisThread);
    pthread_cleanup_pop(1); // free(attributi_thread);
    pthread_cleanup_pop(1); // pthread_cond_signal(Client2->cond)
    pthread_cleanup_pop(1); // pthread_cond_signal(Client1->cond)

    pthread_exit(NULL);
}


/* Funziona di avvio thread: dopo 60 secondi invia un segnale SIGUSR1 */
void *checkChatTimedout(void *arg)
{
    pthread_t tid = *((pthread_t*)arg);
    int checkerror = 0;

    sleep(61);
    fprintf(stderr,"> Invio segnale SIGUSR1.\n");
    checkerror = pthread_kill(tid, SIGUSR1);
    if(checkerror != 0) fprintf(stderr,"Errore thread_kill %s\n", strerror(checkerror));

    pthread_exit(NULL);
}

/* Signal handler per SIGUSR1: usato per interrompere la chat in gestisciChat() */
void chatTimedout(int sig)
{
    if(sig == SIGUSR1)
    {
        char *buffer = "CHAT TIMEDOUT\n";
        write(STDERR_FILENO, buffer, strlen(buffer));
    }
    
}

/* Signal handler per SIGINT: usato per spegnere il server */
void serverOFF(int sig)
{
    if(sig == SIGINT)
    {
        char *buffer = "\n!> SIGINT RICEVUTO <!\n";
        write(STDERR_FILENO, buffer, strlen(buffer));

        serverON = false;

        // WAKE UP stanze server gestisciRoom() threads
        for(int i=0; i<MAX_NUM_ROOMS; i++)
        {
            safe_pthread_cond_signal(stanzeServer[i].cond, "Errore condsignal serverOFF");
        }

        safe_close(socketServer, "Errore chiusura socket server");
    }
}
