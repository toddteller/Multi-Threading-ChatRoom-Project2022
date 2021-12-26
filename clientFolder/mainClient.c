#include "client.h"

/* Funzioni di avvio threads */
void* checkStopWaiting(void *arg);

int main(int argc, char **argv)
{
    /* Controllo parametri input */
    if(argc!=4) fprintf(stderr, "Uso corretto: %s <indirizzo> <port> <nickname>\n", argv[0]), exit(EXIT_FAILURE);
    if(strlen(argv[3]) > 16) fprintf(stderr, "Lunghezza massima nickname: 16 caratteri\n"), exit(EXIT_FAILURE);

    /* Inizializzazione variabili */
    int status = 0;
    char *buffer;
    buffer = (char*)calloc(16,sizeof(char));
    if(buffer == NULL)
        fprintf(stderr,"Errore malloc buffer\n"),
        exit(EXIT_FAILURE);
    size_t buffer_size = 16;
    char nickname[18];
    memset(nickname, '\0', 18);
    strncpy(nickname, argv[3], 16);

    /* Connessione server */
    unsigned short int port;
    int socketfdserver;
    struct sockaddr_in indirizzo;

    inet_aton(argv[1], &indirizzo.sin_addr);
    port = atoi(argv[2]);
    socketfdserver = setupConnectionAndConnect(indirizzo, port);
    printf("> Connesso correttamente al server\n\n");

    /* Invio nickname al server */
    write_to_server(socketfdserver, nickname, 16, "Errore scrittura nickname");

    /* Cattura feedback dal server ("OK" oppure "EX") */
    memset(buffer, '\0', buffer_size);
    read_from_server(socketfdserver, buffer, 2, "Errore lettura feedback"); 

    if(strncmp(buffer, "EX", 2) == 0){ // "EX": nickname già esistente
        fprintf(stderr, "Il nickname inserito esiste già!\n");
        close(socketfdserver);
        exit(EXIT_FAILURE);
    }
    else if(strncmp(buffer, "OK", 2) != 0){
        close(socketfdserver);
        fprintf(stderr, "Errore lettura feedback\n");
        exit(EXIT_FAILURE);
    }

    write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

    /* Invio operazione ("1" oppure "2") al server */
    int inputOperazione;

    do 
    {   
        fd_set FileDescriptorSET;
        struct timeval TempoMassimoWaitSelect;
        TempoMassimoWaitSelect.tv_sec = 20;
        TempoMassimoWaitSelect.tv_usec = 0;

        bool checkInput;
        do {
            do{
                checkInput = false;

                FD_ZERO(&FileDescriptorSET);
                FD_SET(STDIN_FILENO, &FileDescriptorSET);

                memset(buffer, '\0', buffer_size);
                menuPrincipaleUI(nickname);

                status = select(1, &FileDescriptorSET, NULL, NULL, &TempoMassimoWaitSelect);
                if(status == SELECT_TIMEDOUT)
                {
                    fprintf(stderr, "\n!!> Connessione scaduta <!!\n");
                    exit(EXIT_FAILURE);
                }

                getline(&buffer, &buffer_size, stdin);

                if(strlen(buffer) == 2 && (strncmp("1", buffer, 1) == 0 || strncmp("2", buffer, 1) == 0)) 
                {
                    checkInput = true;
                    printf("\n");
                }
                else
                {
                    printf("Input errato, riprova.\n");
                }
            }while(!checkInput);

            inputOperazione = buffer[0] - '0';

            write_to_server(socketfdserver, buffer, 1, "Errore scrittura operazione");

            /* Cattura feedback dal server ("OK" oppure "IN") */
            memset(buffer, '\0', buffer_size);
            read_from_server(socketfdserver, buffer, 2, "Connessione scaduta");

            if(strncmp(buffer, "IN", 2) == 0){ // "IN": input errato
                checkInput = false;
                printf("Input errato, riprova.\n");
            }
            else if(strncmp(buffer, "OK", 2) != 0){
                close(socketfdserver);
                fprintf(stderr, "Errore lettura feedback\n");
                exit(EXIT_FAILURE);
            }

        } while(!checkInput);

        write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

        if(inputOperazione == VISUALIZZA_STANZE)
        { 
            printf("\n> Hai scelto di visualizzare le stanze del server per cercare una chat.\n");
            sleep(2);
            cleanSTDIN();
            system("clear");

            // LETTURA NUMERO STANZE
            memset(buffer, '\0', buffer_size);
            read_from_server(socketfdserver, buffer, 1, "Errore lettura feedback");

            int numeroStanze = atoi(buffer);
            Room Rooms[numeroStanze];

            write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

            /* Lettura informazioni stanze */
            for(int i=0; i<numeroStanze; i++)
            {
                // LETTURA NOME STANZA I-ESIMA
                memset(buffer, '\0', buffer_size);
                read_from_server(socketfdserver, buffer, 16, "Errore lettura feedback");
                
                strncpy(Rooms[i].roomName, buffer, 16);

                write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

                // LETTURA NUMERO MASSIMO CLIENTS STANZA I-ESIMA
                memset(buffer, '\0', buffer_size);
                read_from_server(socketfdserver, buffer, 1, "Errore lettura feedback");

                Rooms[i].maxClients = atoi(buffer);

                write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

                // LETTURA NUMERO CLIENTS ATTUALI STANZA I-ESIMA
                memset(buffer, '\0', buffer_size);
                read_from_server(socketfdserver, buffer, 1, "Errore lettura feedback");

                Rooms[i].numeroClients = atoi(buffer);

                write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");
            }

            memset(buffer, '\0', buffer_size);
            read_from_server(socketfdserver, buffer, 2, "Errore lettura feedback");
            
            if(strncmp(buffer, "OK", 2) != 0){
                close(socketfdserver);
                fprintf(stderr, "Errore lettura feedback\n");
                exit(EXIT_FAILURE);
            }

            stampaStanzeUI(Rooms, numeroStanze);
            cleanSTDIN();

            /* Scelta della stanza */
            int inputStanza;
            do 
            {
                TempoMassimoWaitSelect.tv_sec = 20;
                TempoMassimoWaitSelect.tv_usec = 0;

                do 
                {
                    checkInput = false;

                    FD_ZERO(&FileDescriptorSET);
                    FD_SET(STDIN_FILENO, &FileDescriptorSET);

                    memset(buffer, '\0', buffer_size);
                    printf("> Seleziona una stanza (inserisci un numero da 1 a %d)\n", numeroStanze);
                    printf("(20 secondi prima che la connessione venga chiusa)\n");

                    status = select(1, &FileDescriptorSET, NULL, NULL, &TempoMassimoWaitSelect);

                    if(status == SELECT_TIMEDOUT)
                    {
                        fprintf(stderr, "\n!!> Connessione scaduta <!!\n");
                        exit(EXIT_FAILURE);
                    }
                    else if(status < 0)
                    {
                        fprintf(stderr, "Errore select\n");
                        exit(EXIT_FAILURE);
                    }

                    getline(&buffer, &buffer_size, stdin);

                    /* Controllo input */
                    if(strlen(buffer) == 2 && (strncmp("1", buffer, 1) == 0 ||
                                               strncmp("2", buffer, 1) == 0 ||
                                               strncmp("3", buffer, 1) == 0 ||
                                               strncmp("4", buffer, 1) == 0 ||
                                               strncmp("5", buffer, 1) == 0)) {
                        checkInput = true;
                    }
                    else
                    {
                        printf("Input errato, riprova.\n");
                    }
                } while(!checkInput);

                /* Scrittura input stanza al server */
                write_to_server(socketfdserver, buffer, 1, "Errore scrittura input stanza.");

                inputStanza = (buffer[0] - '0') - 1; 

                /* Cattura feedback dal server ("OK" oppure "IN") */
                memset(buffer, '\0', buffer_size);
                read_from_server(socketfdserver, buffer, 2, "Errore chiusura socket");

                if(strncmp(buffer, "IN", 2) == 0){ // "IN": input errato
                    checkInput = false;
                    printf("Input errato, riprova.\n");
                }
                else if(strncmp(buffer, "OK", 2) != 0){
                    close(socketfdserver);
                    fprintf(stderr, "Errore lettura feedback\n");
                    exit(EXIT_FAILURE);
                }

            } while(!checkInput);

            write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

            bool roomIsFull = false;

            /* Controlla se la stanza è piena */
            memset(buffer, '\0', buffer_size);
            read_from_server(socketfdserver, buffer, 2, "Errore lettura feedback");
        
            if(strncmp(buffer, "FU", 2) == 0){ // "FU": stanza piena
                printf("La stanza selezionata è piena.\n");
                roomIsFull = true;
            }
            else if(strncmp(buffer, "OK", 2) != 0){
                close(socketfdserver);
                fprintf(stderr, "Errore lettura feedback\n");
                exit(EXIT_FAILURE);
            }

            if(!roomIsFull)
            {
                bool chatTimedout;

                do
                {
                    chatTimedout = false;

                    write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

                    /* Inizializzazione attributi thread checkStopWaiting */
                    pthread_attr_t *attributi_thread;
                    attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
                    if(attributi_thread == NULL){
                        fprintf(stderr, "Errore allocazione attributi thread\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_attr_init(attributi_thread);
                    pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED);
                    
                    /* Creazione thread checkStopWaiting: controlla se il client digita STOP per interrompere l'attesa */
                    pthread_t TIDstopWait;
                    pthread_create(&TIDstopWait, attributi_thread, checkStopWaiting, (void*)&socketfdserver);
                
                    printf("\n> Sei in attesa di una chat nella stanza %s...\n", Rooms[inputStanza].roomName);
                    printf("  (Per interrompere l'attesa digitare 'STOP')\n");

                    /* Cattura feedback dal server ('OK' oppure 'ST') */
                    memset(buffer, '\0', buffer_size);
                    read_from_server(socketfdserver, buffer, 2, "Errore lettura feedback");

                    if((strncmp(buffer, "OK", 2) != 0 && strncmp(buffer, "ST", 2) != 0)){
                        close(socketfdserver);
                        fprintf(stderr, "Errore lettura feedback\n");                   
                        exit(EXIT_FAILURE);
                    }
                    
                    free(attributi_thread);

                    if(strncmp(buffer, "ST", 2) == 0) // "ST": Attesa interrotta
                    {
                        printf("\n> Hai interrotto l'attesa. Ritorno al menu principale.\n");
                        sleep(2);
                        cleanSTDIN(); 
                        system("clear");
                    }
                    else
                    {
                        /* Cancella thread checkStopWaiting */
                        pthread_cancel(TIDstopWait); 
                    
                        memset(buffer, '\0', buffer_size);
                        read_from_server(socketfdserver, buffer, 2, "Errore lettura feedback");

                        if(strncmp(buffer, "OK", 2) != 0){
                            close(socketfdserver);
                            fprintf(stderr, "Errore lettura feedback\n");
                            exit(EXIT_FAILURE);
                        }

                        write_to_server(socketfdserver, "OK", 2, "Errore scrittura feedback");

                        cleanSTDIN();

                        /* Cattura nickname del client con cui questo client è in comunicazione */
                        memset(buffer, '\0', buffer_size);
                        read_from_server(socketfdserver, buffer, 16, "Errore lettura feedback");
                    
                        /* Gestione chat */
                        ssize_t bytesScritti;
                        ssize_t bytesLetti;
                        bool stopChat = false;

                        chatStartUI(buffer);
                        
                        fd_set FileDescriptorSET;
                        int NumeroMassimoFDSelect = socketfdserver + 1;

                        char resultMsg[1024];
                        char *bufferMsg = (char*)calloc(1024, sizeof(char));
                        size_t sizeBufferMsg = 1024;
                        if(bufferMsg == NULL){
                            fprintf(stderr, "Errore allocazione memoria bufferMsg\n");
                            stopChat = true;
                            status = ERROR;
                        }
                        
                        do
                        {
                            FD_ZERO(&FileDescriptorSET);
                            FD_SET(socketfdserver, &FileDescriptorSET);
                            FD_SET(STDIN_FILENO, &FileDescriptorSET);

                            status = select(NumeroMassimoFDSelect, &FileDescriptorSET, NULL, NULL, NULL);

                            if(status < 0) 
                            { 
                                fprintf(stderr,"Errore select\n");
                                stopChat = true;
                                status = ERROR;
                                break;
                            }

                            for(int fd=0; fd<NumeroMassimoFDSelect; fd++)
                            {
                                if(FD_ISSET(fd, &FileDescriptorSET))
                                {
                                    if(fd == STDIN_FILENO)
                                    {
                                        getline(&bufferMsg, &sizeBufferMsg, stdin);

                                        if(strlen(bufferMsg) > 1006) 
                                        { 
                                            fprintf(stderr,"Messaggio troppo lungo! (massimo 1000 caratteri)\n");
                                        }
                                        else if(strncmp(bufferMsg, "/STOP\n", 1024) != 0)
                                        {
                                            buildMessageChat(resultMsg, bufferMsg, nickname);
                                            printf("Stringa: %s", resultMsg);
                                            bytesScritti = safeWrite(socketfdserver, resultMsg, 1024);
                                            if(bytesScritti != 1024){
                                                fprintf(stderr,"Errore scrittura messaggio.\n");
                                                stopChat = true;
                                                status = ERROR;
                                            }
                                        }
                                        else
                                        {
                                            printf("Hai interrotto la conversazione. Ritorno al menu principale.\n");
                                            stopChat = true;
                                            bufferMsg[5] = '\0';
                                            bytesScritti = safeWrite(socketfdserver, bufferMsg, 1024);
                                            if(bytesScritti != 1024){
                                                fprintf(stderr,"Errore scrittura messaggio stop.\n");
                                                status = ERROR;
                                            }
                                        }

                                        memset(bufferMsg, '\0', malloc_usable_size(bufferMsg));
                                    }
                                    else if(fd == socketfdserver)
                                    {
                                        bytesLetti = safeRead(socketfdserver, bufferMsg, 1024);
                                        if(bytesLetti != 1024)
                                        {
                                            fprintf(stderr,"Errore lettura messaggio.\n");
                                            stopChat = true;
                                            status = ERROR;
                                        }
                                        else if(strncmp(bufferMsg, "/STOP", 1024) == 0) 
                                        { 
                                            printf("\n> L'altro client ha interrotto la conversazione. Ritorno al menu principale.\n");
                                            stopChat = true;
                                        }
                                        else if(strncmp(bufferMsg, "TIMEC", 1024) == 0) 
                                        {
                                            fprintf(stderr,"\n> La chat è andata in TIMEDOUT. Ritorno in attesa nella stanza %s.\n", Rooms[inputStanza].roomName);
                                            stopChat = true;
                                            chatTimedout = true;
                                        }
                                        else if(strncmp(bufferMsg, "IDLEC", 1024) == 0)
                                        {
                                            fprintf(stderr, "\n> La chat è inattiva. Ritorno al menu principale.\n");
                                            stopChat = true;
                                        }
                                        else
                                        {
                                            printf("%s", bufferMsg);
                                        }

                                        memset(bufferMsg, '\0', malloc_usable_size(bufferMsg));
                                    }
                                }
                            }
                        }while(!stopChat);

                        free(bufferMsg);
                        fprintf(stderr, "> Chat terminata.\n");

                        if(status != ERROR) 
                        {
                            /* Cattura feedback dal server ('OK') */
                            memset(buffer, '\0', buffer_size);
                            bytesLetti = safeRead(socketfdserver, buffer, 2);
                            if(bytesLetti != 2){
                                close(socketfdserver);
                                fprintf(stderr, "Errore lettura feedback\n");
                                exit(EXIT_FAILURE);
                            }

                            /* Scrittura feedback al server ('OK' - termina checkConnectionClient() lato server) */
                            bytesScritti = safeWrite(socketfdserver, "OK", 2);
                            if(bytesScritti != 2){
                                close(socketfdserver);
                                fprintf(stderr, "Errore scrittura feedback\n");
                                exit(EXIT_FAILURE);
                            }

                            sleep(2);
                            system("clear");
                            cleanSTDIN(); 
                        }
                        else break;               
                    }
                }while(chatTimedout);
            }
        }
        else if(inputOperazione == ESCI_DAL_SERVER)
        { 
            printf("Uscita dal server.\n");
            break;
        }

        fprintf(stderr, "Input: %d Checkerror: %d\n", inputOperazione, status);

    } while(inputOperazione == VISUALIZZA_STANZE && status != ERROR); 

    /* Chiusura socket */
    fprintf(stderr, "Chiusura connessione.\n");
    close(socketfdserver);

    free(buffer);

    /* EXIT */
    if(status == ERROR){
        exit(EXIT_FAILURE);
    }
    else{
        exit(EXIT_SUCCESS);
    }
    
}


/* Funzione di avvio thread: controlla se il client digita 'STOP' per interrompere l'attesa */
void* checkStopWaiting(void *arg)
{
    int socketfdserver = *((int*)arg);
    ssize_t bytesScritti = 0;
    size_t buffersize = 16;
    char *buffer = (char*)malloc(sizeof(char)*16);
    if(buffer == NULL) fprintf(stderr,"Errore allocazione memoria buffer checkStopWaiting\n");
    pthread_cleanup_push(free, (void*)buffer);

    do
    {
        memset(buffer, '\0', 16);
        getline(&buffer, &buffersize, stdin);

    }while((strncmp(buffer, "STOP\n", buffersize) != 0));

    /* Invia 'ST' al server per interrompere l'attesa */
    fprintf(stderr, "Invio messaggio d'interruzione al server: interruzione attesa.\n");
    bytesScritti = safeWrite(socketfdserver, "ST", 2);
    if(bytesScritti != 2){
        fprintf(stderr, "Errore scrittura STOP.\n");
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}