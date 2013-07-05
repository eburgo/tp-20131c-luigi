#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
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
	int socketEscucha;
	int puerto;
	t_list *bloqueados;
	t_list *personajes; //por orden de llegada para saber cual matar en caso de bloqueo
	t_queue *listos; //cola de listos
	fd_set *set;
	sem_t *sem;

}__attribute__((__packed__)) Planificador;

typedef struct{
	char* simbolo;
	int socket;
	int quantum;
	char* causaBloqueo;
}__attribute__((__packed__)) Personaje;

typedef struct{
	char simboloPersonaje;
	char recursoAAsignar;
}__attribute__((__packed__)) PersonajeLiberado;
