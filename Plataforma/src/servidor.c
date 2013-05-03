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
#include <commons/collections/list.h>
#include <commons/socket.h>
#include <commons/structCompartidas.h>
#include "Planificador.h"
#include "servidor.h"

#define DIRECCION INADDR_ANY   //INADDR_ANY representa la direccion de cualquier
//interfaz conectada con la computadora
#define PUERTO 5000
#define BUFF_SIZE 1024

//------ TIPOS DE MENSAJES!!------
#define ERROR_MENSAJE -2
#define PEDIDO_DE_NIVEL 1 //lo pide un personaje al orquestador
#define SOLICITUD_NIVEL 2//mensaje de un nivel

// PROTOTIPOS
void derivarAlOrquestador(void* personaje);
void manejarConexion(int* socket);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
//Listas compartidas.
t_queue *personajesListos;
t_queue *personajesBloqueados;
t_list *niveles;

t_log* logger;


int main() {
	int socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	//t_queue *colaHilos = queue_create();
	personajesListos = queue_create();
	personajesBloqueados = queue_create();
	niveles = list_create();

	logger = log_create("/home/utnso/plataforma.log", "TEST", true, LOG_LEVEL_TRACE);
	log_info(logger,
			"Log creado con exito, se procede levantar el servidor");

	socketEscucha = iniciarServidor(PUERTO);

	printf("Escuchando conexiones entrantes.\n");

// Aceptar una nueva conexion entrante. Se genera un nuevo socket con la nueva conexion.
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));

		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {
			log_error(logger,
						"Error al aceptar una conexión.");
			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;
		}
		log_info(logger,
					"Se acepto una conexion correctamente.");
		log_info(logger,
					"se crea un hilo para manejar la conexion.");
		pthread_create(thread, NULL, (void*) manejarConexion,
				socketNuevaConexion);

	}

	close(socketEscucha);
	log_info(logger,
					"Se serro el socket de escuchar de la plataforma.");
	return EXIT_SUCCESS;
}

void manejarConexion(int* socket) {
	MPS_MSG mensaje;
	MPS_MSG respuestaError;
	respuestaError.PayloadDescriptor = ERROR_MENSAJE;
	respuestaError.PayLoadLength = strlen("payloadDescriptor incorrecto.") + 1;
	respuestaError.Payload = "payloadDescriptor incorrecto.";

	recibirMensaje(*socket, &mensaje);
	log_info(logger,
					"Se recibio un mensaje tipo: %d", mensaje.PayloadDescriptor);
	pthread_t thread;
	switch (mensaje.PayloadDescriptor) {

	case SOLICITUD_NIVEL:
		log_info(logger,
						"Se conecto un nivel");
		pthread_create(&thread, NULL, (void*) iniciarPlanificador,
				mensaje.Payload);
		log_info(logger,
						"Se creo un hilo para planificar para ese nivel");

		break;
	case PEDIDO_DE_NIVEL:

		log_info(logger,
						"Se conecto un personaje.");
		NivelConexion *nivel=malloc(sizeof(NivelConexion));
		nivel->ipNivel="192.168.0.1";
		nivel->ipPlanificador="192.168.0.1";
		nivel->puertoNivel="4500";
		nivel->puertoPlanificador="4501";

		mensaje.PayloadDescriptor=100;
		mensaje.PayLoadLength=100;
		mensaje.Payload = nivel;
		enviarMensaje(*socket,&mensaje);

		log_info(logger,
						"se lo deriva al orquestador");
		break;
	default:
		log_error(logger,
						"Tipo de mensaje desconocido.");
		enviarMensaje(*socket, &respuestaError);
		break;
	}
}


