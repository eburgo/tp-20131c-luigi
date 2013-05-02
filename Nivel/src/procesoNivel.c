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

	//Globales
	Nivel* nivel;
	t_log* logger;

	int main(int argc, char **argv) {
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

		pthread_t hiloOrquestador;
		pthread_t hiloPersonajes;

		pthread_create(&hiloOrquestador, NULL, (void*) detectarInterbloqueos,NULL);
		pthread_create(&hiloPersonajes, NULL, (void*) comunicarPersonajes,NULL);

		pthread_join(hiloOrquestador, NULL );
		pthread_join(hiloPersonajes, NULL );

		log_destroy(logger);
		return EXIT_SUCCESS;
	}

	//Esta funcion se encargara de detectar los interbloqueos de los personajes
	//como tambien se comunicara con el orquestador.
	int detectarInterbloqueos(){
		return 0;
	}

	//Esta funcion ... arreglate cata!
	int comunicarPersonajes(){

		//cambiar puerto por el del parametro , blah.
		//int socketEscucha = iniciarServidor(5005);


		return 0;
	}
