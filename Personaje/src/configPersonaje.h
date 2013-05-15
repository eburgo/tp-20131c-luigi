#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <commons/socket.h>

//Estructuras

typedef struct nivel {
	char *nombre;
	t_queue *objetos;
} __attribute__((__packed__)) Nivel;

typedef struct personaje {
	char *nombre;
	char *simbolo;
	t_queue *listaNiveles;
	int vidas;
	char *ip;
	int puerto;
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
 * Extrae el primer elemento de la cola.
 * Devuelve -1 si ya no tiene mas niveles.
 */
Nivel* extraerProximoNivel(Personaje *pj);

/*
 * Se fija el primer elemento de la cola.
 * Devuelve -1 si ya no tiene mas niveles.
 */
Nivel* verProximoNivel(Personaje *pj);

/*
 * Devuelve una struct de NivelConexion con las ip y puertos
 * del planificador y del nivel.
 */
t_stream* pedirNivel(Personaje *personaje,int socketOrquestador);
