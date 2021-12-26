#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

// STRUTTURA DATI NODO AVL
typedef struct node{
	struct node *sx;
	struct node *dx;
	int altezza; 
	char nickname[16]; 
	int uniqueNumberNickname;  
} AlberoAVL;

/*=======================================================================*\
                            	ROTAZIONI
\*=======================================================================*/ 
AlberoAVL *alberoAVL_rotateSSX(AlberoAVL *T);
AlberoAVL *alberoAVL_rotateSDX(AlberoAVL *T);
AlberoAVL *alberoAVL_rotateDSX(AlberoAVL *T);
AlberoAVL *alberoAVL_rotateDSX(AlberoAVL *T);

/*=======================================================================*\
                            	BILANCIA
\*=======================================================================*/ 
AlberoAVL *alberoAVL_bilanciaSX(AlberoAVL *T);
AlberoAVL *alberoAVL_bilanciaDX(AlberoAVL *T);

/*=======================================================================*\
                            OPERAZIONI AVL
\*=======================================================================*/ 
AlberoAVL *alberoAVL_insertNickname(AlberoAVL *T, char *nickname, int uniqueNumberNickname);
AlberoAVL *alberoAVL_deleteNickname(AlberoAVL *T, int uniqueNumberNickname);
AlberoAVL *alberoAVL_deleteRoot(AlberoAVL *T);
AlberoAVL *alberoAVL_staccaMinimo(AlberoAVL *T, AlberoAVL *P);
bool alberoAVL_nicknameExists(AlberoAVL *T, int uniqueNumberNickname);
AlberoAVL *alberoAVL_deleteTree(AlberoAVL *T);
void alberoAVL_printTree(AlberoAVL *T);
int alberoAVL_altezza(AlberoAVL *T);
int max(int a, int b);
int sumOfCharactersStringASCIICode(char *string);