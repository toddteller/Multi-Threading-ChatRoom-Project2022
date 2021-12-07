#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

/* Funzioni controllo errori */
void checkS(int valueToCheck, const char *s, int unsuccessValue); //socket

/* Funzioni lettura e scrittura completa e safe */
ssize_t safeRead(int fd, void *buf, size_t count);
ssize_t safeWrite(int fd, const void *buf, size_t count);

typedef struct{
    char roomName[16];
    int maxClients;
    int numClients;
} Room;

int main(int argc, char **argv)
{
    if(argc!=4) fprintf(stderr, "Uso corretto: %s <indirizzo> <port> <nickname>\n", argv[0]), exit(EXIT_FAILURE);
    if(strlen(argv[3]) > 16) fprintf(stderr, "Lunghezza massima nickname: 16 caratteri\n"), exit(EXIT_FAILURE);

    char *buffer;
    size_t bufsize = sizeof(buffer);
    char nickname[16];
    unsigned short int port;
    struct sockaddr_in indirizzo;
    int socketfd, checkerror;

    memset(nickname, '\0', sizeof(nickname));
    strncpy(nickname, argv[3], 16);
    port = atoi(argv[2]);

    indirizzo.sin_family = AF_INET;
    indirizzo.sin_port = htons(port);
    inet_aton(argv[1], &indirizzo.sin_addr);

    socketfd = socket(PF_INET, SOCK_STREAM, 0);
    checkS(socketfd, "Errore socket()", -1);

    checkerror = connect(socketfd, (struct sockaddr*)&indirizzo, sizeof(indirizzo));
    checkS(checkerror, "Errore connect()", -1);

    ssize_t bytesScritti = 0;
    ssize_t bytesLetti = 0;

    printf("Connesso.\n");

    /* Invio nickname al server */
    bytesScritti = safeWrite(socketfd, nickname, 16);
    if(bytesScritti != 16){ // Errore: chiusura socket ed esci dal programma
        fprintf(stderr, "Errore scrittura nickname\n");
        checkerror = close(socketfd);
        checkS(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }

    printf("bytesScritti nickname: %ld\n", bytesScritti);

    buffer = (char*)malloc(sizeof(buffer)*16);
    if(buffer == NULL){
        fprintf(stderr,"Errore malloc buffer\n");
        exit(EXIT_FAILURE);
    }

    /* Cattura feedback dal server ("OK" oppure "EX") */
    memset(buffer, '\0', bufsize);
    bytesLetti = safeRead(socketfd, buffer, 2);
    if(bytesLetti != 2){
        checkerror = close(socketfd);
        fprintf(stderr, "Errore lettura feedback\n");
        checkS(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }
    else if(strncmp(buffer, "EX", 2) == 0){ // Il client ha ricevuto 'EX': nickname già esistente
        fprintf(stderr, "Il nickname inserito esiste già!\n");
        checkerror = close(socketfd);
        checkS(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }
    else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
        checkerror = close(socketfd);
        fprintf(stderr, "Errore lettura feedback\n");
        checkS(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }

    /* Scrittura feedback al server ('OK') */
    bytesScritti = safeWrite(socketfd, "OK", 2);
    if(bytesScritti != 2){
        checkerror = close(socketfd);
        fprintf(stderr, "Errore scrittura feedback\n");
        checkS(checkerror, "Errore chiusura socket", -1);
        exit(EXIT_FAILURE);
    }

    /* Invio operazione ('1' oppure '2') al server */
    bool checkInput;
    int input;

    do {
        do {
            do {
                checkInput = false;
                printf("Inserisci un operazione da effettuare [1 o 2] (massimo 10 secondi per rispondere prima che si chiuda la connessione): ");
                getline(&buffer, &bufsize, stdin);
                if(strlen(buffer) == 2 && (strncmp("1", buffer, 1) == 0 || strncmp("2", buffer, 1) == 0)) {
                    checkInput = true;
                    printf("\n");
                }else{
                    printf("Input errato, riprova.\n");
                }
            } while(!checkInput);

            /* Scrittura operazione al server */
            bytesScritti = safeWrite(socketfd, buffer, 1);
            if(bytesScritti != 1){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore scrittura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            input = buffer[0] - '0'; // Salva input

            /* Cattura feedback dal server ("OK" oppure "IN") */
            memset(buffer, '\0', bufsize);
            bytesLetti = safeRead(socketfd, buffer, 2);
            if(bytesLetti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }
            else if(strncmp(buffer, "IN", 2) == 0){ // Il client ha ricevuto 'IN': input errato
                checkInput = false;
                printf("Input errato, riprova.\n");
            }
            else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

        } while(!checkInput);

        /* Scrittura feedback al server ('OK') */
        bytesScritti = safeWrite(socketfd, "OK", 2);
        if(bytesScritti != 2){
            checkerror = close(socketfd);
            fprintf(stderr, "Errore scrittura feedback\n");
            checkS(checkerror, "Errore chiusura socket", -1);
            exit(EXIT_FAILURE);
        }

        if(input == 1){ // visualizza stanze e cerca una chat

            // LETTURA NUMERO STANZE
            memset(buffer, '\0', bufsize);
            bytesLetti = safeRead(socketfd, buffer, 1);
            if(bytesLetti != 1){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            int numeroStanze = atoi(buffer);
            Room Rooms[numeroStanze];

            /* Scrittura feedback al server ('OK') */
            bytesScritti = safeWrite(socketfd, "OK", 2);
            if(bytesScritti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore scrittura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            for(int i=0; i<numeroStanze; i++){

                printf("STANZA NUMERO [%d]\n", i+1);
                printf("--------------------------\n");
                // LETTURA NOME STANZA I-ESIMA
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 16);
                if(bytesLetti != 16){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                strncpy(Rooms[i].roomName, buffer, 16);
                printf("Nome stanza: %s", Rooms[i].roomName);

                /* Scrittura feedback al server ('OK') */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                // LETTURA NUMERO MASSIMO CLIENTS STANZA I-ESIMA
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 1);
                if(bytesLetti != 1){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                int maxClients = atoi(buffer);
                Rooms[i].maxClients = maxClients;

                /* Scrittura feedback al server ('OK') */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                // LETTURA NUMERO CLIENTS ATTUALI STANZA I-ESIMA
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 1);
                if(bytesLetti != 1){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                int numClients = atoi(buffer);
                Rooms[i].numClients = numClients;

                printf("Clients: %d/%d\n", Rooms[i].numClients, Rooms[i].maxClients);

                /* Scrittura feedback al server ('OK') */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                printf("--------------------------\n\n");
            }// end for

            /* Scelta della stanza */
            do {
                do {
                    checkInput = false;
                    printf("Seleziona una stanza (inserisci un numero da 1 a %d): ", numeroStanze);
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

                /* Scrittura operazione al server */
                bytesScritti = safeWrite(socketfd, buffer, 1);
                if(bytesScritti != 1){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                input = (buffer[0] - '0') - 1; // Salva input

                /* Cattura feedback dal server ("OK" oppure "IN") */
                memset(buffer, '\0', bufsize);
                bytesLetti = safeRead(socketfd, buffer, 2);
                if(bytesLetti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }
                else if(strncmp(buffer, "IN", 2) == 0){ // Il client ha ricevuto 'IN': input errato
                    checkInput = false;
                    printf("Input errato, riprova.\n");
                }
                else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }
            } while(!checkInput);

            /* Scrittura feedback al server ('OK') */
            bytesScritti = safeWrite(socketfd, "OK", 2);
            if(bytesScritti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore scrittura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            /* Cattura feedback dal server ("OK" oppure "FU") */
            bool isFull = false;
            memset(buffer, '\0', bufsize);
            bytesLetti = safeRead(socketfd, buffer, 2);
            if(bytesLetti != 2){
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }
            else if(strncmp(buffer, "FU", 2) == 0){ // Il client ha ricevuto 'FU': stanza piena
                printf("La stanza selezionata è piena.\n");
                isFull = true;
            }
            else if(strncmp(buffer, "OK", 2) != 0){ // Il client non ha ricevuto 'OK': errore
                checkerror = close(socketfd);
                fprintf(stderr, "Errore lettura feedback\n");
                checkS(checkerror, "Errore chiusura socket", -1);
                exit(EXIT_FAILURE);
            }

            if(!isFull) // Il client ha ricevuto 'OK': stanza non piena
            {
                /* Scrittura feedback al server ('OK') */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                /* Cattura feedback dal server ('OK') */
                bytesLetti = safeRead(socketfd, buffer, 2);
                if(bytesLetti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore lettura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

                /* Scrittura feedback al server ('OK' - termina checkConnectionClient() lato server) */
                bytesScritti = safeWrite(socketfd, "OK", 2);
                if(bytesScritti != 2){
                    checkerror = close(socketfd);
                    fprintf(stderr, "Errore scrittura feedback\n");
                    checkS(checkerror, "Errore chiusura socket", -1);
                    exit(EXIT_FAILURE);
                }

            }

        }
        else if(input == 2){ // esci dal server
            break;
        }

    } while(1);



    /* Chiusura socket */
    checkerror = close(socketfd);
    checkS(checkerror, "Errore chiusura socket", -1);

    exit(EXIT_SUCCESS);
}



/* Funzioni controllo errori */
void checkS(int valueToCheck, const char *s, int unsuccessValue){
    if(valueToCheck == unsuccessValue){
        perror(s);
        exit(EXIT_FAILURE);
    }
}

/* Funzioni lettura completa e safe */
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
    }

    return (count - bytesDaLeggere);
}

/* Funzioni scrittura completa e safe */
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
    }

    return (count - bytesDaScrivere);
}
