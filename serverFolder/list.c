#include "list.h"

// Inserisce un nodo in testa alla lista
List *listNicknames_insertOnHead(List *L, char *nickname, size_t sizeNickname){
    List *newNode = (List*)malloc(sizeof(List));
    if(newNode == NULL)
        fprintf(stderr, "Errore malloc listNicknames_insertOnHead\n"),
        exit(EXIT_FAILURE);

    strncpy(newNode->nickname, nickname, sizeNickname);
    newNode->next = L;
    return newNode;
}

// Inserisce un nodo in coda alla lista
List *listNicknames_insertOnQueue(List *L, char *nickname, size_t sizeNickname){
    if(L != NULL){
        L->next = listNicknames_insertOnQueue(L->next, nickname, sizeNickname);
    }
    else{
        List *newNode = (List*)malloc(sizeof(List));
        if(newNode == NULL)
            fprintf(stderr, "Errore malloc listNicknames_insertOnQueue\n"),
            exit(EXIT_FAILURE);

        strncpy(newNode->nickname, nickname, sizeNickname);
        newNode->next = NULL;
        L = newNode;
    }
    return L;
}

/* Elimina un nodo all'interno della lista che ha campo 'L->nickname'
 uguale a 'nickname'. Non elimina se non è presente. */
List *listNicknames_deleteNode(List *L, char *nickname){
    if(L != NULL){
        if(strcmp(nickname, L->nickname) == 0){
            List *tmp;
            tmp = L;
            L = L->next;
            free(tmp);
        }
        else{
            L->next = listNicknames_deleteNode(L->next, nickname);
        }
    }
    return L;
}

// Distrugge la lista
List *listNicknames_destroy(List *L){
    if(L != NULL){
        L->next = listNicknames_destroy(L->next);
        free(L);
    }
    return NULL;
}

// Stampa la lista
void listNicknames_print(List *L){
    if(L != NULL){
        printf("%s\n", L->nickname);
        listNicknames_print(L->next);
    }
}

// Controlla se la stringa 'nickname' è già presente nella lista
bool listNicknames_existingNickname(List *L, char *nickname, size_t sizeNickname){
    bool isPresent = false;
    if(L != NULL){
        if(strncmp(nickname, L->nickname, sizeNickname) == 0){
            isPresent = true;
        }
        else{
            isPresent = listNicknames_existingNickname(L->next, nickname, sizeNickname);
        }
    }
    return isPresent;
}
