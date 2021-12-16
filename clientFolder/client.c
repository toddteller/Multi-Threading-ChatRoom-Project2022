#include "client.h"

/* Funzioni controllo errori con perror */
void check_perror(int valueToCheck, const char *s, int unsuccessValue){
    if(valueToCheck == unsuccessValue){
        perror(s);
        exit(EXIT_FAILURE);
    }
}

/* Funzione controllo errori con strerror */
void check_strerror(int valueToCheck, const char *s, int successValue){
    if(valueToCheck != successValue){
        fprintf(stderr, "%s: %s\n", s, strerror(valueToCheck));
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
        fprintf(stderr,"Letto:%s\n", (char*)buf);
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
        fprintf(stderr,"Scritto:%s\n", (char*)buf);
    }

    return (count - bytesDaScrivere);
}

/* Funzione di avvio thread: controlla se il client digita 'STOP' per interrompere l'attesa */
void* checkStopWaiting(void *arg)
{
    int socketfd = *((int*)arg);
    ssize_t bytesScritti = 0;
    char *buffer = (char*)malloc(sizeof(char)*16);
    size_t buffersize = 16;

    // stdin
    do{
        memset(buffer, '\0', 16);
        getline(&buffer, &buffersize, stdin);
    }while((strncmp(buffer, "STOP\n", buffersize) != 0));

    /* Invia 'ST' al server per interrompere l'attesa */
    fprintf(stderr, "Invio messaggio d'interruzione al server: interruzione attesa.\n");
    bytesScritti = safeWrite(socketfd, "ST", 2);
    if(bytesScritti != 2){
        fprintf(stderr, "Errore scrittura STOP.\n");
    }

    return NULL;
}

void buildMessageChat(char *output, char *messaggio, char *nickname){
    memset(output, '\0', 1024);
    char buffer[2] = ": ";
    strncpy(output, nickname, 16);
    strncat(output, buffer, 2);
    strncat(output, messaggio, 1006);
}

/* FUNZIONI UI - USER INTERFACE*/
/* Stampa un carattere 'count' volte */
void stampaCarattere(int carattere, int count)
{
    for(int i=0; i<count; i++)
    {
        printf("%c", carattere);
    }
}

/* Stampa le stanze */
void stampaStanze(Room stanze[], int max)
{
    int len;
    for(int i=0; i<max; i++)
    {
        printf("%c", 124);
        stampaCarattere(35, 17);
        printf("%c", 124);
        printf("\n");

        printf("%c ", 124);
        printf("ROOM NUMBER [%d]", i+1);
        printf(" %c", 124);
        printf("\n");

        printf("%c", 124);
        stampaCarattere(35, 17); // =
        printf("%c", 124);
        printf("\n");

        printf("%c ", 124);
        len = strlen(stanze[i].roomName);
        stanze[i].roomName[len-1] = '\0';
        len = 14 - len;
        printf("> %s", stanze[i].roomName);
        stampaCarattere(32, len);
        printf(" %c", 124);
        printf("\n");

        printf("%c ", 124);
        printf("> Clients: %d%c%d", stanze[i].numClients, 124, stanze[i].maxClients);
        stampaCarattere(32, 1);
        printf(" %c", 124);
        printf("\n");

        printf("%c", 124);
        stampaCarattere(35, 17);
        printf("%c", 124);
        
        printf("\n\n");
    }
}

void menuPrincipale(char *nickname)
{
    stampaCarattere(61, 45); 
    printf("\n");

    printf("%c ", 124);
	stampaCarattere(32, 9);
    printf("Benvenuto in randomChat");
	stampaCarattere(32, 10);
    printf("%c", 124);
    printf("\n");

	printf("%c ", 124);
    stampaCarattere(95, 41);
    printf(" %c", 124);
    printf("\n");

	printf("%c ", 124);
	stampaCarattere(32, 41);
	printf(" %c", 124);
	printf("\n");

	printf("%c ", 124);
	stampaCarattere(32, 16);
	printf("OPERAZIONI");
	stampaCarattere(32, 15);
	printf(" %c", 124);
	printf("\n");

	printf("%c ", 124);
	stampaCarattere(32, 41);
	printf(" %c", 124);
	printf("\n");

	printf("%c ", 124);
	printf("[1] Visualizza le stanze e cerca una chat");
	printf(" %c", 124);
	printf("\n");

	printf("%c ", 124);
	printf("[2] Esci da randomChat");
	stampaCarattere(32, 19);
	printf(" %c", 124);
	printf("\n");

	printf("%c ", 124);
	stampaCarattere(32, 41);
	printf(" %c", 124);
	printf("\n");

    printf("%c ", 124);
    printf("Nickname: %s", nickname);
	stampaCarattere(32, 15+(16-strlen(nickname)));
	printf(" %c", 124);
	printf("\n");

	stampaCarattere(61, 45); 
    printf("\n\n");

	printf("> Inserisci un'operazione [1] o [2]\n");
	printf("(20 secondi prima che la connessione venga chiusa)\n");
}

void chatStartUI(char *nickname)
{
    printf("\n");
    stampaCarattere(61, 14);
    printf("\n%cCHAT TROVATA%c\n", 124, 124);

    stampaCarattere(61, 90);
    printf("\n> !ATTENZIONE! Dopo 30 secondi di totale inattività si viene riportati al menu principale.\n");
    printf("> !ATTENZIONE! Dopo 60 secondi si viene automaticamente rimessi in attesa alla ricerca di\n               una nuova chat nella stessa stanza.\n");
    printf("\n> E' possibile interrompere la chat in qualsiasi momento digitando '/STOP'\n");
    printf("> Sei in comunicazione con %s\n", nickname);
    stampaCarattere(61, 90);
    printf("\n");
}

/* Pulisce il file descriptor STDIN_FILENO */
void cleanSTDIN()
{
    char buffer[1];
    int checkerror;

    checkerror = fcntl(STDIN_FILENO, F_SETFL, (fcntl(STDIN_FILENO, F_GETFD, 0) | O_NONBLOCK));
	if(checkerror!=0) fprintf(stderr,"Error fcntl\n");
	while((checkerror = read(STDIN_FILENO, buffer, 1)) != -1);
	if(errno != EAGAIN) fprintf(stderr,"Error cleaning stdin %d\n", checkerror);
	checkerror = fcntl(STDIN_FILENO, F_SETFL, (fcntl(STDIN_FILENO, F_GETFD, 0) &~O_NONBLOCK));
	if(checkerror!=0) fprintf(stderr,"Error fcntl2\n");
}