#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct node{
	struct node *sx;
	struct node *dx;
	int h;
	char nickname[16];
	int nicknameInt;
} AlberoAVL; // Struttura dati albero AVL contenente i nicknames

//somma di caratteri in ascii code di una stringa
int sumOfAsciiString(char *string);
//altezza
int altezza(AlberoAVL *T);
//max
int max(int a, int b);
//rotateSSX
AlberoAVL *rotateSSX(AlberoAVL *T);
//rotateSDX
AlberoAVL *rotateSDX(AlberoAVL *T);
//rotateDSX
AlberoAVL *rotateDSX(AlberoAVL *T);
//rotateDDX
AlberoAVL *rotateDSX(AlberoAVL *T);
//bilanciaSX
AlberoAVL *bilanciaSX(AlberoAVL *T);
//bilanciaDX
AlberoAVL *bilanciaDX(AlberoAVL *T);
//insert
AlberoAVL *alberoAVL_insertNickname(AlberoAVL *T, char *nickname, int nicknameInt);
//elimina
AlberoAVL *alberoAVL_deleteNickname(AlberoAVL *T, int nicknameInt);
AlberoAVL *deleteRoot(AlberoAVL *T);
AlberoAVL *staccaMinimo(AlberoAVL *T, AlberoAVL *P);
//distruggi
AlberoAVL *deleteTreeAVL(AlberoAVL *T);
//stampa
void printTree(AlberoAVL *T);
//ricerca
bool alberoAVL_nicknameExists(AlberoAVL *T, int nicknameInt);