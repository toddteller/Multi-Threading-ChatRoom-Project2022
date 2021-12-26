#include "AVL.h"


/*=======================================================================*\
                            	ROTAZIONI
\*=======================================================================*/ 
AlberoAVL *alberoAVL_rotateSSX(AlberoAVL *T)
{
	AlberoAVL *sx = T->sx;
	T->sx = sx->dx;
	sx->dx = T;
	T->altezza = 1 + max(alberoAVL_altezza(T->sx),alberoAVL_altezza(T->dx));
	sx->altezza = 1 + max(alberoAVL_altezza(sx->sx), alberoAVL_altezza(sx->dx));
	return sx;
}

AlberoAVL *alberoAVL_rotateSDX(AlberoAVL *T)
{
	AlberoAVL *dx = T->dx;
	T->dx = dx->sx;
	dx->sx = T;
	T->altezza = 1 + max(alberoAVL_altezza(T->dx),alberoAVL_altezza(T->sx));
	dx->altezza = 1 + max(alberoAVL_altezza(dx->dx), alberoAVL_altezza(dx->sx));
	return dx;
}

AlberoAVL *alberoAVL_rotateDSX(AlberoAVL *T)
{
	if(T != NULL)
	{
		T->sx = alberoAVL_rotateSDX(T->sx);
		T = alberoAVL_rotateSSX(T);
	}
	return T;
}

AlberoAVL *alberoAVL_rotateDDX(AlberoAVL *T)
{
	if(T != NULL)
	{
		T->dx = alberoAVL_rotateSSX(T->dx);
		T = alberoAVL_rotateSDX(T);
	}
	return T;
}


/*=======================================================================*\
                            	BILANCIA
\*=======================================================================*/ 
AlberoAVL *alberoAVL_bilanciaSX(AlberoAVL *T)
{
	if(T != NULL)
	{
		if(abs(alberoAVL_altezza(T->sx)-alberoAVL_altezza(T->dx)) == 2)
		{
			AlberoAVL *sx = T->sx;
			if(alberoAVL_altezza(sx->sx) > alberoAVL_altezza(sx->dx))
			{
				T = alberoAVL_rotateSSX(T);
			}
			else
			{
				T = alberoAVL_rotateDSX(T);
			}
		}
		else
		{
			T->altezza = 1 + max(alberoAVL_altezza(T->sx), alberoAVL_altezza(T->dx));
		}
	}
	return T;
}

AlberoAVL *alberoAVL_bilanciaDX(AlberoAVL *T)
{
	if(T != NULL)
	{
		if(abs(alberoAVL_altezza(T->sx)-alberoAVL_altezza(T->dx)) == 2)
		{
			AlberoAVL *dx = T->dx;
			if(alberoAVL_altezza(dx->dx) > alberoAVL_altezza(dx->sx))
			{
				T = alberoAVL_rotateSDX(T);
			}
			else
			{
				T = alberoAVL_rotateDDX(T);
			}
		}
		else
		{
			T->altezza = 1 + max(alberoAVL_altezza(T->sx), alberoAVL_altezza(T->dx));
		}
	}
	return T;
}



/*=======================================================================*\
                            OPERAZIONI AVL
\*=======================================================================*/ 

/* Inserisce un nuovo nodo nell'albero AVL */
AlberoAVL *alberoAVL_insertNickname(AlberoAVL *T, char *nickname, int uniqueNumberNickname)
{
	if(T == NULL)
	{
		T = (AlberoAVL*)malloc(sizeof(AlberoAVL));
		if(T == NULL) fprintf(stderr, "Errore allocazione nodo AVL\n");

		strncpy(T->nickname, nickname, 16);
		T->uniqueNumberNickname = uniqueNumberNickname;
		T->altezza = 0;
		T->sx = NULL;
		T->dx = NULL;
	}
	else
	{
		if(uniqueNumberNickname >= T->uniqueNumberNickname)
		{
			T->dx = alberoAVL_insertNickname(T->dx, nickname, uniqueNumberNickname);
			T = alberoAVL_bilanciaDX(T);
		}
		else if(uniqueNumberNickname < T->uniqueNumberNickname)
		{
			T->sx = alberoAVL_insertNickname(T->sx, nickname, uniqueNumberNickname);
			T = alberoAVL_bilanciaSX(T);
		}
	}
	return T;
}

/* Elimina un nodo dall'albero AVL univocamente determinato da uniqueNumberNickname */
AlberoAVL *alberoAVL_deleteNickname(AlberoAVL *T, int uniqueNumberNickname)
{
	if(T != NULL)
	{
		if(T->uniqueNumberNickname < uniqueNumberNickname)
		{
			T->dx = alberoAVL_deleteNickname(T->dx, uniqueNumberNickname);
			T = alberoAVL_bilanciaSX(T);
		}
		else if(T->uniqueNumberNickname > uniqueNumberNickname)
		{
			T->sx = alberoAVL_deleteNickname(T->sx, uniqueNumberNickname);
			T = alberoAVL_bilanciaDX(T);
		}
		else
		{
			T = alberoAVL_deleteRoot(T);
		}
	}
	return T;
}

AlberoAVL *alberoAVL_deleteRoot(AlberoAVL *T)
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
			tmp = alberoAVL_staccaMinimo(T->dx, T);
			T->uniqueNumberNickname = tmp->uniqueNumberNickname;
			strncpy(T->nickname, tmp->nickname, 16);
			free(tmp);
			T = alberoAVL_bilanciaSX(T);
		}
	}
	return T;
}

AlberoAVL *alberoAVL_staccaMinimo(AlberoAVL *T, AlberoAVL *P)
{
	AlberoAVL *min, *root;
	if(T!=NULL)
	{
		if(T->sx != NULL)
		{
			min = alberoAVL_staccaMinimo(T->sx, T);
			root = alberoAVL_bilanciaDX(T);
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

/* Data una stringa, restituisce la somma dei suoi caratteri convertiti in intero */
int sumOfCharactersStringASCIICode(char *string)
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

/* Ricerca un nodo nell'albero AVL unicovamente determinato da uniqueNumberNickname */
bool alberoAVL_nicknameExists(AlberoAVL *T, int uniqueNumberNickname)
{
	bool exists = false;
	if(T != NULL)
	{
		if(T->uniqueNumberNickname < uniqueNumberNickname)
		{
			exists = alberoAVL_nicknameExists(T->dx, uniqueNumberNickname);
		}
		else if(T->uniqueNumberNickname > uniqueNumberNickname)
		{
			exists = alberoAVL_nicknameExists(T->sx, uniqueNumberNickname);
		}
		else
		{
			exists = true;
		}
	}
	return exists;
}

/* Distrugge un albero AVL con radice T */
AlberoAVL *alberoAVL_deleteTree(AlberoAVL *T)
{
	if(T!=NULL)
	{
		T->sx = alberoAVL_deleteTree(T->sx);
		T->dx = alberoAVL_deleteTree(T->dx);
		free(T);
	}

	return NULL;
}

/* Stampa contenuto albero AVL post order */
void alberoAVL_printTree(AlberoAVL *T)
{
	if(T != NULL)
	{
		alberoAVL_printTree(T->sx);
		printf("Nickname: %s | id: %d\n", T->nickname, T->uniqueNumberNickname);
		alberoAVL_printTree(T->dx);
	}
}

/* Restituisce il massimo tra a e b */
int max(int a, int b)
{
	return (a > b)? a : b;
}

/* Restituisce l'alberoAVL_altezza di un albero AVL */
int alberoAVL_altezza(AlberoAVL *T)
{
	if(T != NULL) return T->altezza;
	else return -1;
}