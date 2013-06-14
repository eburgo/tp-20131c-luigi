#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <commons/tad_items.h>

typedef struct ubicacionP {
	int x;
	int y;
	char* simbolo;
	t_queue* recursosObtenidos;
	t_list *itemsAsignados;
	char* itemNecesitado;

}__attribute__((__packed__)) Personaje;

typedef struct {
	char* nombre;
	int cantidad;
}__attribute__((__packed__))RecursoAsignado;

typedef struct nivel {
	char *nombre;
	t_list *items;
	char *ip;
	int puerto;
	int tiempoChequeoDeadLock;
	int recovery;

}__attribute__((__packed__)) Nivel;



Nivel* levantarConfiguracion(char *rutaArchivo);
// Libera los recursosy le avisa al orquestador de los recursos liberados.
void liberarRecursos(Personaje* personaje, int socketOrquestador);
