#include "client.h"


/*=======================================================================*\
                            CONTROLLO ERRORI
\*=======================================================================*/ 
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

/*=======================================================================*\
                            OPERAZIONI I/O
\*=======================================================================*/ 
int setupConnectionAndConnect(struct sockaddr_in indirizzo, unsigned short int port)
{
    int socketfd, checkerror;

    indirizzo.sin_family = AF_INET;
    indirizzo.sin_port = htons(port);

    socketfd = socket(PF_INET, SOCK_STREAM, 0);
    check_perror(socketfd, "Errore socket()", ERROR);

    checkerror = connect(socketfd, (struct sockaddr*)&indirizzo, sizeof(indirizzo));
    check_perror(checkerror, "Errore connect()", ERROR);

    return socketfd;
}

/* Funzioni lettura completa e safe */
ssize_t safeRead(int fd, void *buf, size_t count)
{
    size_t bytesDaLeggere = count;
    ssize_t bytesLetti = 0;

    while(bytesDaLeggere > 0)
    {
        if((bytesLetti = read(fd, buf+(count-bytesDaLeggere), bytesDaLeggere)) < 0){
            return ERROR;
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
            return ERROR;
        }
        else{
            bytesDaScrivere -= bytesScritti;
        }
        fprintf(stderr,"Scritto:%s\n", (char*)buf);
    }

    return (count - bytesDaScrivere);
}

/* Scrive esattamente count bytes al server */
void write_to_server(int socketfd, void *buf, size_t count, const char *errorMsg)
{
    long unsigned int bytes = 0;

    bytes = safeWrite(socketfd, buf, count);
    if(bytes != count)
    {
        close(socketfd);
        perror(errorMsg);
        exit(EXIT_FAILURE);
    }
}

/* Legge esattamente count bytes dal server */
void read_from_server(int socketfd, void *buf, size_t count, const char *errorMsg)
{
    long unsigned int bytes = 0;

    bytes = safeRead(socketfd, buf, count);
    if(bytes != count)
    {
        close(socketfd);
        perror(errorMsg);
        exit(EXIT_FAILURE);
    }
}

/* Costruisce il messaggio prima di inviarlo */
void buildMessageChat(char *output, char *messaggio, char *nickname){
    memset(output, '\0', 1024);
    char buffer[2] = ": ";
    strncpy(output, nickname, 16);
    strncat(output, buffer, 2);
    strncat(output, messaggio, 1006);
}

/*=======================================================================*\
                        FUNZIONI USER INTERFACE
\*=======================================================================*/ 
/* Stampa un carattere 'count' volte */
void stampaCarattere(int carattere, int count)
{
    for(int i=0; i<count; i++)
    {
        printf("%c", carattere);
    }
}

/* Stampa le stanze */
void stampaStanzeUI(Room stanze[], int max)
{
    int len;
    for(int i=0; i<max; i++)
    {
        printf("%c", 124);
        stampaCarattere(61, 17);
        printf("%c", 124);
        printf("\n");

        printf("%c ", 124);
        printf("ROOM NUMBER [%d]", i+1);
        printf(" %c", 124);
        printf("\n");

        printf("%c", 124);
        stampaCarattere(61, 17);
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
        printf("> Clients: %d%c%d", stanze[i].numeroClients, 124, stanze[i].maxClients);
        stampaCarattere(32, 1);
        printf(" %c", 124);
        printf("\n");

        printf("%c", 124);
        stampaCarattere(61, 17);
        printf("%c", 124);
        
        printf("\n\n");
    }
}

/* Stampa Menu Principale */
void menuPrincipaleUI(char *nickname)
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

/* Messaggio inizio chat avviata */
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