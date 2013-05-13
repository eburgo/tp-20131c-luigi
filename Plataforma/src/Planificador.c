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

void recibirPersonaje(int *socket);// cuando llega una conexion se lo mete en la cola
void dirigirMovimientos();//se va dandole permisos a los personajes en base a la cola q tenemos. En un hilo aparte!
t_queue *personajesListos;
int cantPersonajes = 0;
Planificador *planificador;

extern t_dictionary *planificadores;
extern pthread_mutex_t semaforo_planificadores;
int quantumDefault = 2;
pthread_mutex_t semaforo_listos = PTHREAD_MUTEX_INITIALIZER;


t_log* logger;

int iniciarPlanificador(void* nombreNivel) {

	logger = log_create("/home/utnso/planificador.log", "TEST", true,
			LOG_LEVEL_TRACE);

	planificador = malloc(sizeof(Planificador));
	int *socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	personajesListos = queue_create();

	//inicializarQuantum(); todo

	socketEscucha = malloc(sizeof(int));
	planificador->puerto = realizarConexion(socketEscucha);
	planificador->nombreNivel = (char*) nombreNivel;
	planificador->ip = "127.0.0.1"; //todavia no se como conseguirla
	planificador->personajes = list_create();
	planificador->bloqueados = list_create();
	agregarmeEnPlanificadores(planificador);
	pthread_t thread2;
	pthread_create(&thread2, NULL, (void*) dirigirMovimientos, NULL );
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		if ((*socketNuevaConexion = accept(*socketEscucha, NULL, 0)) < 0) {
			log_error(logger, "Error al aceptar una conexión.");
			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;
		}
		log_info(logger, "Se acepto una conexion correctamente.");
		log_info(logger, "se crea un hilo para manejar la conexion.");
		pthread_create(thread, NULL, (void*) recibirPersonaje,
				socketNuevaConexion);

	}

	return (EXIT_SUCCESS);
}

void agregarmeEnPlanificadores(Planificador* planificador) {
	pthread_mutex_lock(&semaforo_planificadores);
	dictionary_put(planificadores, planificador->nombreNivel,
			(void*) planificador);
	pthread_mutex_lock(&semaforo_planificadores);
}

int notificarMovimientoPermitido(int socketPersonaje) {
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = MOVIMIENTO_PERMITIDO
	;
	mensajeAEnviar->PayLoadLength = strlen("Movete") + 1;
	mensajeAEnviar->Payload = "Movete";
	enviarMensaje(socketPersonaje, mensajeAEnviar);
	return 0;
}

void recibirPersonaje(int* socket) {
	MPS_MSG mensajeInicializar;
	Personaje* personaje = malloc(sizeof(Personaje));

	recibirMensaje(*socket, &mensajeInicializar);

	personaje->simbolo = *(char*)mensajeInicializar.Payload;
	personaje->socket = *socket;
	personaje->quantum = quantumDefault;
	pthread_mutex_lock(&semaforo_listos);
	cantPersonajes++;
	queue_push(personajesListos, personaje);
	pthread_mutex_unlock(&semaforo_listos);
}
void dirigirMovimientos() {
	Personaje *pj;
	MPS_MSG respuesta;
	while (1) {

		while (queue_is_empty(personajesListos))
			sleep(1);
		pj = queue_peek(personajesListos);
		notificarMovimientoPermitido(pj->socket);
		pj->quantum--;
		recibirMensaje(pj->socket, &respuesta);

		switch (respuesta.PayloadDescriptor) {
		case MOVIMIENTO_FINALIZADO:
			if(pj->quantum==0){
				queue_pop(personajesListos);
				pj->quantum = quantumDefault;
				queue_push(personajesListos,pj);
			}
			break;
		case BLOQUEADO:
			queue_pop(personajesListos);
			list_add(planificador->bloqueados,pj);
			break;
		case NIVEL_FINALIZADO:
			queue_pop(personajesListos);
			close(pj->socket);
			break;
		default:
			return;

		}
	}
}

