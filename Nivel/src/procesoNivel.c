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

int detectarInterbloqueos();
int comunicarPersonajes();
//Obtiene el puerto que asigno la funcion de forma aleatoria.
int obtenerPuertoGenereado(int sockedEscucha);

//Globales
Nivel* nivel;
t_log* logger;
struct sockaddr_in sAddr;

int main(int argc, char **argv) {
	int socketEscucha, miPuerto;
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

	miPuerto = obtenerPuertoGenereado(socketEscucha);

	pthread_t hiloOrquestador;
	pthread_t hiloPersonajes;

	pthread_create(&hiloOrquestador, NULL, (void*) detectarInterbloqueos,
			miPuerto );
	pthread_create(&hiloPersonajes, NULL, (void*) comunicarPersonajes, socketEscucha );

	pthread_join(hiloOrquestador, NULL );
	pthread_join(hiloPersonajes, NULL );

	log_destroy(logger);
	return EXIT_SUCCESS;
}

//Esta funcion se encargara de detectar los interbloqueos de los personajes
//como tambien se comunicara con el orquestador.
int detectarInterbloqueos(int miPuerto ) {
	return 0;
}

//Cata lo hace.
int comunicarPersonajes(int socketEscucha) {
	while (1) {
			thread = malloc(sizeof(pthread_t));
			socketNuevaConexion = malloc(sizeof(int));
			if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {
				perror("Error al aceptar conexion entrante");
				return EXIT_FAILURE;

			}

			pthread_create(thread, NULL, (void*) interactuarConPersonaje,
					socketNuevaConexion);
			queue_push(colaHilos, thread);
		}
	// recibir una señal para cortar el ciclo-
	//Funcion que se ocupe de comunicarse con el personaje. El personaje manda un msj con el recurso que necesita
	//

	return 0;
}
int obtenerPuertoGenereado(int socketEscucha) {
	socketEscucha = iniciarServidor(0);
	socklen_t len = sizeof(sAddr);
	getsockname(socketEscucha, (struct sockaddr *) &sAddr, &len);
}
