#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/nivel.h>

typedef struct nivel {
	char *nombre;
	t_list *items;
	char *ip;
	char *puerto;
	int tiempoChequeoDeadLock;
	int recovery;

} __attribute__((__packed__)) Nivel;

Nivel* levantarConfiguracion(char *rutaArchivo);
