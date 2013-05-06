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
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <commons/socket.h>
#include "Planificador.h"
#include "servidor.h"
#include "Orquestador.h"

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
//Listas compartidas.
t_dictionary *planificadores;
t_dictionary *niveles;
//t_queue *personajesBloqueados;


t_log* logger;

// PROTOTIPOS

int iniciarOrquestador() {
	int socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	//t_queue *colaHilos = queue_create();
	niveles = dictionary_create();
	planificadores = dictionary_create();

	logger = log_create("/home/utnso/plataforma.log", "TEST", true,
			LOG_LEVEL_TRACE);
	log_info(logger, "Log creado con exito, se procede levantar el servidor");

	socketEscucha = iniciarServidor(PUERTO);

	printf("Escuchando conexiones entrantes.\n");

// Aceptar una nueva conexion entrante. Se genera un nuevo socket con la nueva conexion.
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));

		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {
			log_error(logger, "Error al aceptar una conexión.");
			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;
		}
		log_info(logger, "Se acepto una conexion correctamente.");
		log_info(logger, "se crea un hilo para manejar la conexion.");
		pthread_create(thread, NULL, (void*) manejarConexion,
				socketNuevaConexion);

	}

	close(socketEscucha);
	log_info(logger, "Se serro el socket de escuchar de la plataforma.");
	return EXIT_SUCCESS;
}

void manejarConexion(int* socket) {
	MPS_MSG mensaje;

	recibirMensaje(*socket, &mensaje);
	log_info(logger, "Se recibio un mensaje tipo: %d",
			mensaje.PayloadDescriptor);
	switch (mensaje.PayloadDescriptor) {

	case REGISTRAR_NIVEL:
		log_info(logger, "Se conecto un nivel");
		registrarNivel(mensaje.Payload);

		iniciarPlanificador();

		log_info(logger, "Se creo un hilo para planificar para ese nivel");

		break;
	case PJ_PIDE_NIVEL:

		log_info(logger, "Se conecto un personaje.");
		NivelConexion *nivel = malloc(sizeof(NivelConexion));
		// prepararNivelConexion()
			//ok? enviarMsjError(socket,"no esta el nivel")

		t_stream *stream = nivelConexion_serializer(nivel);

		enviarMensaje(*socket, &mensaje);
		log_info(logger, "se lo deriva al orquestador");
		break;
	default:
		log_error(logger, "Tipo de mensaje desconocido.");
		enviarMsjError(socket);
		break;
	}
}

void enviarMsjError(int *socket,char* msjError) {
	MPS_MSG respuestaError;
	respuestaError.PayloadDescriptor = ERROR_MENSAJE;
	respuestaError.PayLoadLength = strlen("msjError") + 1;
	respuestaError.Payload = "msjError";
	enviarMensaje(*socket, &respuestaError);
}

int registrarNivel(Nivel *nivel) {
	return 0;
}

int prepararNivelConexion(char* nombre,NivelConexion nivelConexion){
	/*
	nivel=buscarNivel(nombre))
		ok?
	plataforma =buscarPlataforma(nombre)
		ok? return
	armarNivelConexion(nivelConexion,nivel,plataforma);
	return ok;

	*/
}

int iniciarUnPlanificador(Nivel *nivel) {
	pthread_t thread;
	pthread_create(&thread, NULL, (void*) iniciarPlanificador, nivel);
	return 0;
}

