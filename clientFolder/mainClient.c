#include "client.h"

int main(int argc, char **argv)
{
    /* Controllo parametri input */
    if(argc!=4) fprintf(stderr, "Uso corretto: %s <indirizzo> <port> <nickname>\n", argv[0]), exit(EXIT_FAILURE);
    if(strlen(argv[3]) > 16) fprintf(stderr, "Lunghezza massima nickname: 16 caratteri\n"), exit(EXIT_FAILURE);

    /* Buffer generico e nickname */
    char *buffer;
    buffer = (char*)calloc(16,sizeof(char));
    if(buffer == NULL){
        fprintf(stderr,"Errore malloc buffer\n");
        exit(EXIT_FAILURE);
    }
    size_t bufsize = 16;
    char nickname[18];
    memset(nickname, '\0', 18);
    strncpy(nickname, argv[3], 16);

    /* Connessione server */
    unsigned short int port;
    struct sockaddr_in indirizzo;
    int socketfd, checkerror;

    port = atoi(argv[2]);

    indirizzo.sin_family = AF_INET;
    indirizzo.sin_port = htons(port);
    inet_aton(argv[1], &indirizzo.sin_addr);

    socketfd = socket(PF_INET, SOCK_STREAM, 0);
    check_perror(socketfd, "Errore socket()", -1);

    checkerror = connect(socketfd, (struct sockaddr*)&indirizzo, sizeof(indirizzo));
    check_perror(checkerror, "Errore connect()", -1);

    ssize_t bytesScritti = 0;
    ssize_t bytesLetti = 0;

    printf("Connesso.\n");

    /* Invio nickname al server */
    bytesScritti = safeWrite(socketfd, nickname, 16);
    if(bytesScritti != 16){ // Errore: chiusura socket ed esci dal programma
        fprintf(stderr, "Errore scrittura nickname\n");
        checkerror = close(socketfd);
        check_perror(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }

    /* Cattura feedback dal server ("OK" oppure "EX") */
    memset(buffer, '\0', bufsize);
    bytesLetti = safeRead(socketfd, buffer, 2);
    if(bytesLetti != 2){
        checkerror = close(socketfd);
        fprintf(stderr, "Errore lettura feedback\n");
        check_perror(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }
    else if(strncmp(buffer, "EX", 2) == 0){ // Il client ha ricevuto 'EX': nickname già esistente
        fprintf(stderr, "Il nickname inserito esiste già!\n");
        checkerror = close(socketfd);
        check_perror(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }
    else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
        checkerror = close(socketfd);
        fprintf(stderr, "Errore lettura feedback\n");
        check_perror(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }

    /* Scrittura feedback al server ('OK') */
    bytesScritti = safeWrite(socketfd, "OK", 2);
    if(bytesScritti != 2){
        checkerror = close(socketfd);
        fprintf(stderr, "Errore scrittura feedback\n");
        check_perror(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }

    /* Invio operazione ('1' oppure '2') al server */
    int inputOperazione;
    do 
    {
        bool checkInput;
        do {
            do {
                memset(buffer, '\0', bufsize);
                checkInput = false;
                menuPrincipale(nickname);
                getline(&buffer, &bufsize, stdin);
                if(strlen(buffer) == 2 && (strncmp("1", buffer, 1) == 0 || strncmp("2", buffer, 1) == 0)) {
                    checkInput = true;
                    printf("\n");
                }else{
                    printf("Input errato, riprova.\n");
                }
            }while(!checkInput); // continua finché l'input non valido

            /* Salva input operazione */
            inputOperazione = buffer[0] - '0';

            /* Scrittura operazione al server */
            bytesScritti = safeWrite(socketfd, buffer, 1);
            if(bytesScritti != 1){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore scrittura operazione.\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            /* Cattura feedback dal server ("OK" oppure "IN") */
            memset(buffer, '\0', bufsize);
            bytesLetti = safeRead(socketfd, buffer, 2);
            if(bytesLetti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Connessione scaduta\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }
            else if(strncmp(buffer, "IN", 2) == 0){ // Il client ha ricevuto 'IN': input errato
                checkInput = false;
                printf("Input errato, riprova.\n");
            }
            else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

        } while(!checkInput);

        /* Scrittura feedback al server ('OK') */
        bytesScritti = safeWrite(socketfd, "OK", 2);
        if(bytesScritti != 2){
            checkerror = close(socketfd);
            fprintf(stderr, "Errore scrittura feedback\n");
            check_perror(checkerror, "Errore chiusura socket", -1);
            exit(EXIT_FAILURE);
        }

        if(inputOperazione == 1) // visualizza stanze e cerca una chat
        { 
            printf("\n> Hai scelto di visualizzare le stanze del server per cercare una chat.\n");
            sleep(2);
            system("clear");

            // LETTURA NUMERO STANZE
            memset(buffer, '\0', bufsize);
            bytesLetti = safeRead(socketfd, buffer, 1);
            if(bytesLetti != 1){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            int numeroStanze = atoi(buffer);
            Room Rooms[numeroStanze];

            /* Scrittura feedback al server ('OK') */
            bytesScritti = safeWrite(socketfd, "OK", 2);
            if(bytesScritti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore scrittura feedback\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            /* Lettura informazioni stanze */
            for(int i=0; i<numeroStanze; i++)
            {
                // LETTURA NOME STANZA I-ESIMA
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 16);
                if(bytesLetti != 16){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                strncpy(Rooms[i].roomName, buffer, 16);

                /* Scrittura feedback al server ('OK') */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                // LETTURA NUMERO MASSIMO CLIENTS STANZA I-ESIMA
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 1);
                if(bytesLetti != 1){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                Rooms[i].maxClients = atoi(buffer);

                /* Scrittura feedback al server ('OK') */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                // LETTURA NUMERO CLIENTS ATTUALI STANZA I-ESIMA
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 1);
                if(bytesLetti != 1){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                Rooms[i].numClients = atoi(buffer);

                /* Scrittura feedback al server ('OK') */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }
            }// END for

            /* Cattura feedback dal server ("OK") */
            memset(buffer, '\0', bufsize);
            bytesLetti = safeRead(socketfd, buffer, 2);
            if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            stampaStanze(Rooms, numeroStanze); // stampa le stanze su STDOUT_FILENO
            cleanSTDIN(); // pulisci STDIN_FILENO

            /* Scelta della stanza */
            int inputStanza;
            do {
                do {
                    memset(buffer, '\0', bufsize);
                    checkInput = false;
                    printf("> Seleziona una stanza (inserisci un numero da 1 a %d)\n", numeroStanze);
                    printf("(20 secondi prima che la connessione venga chiusa)\n");
                    getline(&buffer, &bufsize, stdin);
                    if(strlen(buffer) == 2 && (strncmp("1", buffer, 1) == 0 ||
                                               strncmp("2", buffer, 1) == 0 ||
                                               strncmp("3", buffer, 1) == 0 ||
                                               strncmp("4", buffer, 1) == 0 ||
                                               strncmp("5", buffer, 1) == 0)) {
                        checkInput = true;
                    }else{
                        printf("Input errato, riprova.\n");
                    }
                } while(!checkInput);

                /* Scrittura input stanza al server */
                bytesScritti = safeWrite(socketfd, buffer, 1);
                if(bytesScritti != 1){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura input stanza.\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                inputStanza = (buffer[0] - '0') - 1; // Salva input

                /* Cattura feedback dal server ("OK" oppure "IN") */
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 2);
                if(bytesLetti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Connessione scaduta.\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }
                else if(strncmp(buffer, "IN", 2) == 0){ // Il client ha ricevuto 'IN': input errato
                    checkInput = false;
                    printf("Input errato, riprova.\n");
                }
                else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    check_perror(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }
            } while(!checkInput); // continua finché l'input non è valido


            /* Scrittura feedback al server ('OK') */
            bytesScritti = safeWrite(socketfd, "OK", 2);
            if(bytesScritti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore scrittura feedback\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            /* Cattura feedback dal server ("OK" oppure "FU") */
            bool isFull = false;
            memset(buffer, '\0', bufsize);
            bytesLetti = safeRead(socketfd, buffer, 2);
            if(bytesLetti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }
            else if(strncmp(buffer, "FU", 2) == 0){ // Il client ha ricevuto 'FU': stanza piena
                printf("La stanza selezionata è piena.\n");
                isFull = true;
            }
            else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                check_perror(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            if(!isFull) // Il client ha ricevuto 'OK': stanza non piena
            {
                bool chatTimedout; // indica se la chat è andata in timedout 

                do // ciclo do while: ritorna in attesa nella stessa stanza 
                {
                    chatTimedout = false;

                    /* Scrittura feedback al server ('OK') */
                    bytesScritti = safeWrite(socketfd, "OK", 2);
                    if(bytesScritti != 2){
                        checkerror = close(socketfd);
                        fprintf(stderr, "Errore scrittura feedback\n");
                        check_perror(checkerror, "Errore chiusura socket", -1);
                        exit(EXIT_FAILURE);
                    }

                    /* Inizializzazione attributi thread checkStopWaiting */
                    pthread_attr_t *attributi_thread;
                    attributi_thread = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
                    if(attributi_thread == NULL){
                        fprintf(stderr, "Errore allocazione attributi thread\n");
                        exit(EXIT_FAILURE);
                    }
                    checkerror = pthread_attr_init(attributi_thread);
                    check_strerror(checkerror, "Errore init attr thread", 0);
                    checkerror = pthread_attr_setdetachstate(attributi_thread, PTHREAD_CREATE_DETACHED);
                    check_strerror(checkerror, "Errore setdetachstate", 0);

                    /* Creazione thread checkStopWaiting: controlla se il client digita STOP per interrompere l'attesa */
                    pthread_t TIDstopWait;
                    checkerror = pthread_create(&TIDstopWait, attributi_thread, checkStopWaiting, (void*)&socketfd); // passaggio socket
                    check_strerror(checkerror, "Errore pthread create thread", 0);

                    printf("\n> Sei in attesa di una chat nella stanza %s...\n", Rooms[inputStanza].roomName);
                    printf("  (Per interrompere l'attesa digitare 'STOP')\n");

                    /* Cattura feedback dal server ('OK' oppure 'ST') */
                    memset(buffer, '\0', bufsize);
                    bytesLetti = safeRead(socketfd, buffer, 2);
                    if(bytesLetti != 2 || (strncmp(buffer, "OK", 2) != 0 && strncmp(buffer, "ST", 2) != 0)){
                        checkerror = close(socketfd);
                        fprintf(stderr, "Errore lettura feedback\n");
                        check_perror(checkerror, "Errore chiusura socket", -1);
                        exit(EXIT_FAILURE);
                    }
                    
                    free(attributi_thread);

                    if(strncmp(buffer, "ST", 2) == 0) /* Attesa interrotta */
                    {
                        printf("\n> Hai interrotto l'attesa. Ritorno al menu principale.\n");
                        sleep(2);
                        system("clear");
                    }
                    else /* Se il client non ha interrotto l'attesa -> avvia chat */
                    {
                        /* Cancella thread checkStopWaiting */
                        checkerror = pthread_cancel(TIDstopWait); 
                        check_strerror(checkerror, "Errore pthread create thread", 0);

                        /* Cattura feedback dal server ('OK') */
                        memset(buffer, '\0', bufsize);
                        bytesLetti = safeRead(socketfd, buffer, 2);
                        if(bytesLetti != 2 || strncmp(buffer, "OK", 2) != 0){
                            checkerror = close(socketfd);
                            fprintf(stderr, "Errore lettura feedback\n");
                            check_perror(checkerror, "Errore chiusura socket", -1);
                            exit(EXIT_FAILURE);
                        }

                        /* Scrittura feedback al server ('OK' - termina checkConnectionClient() lato server) */
                        bytesScritti = safeWrite(socketfd, "OK", 2);
                        if(bytesScritti != 2){
                            checkerror = close(socketfd);
                            fprintf(stderr, "Errore scrittura feedback\n");
                            check_perror(checkerror, "Errore chiusura socket", -1);
                            exit(EXIT_FAILURE);
                        }

                        cleanSTDIN(); // pulisci STDIN_FILENO

                        /* Cattura nickname del client con cui questo client è in comunicazione */
                        memset(buffer, '\0', bufsize);
                        bytesLetti = safeRead(socketfd, buffer, 16);
                        if(bytesLetti != 16){
                            checkerror = close(socketfd);
                            fprintf(stderr, "Errore lettura feedback\n");
                            check_perror(checkerror, "Errore chiusura socket", -1);
                            exit(EXIT_FAILURE);
                        }

                        chatStartUI(buffer); // UI Chat Start

                        /* Gestione chat */
                        bool stopChat = false;

                        fd_set fds;
                        int nfds = socketfd + 1; // numero massimo file descriptor per la select

                        char resultMsg[1024];
                        char *bufferMsg = (char*)calloc(1024, sizeof(char));
                        size_t sizeBufferMsg = 1024;
                        if(bufferMsg == NULL){
                            fprintf(stderr, "Errore allocazione memoria bufferMsg\n");
                            stopChat = true;
                            checkerror = -1;
                        }
                        
                        do{
                            FD_ZERO(&fds);
                            FD_SET(socketfd, &fds);
                            FD_SET(STDIN_FILENO, &fds);

                            /* SELECT con controllo */
                            checkerror = select(nfds, &fds, NULL, NULL, NULL);
                            if(checkerror < 0) // ERRORE select
                            { 
                                fprintf(stderr,"Errore select\n");
                                stopChat = true;
                                checkerror = -1;
                                break;
                            }

                            // SELECT was successful
                            for(int fd=0; fd<nfds; fd++)
                            {
                                if(FD_ISSET(fd, &fds))
                                {
                                    if(fd == 0) // STDIN_FILENO
                                    {
                                        getline(&bufferMsg, &sizeBufferMsg, stdin); // stdin

                                        if(strlen(bufferMsg) > 1006) // MASSIMO 1006 CARATTERI
                                        { 
                                            fprintf(stderr,"Messaggio troppo lungo! (massimo 1000 caratteri)\n");
                                        }
                                        else if(strncmp(bufferMsg, "/STOP\n", 1024) != 0) // /STOP permette di terminare la chat
                                        {
                                            buildMessageChat(resultMsg, bufferMsg, nickname);
                                            printf("Stringa: %s", resultMsg);
                                            bytesScritti = safeWrite(socketfd, resultMsg, 1024);
                                            if(bytesScritti != 1024){
                                                fprintf(stderr,"Errore scrittura messaggio.\n");
                                                stopChat = true;
                                                checkerror = -1;
                                            }
                                        }
                                        else // /STOP inserito, terminazione chat
                                        {
                                            printf("Hai interrotto la conversazione. Ritorno al menu principale.\n");
                                            stopChat = true;
                                            bufferMsg[5] = '\0';
                                            bytesScritti = safeWrite(socketfd, bufferMsg, 1024);
                                            if(bytesScritti != 1024){
                                                fprintf(stderr,"Errore scrittura messaggio stop.\n");
                                                checkerror = -1;
                                            }
                                        }

                                        memset(bufferMsg, '\0', malloc_usable_size(bufferMsg));
                                    }
                                    else if(fd == socketfd) // Messaggio proveniente dal server 
                                    {
                                        bytesLetti = safeRead(socketfd, bufferMsg, 1024);
                                        if(bytesLetti != 1024)
                                        {
                                            fprintf(stderr,"Errore lettura messaggio.\n");
                                            stopChat = true;
                                            checkerror = -1;
                                        }
                                        else if(strncmp(bufferMsg, "/STOP", 1024) == 0) // Lettura /STOP -> terminazione chat
                                        { 
                                            printf("\n> L'altro client ha interrotto la conversazione. Ritorno al menu principale.\n");
                                            stopChat = true;
                                        }
                                        else if(strncmp(bufferMsg, "TIMEC", 1024) == 0) // Lettura TIMEC, chat TIMEDOUT -> terminazione chat
                                        {
                                            fprintf(stderr,"\n> La chat è andata in TIMEDOUT. Ritorno in attesa nella stanza %s.\n", Rooms[inputStanza].roomName);
                                            stopChat = true;
                                            chatTimedout = true;
                                        }
                                        else if(strncmp(bufferMsg, "IDLEC", 1024) == 0) // Lettura IDLEC, chat INATTIVA -> terminazione chat
                                        {
                                            fprintf(stderr, "\n> La chat è inattiva. Ritorno al menu principale.\n");
                                            stopChat = true;
                                        }
                                        else // Stampa messaggio ricevuto
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

                        if(checkerror != -1) // Non si è verificato nessun errore 
                        {
                            /* Cattura feedback dal server ('OK') */
                            memset(buffer, '\0', bufsize);
                            bytesLetti = safeRead(socketfd, buffer, 2);
                            if(bytesLetti != 2){
                                checkerror = close(socketfd);
                                fprintf(stderr, "Errore lettura feedback\n");
                                check_perror(checkerror, "Errore chiusura socket", -1);
                                exit(EXIT_FAILURE);
                            }

                            /* Scrittura feedback al server ('OK' - termina checkConnectionClient() lato server) */
                            bytesScritti = safeWrite(socketfd, "OK", 2);
                            if(bytesScritti != 2){
                                checkerror = close(socketfd);
                                fprintf(stderr, "Errore scrittura feedback\n");
                                check_perror(checkerror, "Errore chiusura socket", -1);
                                exit(EXIT_FAILURE);
                            }

                            sleep(2);
                            system("clear");
                            cleanSTDIN(); // pulisci STDIN_FILENO
                        }
                        else break; // checkerror == -1 -> uscita                  
                    }
                }while(chatTimedout);
            }
        }
        else if(inputOperazione == 2) // EXIT dal server
        { 
            printf("Uscita dal server.\n");
            break;
        }

        fprintf(stderr, "Input: %d Checkerror: %d\n", inputOperazione, checkerror);

    } while(inputOperazione == 1 && checkerror != -1); // Continua fino a quando non si verificano errori e l'input è uguale a uno

    /* Chiusura socket */
    fprintf(stderr, "Chiusura connessione.\n");
    checkerror = close(socketfd);
    check_perror(checkerror, "Errore chiusura socket", -1);

    /* EXIT */
    if(checkerror == 0){
        exit(EXIT_SUCCESS);
    }
    else{
        exit(EXIT_FAILURE);
    }
    
}
