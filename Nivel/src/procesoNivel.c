#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <commons/socket.h>
#include <commons/log.h>
#include "configNivel.h"

//Funciones

//Esta funcion se encargara de detectar los interbloqueos de los personajes
//como tambien se comunicara con el orquestador.
int detectarInterbloqueos();
//Funcion principal que se utiliza cuando un personaje se conecta al nivel.
int comunicarPersonajes();
// Inicia el servidor y devuelve el puerto asignado aleatoreamente.
int realizarConexion(int* socketEscucha);

//Globales
Nivel* nivel;
t_log* logger;
struct sockaddr_in sAddr;

int main(int argc, char **argv) {
	int *socketEscucha;
	int miPuerto;
	logger = log_create("/home/utnso/nivel.log", "TEST", true, LOG_LEVEL_TRACE);
	log_info(logger,
			"Log creado con exito, se procede a loguear el proceso Nivel");

	log_debug(logger, "Chequeando el path del nivel...");
	if (argv[1] == NULL ) {
		log_error(logger, "El path del nivel no puede ser vacio.");
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	log_debug(logger, "Levantando configuracion en el path:%s", argv[1]);
	nivel = levantarConfiguracion(argv[1]);
	log_debug(logger, "Configuracion del nivel levantada correctamente.");

	log_debug(logger, "Levantando el servidor del Nivel:%s",nivel->nombre);
	socketEscucha = malloc(sizeof(int));
	miPuerto = realizarConexion(socketEscucha);
	log_debug(logger, "Servidor del nivel %sse levanto con exito en el puerto:%d",nivel->nombre,miPuerto);

	pthread_t hiloOrquestador;
	pthread_t hiloPersonajes;

	pthread_create(&hiloOrquestador, NULL, (void*) detectarInterbloqueos, &miPuerto );
	pthread_create(&hiloPersonajes, NULL, (void*) comunicarPersonajes, &socketEscucha );

	pthread_join(hiloOrquestador, NULL );
	pthread_join(hiloPersonajes, NULL );

	close(miPuerto);
	log_destroy(logger);
	return EXIT_SUCCESS;
}

int detectarInterbloqueos(int miPuerto ) {
	return 0;
}

//Cata lo hace !!!
int comunicarPersonajes(int socketEscucha) {
	return 0;
}
int realizarConexion(int* socketEscucha) {
	*socketEscucha = iniciarServidor(0);
	struct sockaddr_in sAddr;
	socklen_t len = sizeof(sAddr);
	getsockname(*socketEscucha, (struct sockaddr *) &sAddr, &len);
	return ntohs(sAddr.sin_port);
}
