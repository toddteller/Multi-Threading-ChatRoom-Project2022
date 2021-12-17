#include "list.h"

int main()
{
    Queue *Q = (Queue*)malloc(sizeof(Queue));
    if(Q == NULL)
        fprintf(stderr,"Errore allocazione memoria queue\n"),
        exit(EXIT_FAILURE);
    Q->head = NULL;
    Q->tail = NULL;
    Q->numeroClients = 0;
    int n = 3;
    char *input;
    size_t bufsize = 17;
    input = (char*)malloc(sizeof(char)*bufsize);
    int check;
//    int *actualRoom_id;
    Client clients[5];

    for(int i=1; i<=n; i++){
        do {
            check = 1;
            printf("Inserisci Nick: ");
            getline(&input, &bufsize, stdin);
            if(strlen(input) > 17)
                printf("Inserisci un nickname di massimo 16 caratteri!\n"),
                check = 0;
        } while(!check);

        printf("Username input: %s", input);
        strncpy(clients[i].nickname, input, 17);

        clients[i].actualRoom_id = 5;
        clients[i].matchedRoom_id = 5;

        do {
            check = 1;
            printf("Inserisci indirizzo attuale: ");
            getline(&input, &bufsize, stdin);
            if(strlen(input) > 17)
                printf("Inserisci un nickname di massimo 16 caratteri!\n"),
                check = 0;
        } while(!check);

        printf("Indirizzo: %s", input);
        strncpy(clients[i].address, input, 15);

        do {
            check = 1;
            printf("Inserisci indirizzo precedente: ");
            getline(&input, &bufsize, stdin);
            if(strlen(input) > 17)
                printf("Inserisci un nickname di massimo 16 caratteri!\n"),
                check = 0;
        } while(!check);

        strncpy(clients[i].matchedAddress, input, 15);
        printf("Ultimo Indirizzo: %s", clients[i].matchedAddress);

        enqueue(Q, &clients[i]);
    }

    printf("------------\n");
    printQueue(Q->head);
    printf("------------\n");

    Match *matchFound = NULL;
    nodoQueue *head, *headprev, *actual, *actualprev;
    head = Q->head;
    headprev = NULL;
    actual = Q->head->next;
    actualprev = Q->head;

    int a = 0;
    while(a < Q->numeroClients-1 && matchFound == NULL)
    {
        matchFound = searchCouple(Q, actual, actualprev, head, headprev);
        headprev = head;
        head = head->next;
        actualprev = actual;
        actual = actual->next;
        printf("while\n");
        a += 1;
    }
    printf("uscito\n");

    if(matchFound != NULL)
        printf("I nomi sono: \n %s %s", matchFound->couplantClient1->nickname, matchFound->couplantClient2->nickname);

    printf("------------\n");
    printQueue(Q->head);
    printf("------------\n");

    exit(EXIT_SUCCESS);
}
