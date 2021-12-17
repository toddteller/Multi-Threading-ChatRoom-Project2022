#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>              
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <malloc.h>
#include <termios.h>
#include <errno.h>
#include <math.h>
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
AlberoAVL *deleteTree(AlberoAVL *T);
//stampa
void printTree(AlberoAVL *T);
//ricerca
bool AlberoAVL_nicknameExists(AlberoAVL *T, int nicknameInt);

int main()
{
	AlberoAVL *T = NULL;
	int exit = 0;
	do
	{
		bool checkExists = false;
		int nodi = 0;
		printf("Inserisci numero nodi:");
		scanf("%d", &nodi);
		while(getchar() != '\n');
		char *buffer = malloc(sizeof(char)*16);
		size_t size = 16;
		for(int i = 0; i<nodi; i++)
		{
			do
			{
				memset(buffer, '\0', size);
				checkExists = false;
				printf("Inserisci nickname:");
				getline(&buffer, &size, stdin);
				checkExists = AlberoAVL_nicknameExists(T, sumOfAsciiString(buffer));
				if(checkExists) printf("Il nickname già esiste\n");
			} while (checkExists);

			T = alberoAVL_insertNickname(T, buffer, sumOfAsciiString(buffer));
		}
		printf("\nSTAMPA\n");
		printTree(T);

		int nodiDaEliminare = 0;
		int id = 0;
		printf("\nQuanti nodi vuoi eliminare?\n");
		scanf("%d", &nodiDaEliminare);
		for(int i = 0; i<nodiDaEliminare; i++)
		{
			printf("Inserisci un id da eliminare:\n");
			scanf("%d", &id);
			T = alberoAVL_deleteNickname(T, id);
		}
		
		printf("\nSTAMPA\n");
		printTree(T);

		printf("\nVuoi continuare? 1=SI 0=NO\n");
		scanf("%d", &exit);
	} while (exit == 1);
	

}

//altezza
int altezza(AlberoAVL *T)
{
	if(T != NULL) return T->h;
	else return -1;
}
//max
int max(int a, int b)
{
	return (a > b)? a : b;
}
//rotateSSX
AlberoAVL *rotateSSX(AlberoAVL *T)
{
	AlberoAVL *sx = T->sx;
	T->sx = sx->dx;
	sx->dx = T;
	T->h = 1 + max(altezza(T->sx),altezza(T->dx));
	sx->h = 1 + max(altezza(sx->sx), altezza(sx->dx));
	return sx;
}
//rotateSDX
AlberoAVL *rotateSDX(AlberoAVL *T)
{
	AlberoAVL *dx = T->dx;
	T->dx = dx->sx;
	dx->sx = T;
	T->h = 1 + max(altezza(T->dx),altezza(T->sx));
	dx->h = 1 + max(altezza(dx->dx), altezza(dx->sx));
	return dx;
}
//rotateDSX
AlberoAVL *rotateDSX(AlberoAVL *T)
{
	if(T != NULL)
	{
		T->sx = rotateSDX(T->sx);
		T = rotateSSX(T);
	}
	return T;
}
//rotateDDX
AlberoAVL *rotateDDX(AlberoAVL *T)
{
	if(T != NULL)
	{
		T->dx = rotateSSX(T->dx);
		T = rotateSDX(T);
	}
	return T;
}
//bilanciaSX
AlberoAVL *bilanciaSX(AlberoAVL *T)
{
	if(T != NULL)
	{
		if(abs(altezza(T->sx)-altezza(T->dx)) == 2)
		{
			AlberoAVL *sx = T->sx;
			if(altezza(sx->sx) > altezza(sx->dx))
			{
				T = rotateSSX(T);
			}
			else
			{
				T = rotateDSX(T);
			}
		}
		else
		{
			T->h = 1 + max(altezza(T->sx), altezza(T->dx));
		}
	}
	return T;
}
//bilanciaDX
AlberoAVL *bilanciaDX(AlberoAVL *T)
{
	if(T != NULL)
	{
		if(abs(altezza(T->sx)-altezza(T->dx)) == 2)
		{
			AlberoAVL *dx = T->dx;
			if(altezza(dx->dx) > altezza(dx->sx))
			{
				T = rotateSDX(T);
			}
			else
			{
				T = rotateDDX(T);
			}
		}
		else
		{
			T->h = 1 + max(altezza(T->sx), altezza(T->dx));
		}
	}
	return T;
}
//insert
AlberoAVL *alberoAVL_insertNickname(AlberoAVL *T, char *nickname, int nicknameInt)
{
	if(T == NULL)
	{
		T = (AlberoAVL*)malloc(sizeof(AlberoAVL));
		if(T == NULL) fprintf(stderr, "Errore allocazione nodo AVL\n");

		strncpy(T->nickname, nickname, 16);
		T->nicknameInt = nicknameInt;
		T->h = 0;
		T->sx = NULL;
		T->dx = NULL;
	}
	else
	{
		if(nicknameInt >= T->nicknameInt)
		{
			T->dx = alberoAVL_insertNickname(T->dx, nickname, nicknameInt);
			T = bilanciaDX(T);
		}
		else if(nicknameInt < T->nicknameInt)
		{
			T->sx = alberoAVL_insertNickname(T->sx, nickname, nicknameInt);
			T = bilanciaSX(T);
		}
	}
	return T;
}

//elimina
AlberoAVL *alberoAVL_deleteNickname(AlberoAVL *T, int nicknameInt)
{
	if(T != NULL)
	{
		if(T->nicknameInt < nicknameInt)
		{
			T->dx = alberoAVL_deleteNickname(T->dx, nicknameInt);
			T = bilanciaSX(T);
		}
		else if(T->nicknameInt > nicknameInt)
		{
			T->sx = alberoAVL_deleteNickname(T->sx, nicknameInt);
			T = bilanciaDX(T);
		}
		else
		{
			T = deleteRoot(T);
		}
	}
	return T;
}

AlberoAVL *deleteRoot(AlberoAVL *T)
{
	if(T!=NULL)
	{
		AlberoAVL *tmp;
		if(T->dx == NULL || T->sx == NULL)
		{
			tmp = T;
			if(T->dx != NULL)
			{
				T = T->dx;
			}
			else
			{
				T = T->sx;
			}
			free(tmp);
		}
		else
		{
			tmp = staccaMinimo(T->dx, T);
			T->nicknameInt = tmp->nicknameInt;
			strncpy(T->nickname, tmp->nickname, 16);
			free(tmp);
			T = bilanciaSX(T);
		}
	}
	return T;
}

AlberoAVL *staccaMinimo(AlberoAVL *T, AlberoAVL *P)
{
	AlberoAVL *min, *root;
	if(T!=NULL)
	{
		if(T->sx != NULL)
		{
			min = staccaMinimo(T->sx, T);
			root = bilanciaDX(T);
		}
		else
		{
			min = T;
			root = T->dx;
			if(P != NULL)
			{
				if(P->sx == T)
				{
					P->sx = root;
				}
				else
				{
					P->dx = root;
				}
			}
		}
	}
	return min;
}

int sumOfAsciiString(char *string)
{
	int ret = 0;
	if(string != NULL)
	{
		for(size_t i=0; i<strlen(string); i++){
			ret += (int)string[i];
		}
	}
	return ret;
}

bool AlberoAVL_nicknameExists(AlberoAVL *T, int nicknameInt)
{
	bool exists = false;
	if(T != NULL)
	{
		if(T->nicknameInt < nicknameInt)
		{
			exists = AlberoAVL_nicknameExists(T->dx, nicknameInt);
		}
		else if(T->nicknameInt > nicknameInt)
		{
			exists = AlberoAVL_nicknameExists(T->sx, nicknameInt);
		}
		else
		{
			exists = true;
		}
	}
	return exists;
}

AlberoAVL *deleteTree(AlberoAVL *T)
{
	if(T!=NULL)
	{
		T->sx = deleteTree(T->sx);
		T->dx = deleteTree(T->dx);
		free(T);
	}
	return NULL;
}

void printTree(AlberoAVL *T)
{
	if(T != NULL)
	{
		printTree(T->sx);
		printf("Nickname: %s | id: %d\n", T->nickname, T->nicknameInt);
		printTree(T->dx);
	}
}
