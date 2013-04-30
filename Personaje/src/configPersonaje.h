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
/*
 * Devuelve la cantidad de vidas despues de sumar
 */
int sumarVida(Personaje *pj);
/*
 * devuelve la cantidad de vidas despues de restar
 * o -1 si ya no tiene mas vidas
 */
int sacarVida(Personaje *pj);
/*
 * devuelve -1 si ya no tiene mas niveles
 * o 0 si se devolvio correctamente
 */
Nivel* proximoNivel(Personaje *pj);
