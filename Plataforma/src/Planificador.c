#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/socket.h>
#include "Planificador.h"

extern t_dictionary *planificadores;
extern pthread_mutex_t semaforo_planificadores;
int iniciarPlanificador(void* nombreNivel) {
	Planificador *planificador=malloc(sizeof(Planificador));
	int *socketEscucha;
	socketEscucha = malloc(sizeof(int));
	planificador->puerto=realizarConexion(socketEscucha);
	planificador->nombreNivel =(char*) nombreNivel;
	planificador->ip = "127.0.0.1"; //todavia no se como conseguirla

	agregarmeEnPlanificadores(planificador);
	return(EXIT_SUCCESS);
}

void agregarmeEnPlanificadores(Planificador* planificador){
	pthread_mutex_lock( &semaforo_planificadores);
	dictionary_put(planificadores,planificador->nombreNivel,(void*)planificador);
	pthread_mutex_lock( &semaforo_planificadores);
}
