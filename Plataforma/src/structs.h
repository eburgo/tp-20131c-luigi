#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
//estructuras
typedef struct{
	char* nombre;
	char* ip;
	int puerto;
	int socket;
}__attribute__((__packed__)) Nivel;

typedef struct{
	char* nombreNivel; //nivel q planifica... se entiende no?
	char* ip;
	int puerto;
	t_list *bloqueados;
	t_list *personajes; //por orden de llegada para saber cual matar en caso de bloqueo
}__attribute__((__packed__)) Planificador;

typedef struct{
	char simbolo;
	int socket;
	int quantum;
}__attribute__((__packed__)) Personaje;
