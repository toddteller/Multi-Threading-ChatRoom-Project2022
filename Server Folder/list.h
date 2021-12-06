#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct node{
    struct node *next;
    char nickname[16];
} List; // Lista di nicknames

// Inserisce un nodo in testa alla lista
List *listNicknames_insertOnHead(List *L, char *nickname, size_t sizeNickname);
// Inserisce un nodo in coda alla lista
List *listNicknames_insertOnQueue(List *L, char *nickname, size_t sizeNickname);
/* Elimina un nodo all'interno della lista che ha campo 'L->nickname'
 uguale a 'nickname'. Non elimina se non è presente. */
List *listNicknames_deleteNode(List *L, char *nickname);
// Distrugge la lista
List *listNicknames_destroy(List *L);
// Stampa la lista
void listNicknames_print(List *L);
// Controlla se la stringa 'nickname' è già presente nella lista
bool listNicknames_existingNickname(List *L, char *nickname, size_t sizeNickname);
