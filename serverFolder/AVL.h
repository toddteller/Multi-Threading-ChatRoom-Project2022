#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

// STRUTTURA DATI NODO AVL
typedef struct node{
	struct node *sx;
	struct node *dx;
	int h; // altezza albero
	char nickname[16]; // nickname client
	int nicknameInt;  // somma caratteri nickname convertiti in intero 
} AlberoAVL;

/* Data una stringa, restituisce la somma dei suoi caratteri convertiti in intero */
int sumOfAsciiString(char *string);

/* Restituisce il massimo tra a e b */
int max(int a, int b);

/* Restituisce l'altezza di un albero AVL */
int altezza(AlberoAVL *T);

/* ROTAZIONI */
AlberoAVL *rotateSSX(AlberoAVL *T);
AlberoAVL *rotateSDX(AlberoAVL *T);
AlberoAVL *rotateDSX(AlberoAVL *T);
AlberoAVL *rotateDSX(AlberoAVL *T);

/* BILANCIA SX/DX */
AlberoAVL *bilanciaSX(AlberoAVL *T);
AlberoAVL *bilanciaDX(AlberoAVL *T);

/* Inserisce un nuovo nodo nell'albero AVL */
AlberoAVL *alberoAVL_insertNickname(AlberoAVL *T, char *nickname, int nicknameInt);
/* Elimina un nodo dall'albero AVL univocamente determinato da nicknameInt */
AlberoAVL *alberoAVL_deleteNickname(AlberoAVL *T, int nicknameInt);
AlberoAVL *deleteRoot(AlberoAVL *T);
AlberoAVL *staccaMinimo(AlberoAVL *T, AlberoAVL *P);
/* Ricerca un nodo nell'albero AVL unicovamente determinato da nicknameInt */
bool alberoAVL_nicknameExists(AlberoAVL *T, int nicknameInt);

/* Distrugge un albero AVL con radice T */
AlberoAVL *deleteTreeAVL(AlberoAVL *T);
/* Stampa contenuto albero AVL post order */
void printTree(AlberoAVL *T);
