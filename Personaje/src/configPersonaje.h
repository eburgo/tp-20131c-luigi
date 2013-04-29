#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/queue.h>
#include <commons/config.h>

//Estructuras

typedef struct nivel {
	char *nombre;
	char *objetos;
} __attribute__((__packed__)) Nivel;

typedef struct personaje {
	char *nombre;
	char *simbolo;
	t_queue *listaNiveles;
	int vidas;
	char *ip;
	char *puerto;
} __attribute__((__packed__)) Personaje;



// ------------ FUNCIONES -------------
//Devuelve un personaje a partir de la ruta de un archivo de
//configuracion.
Personaje* levantarConfiguracion(char *rutaArchivo);
