#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/tad_items.h>

typedef struct nivel {
	char *nombre;
	t_list *items;
	char *ip;
	int puerto;
	int tiempoChequeoDeadLock;
	int recovery;

}__attribute__((__packed__)) Nivel;

typedef struct {
	char* nombre;
	t_list *itemsAsignados;
	char* itemNecesitado;
}__attribute__((__packed__)) ProcesoPersonaje;

typedef struct {
	char* nombre;
	int cantidad;
}__attribute__((__packed__))RecursoAsignado;

Nivel* levantarConfiguracion(char *rutaArchivo);
