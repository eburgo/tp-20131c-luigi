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
#include "configPersonaje.h"

//Funciones
void manejarSenial(int n);

//Globales
Personaje* personaje;
t_log* logger;

int main(int argc, char *argv[]) {

	int socketOrquestador;
	logger = log_create("/home/utnso/personaje.log", "TEST", true,
			LOG_LEVEL_TRACE);
	log_info(logger,
			"Log creado con exito, se procede a loguear el proceso Personaje");

	log_debug(logger, "Chequeando el path del personaje...");
	if (argv[1] == NULL ) {
		log_error(logger, "El path del personaje no puede ser vacio.");
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	log_debug(logger, "Levantando configuracion en el path:%s", argv[1]);
	personaje = levantarConfiguracion(argv[1]);
	log_debug(logger, "Configuracion del personaje levantada correctamente.");

	signal(SIGUSR1, manejarSenial);
	signal(SIGTERM, manejarSenial);

	log_debug(logger, "Conectando al orquestador en el puerto:%d",
			personaje->puerto);
	socketOrquestador = conectarAlServidor(personaje->ip,
			personaje->puerto);

	if (socketOrquestador < 0) {
		log_error(logger,
				"Error al conectarse con el orquestador en el puerto:%d",
				personaje->puerto);
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	close(socketOrquestador);
	log_destroy(logger);
	return EXIT_SUCCESS;
}

void manejarSenial(int n) {
	switch (n) {
	case SIGUSR1:
		personaje->vidas = personaje->vidas + 1;
		break;
	case SIGTERM:
		//Aca desarrollar muerte del personaje.
		break;
	}
}

