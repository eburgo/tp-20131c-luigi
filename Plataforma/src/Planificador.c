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
	log_debug(logger, "Se inicia la conexion de escucha del planificador.");
	planificador->puerto = realizarConexion(socketEscucha);
	log_debug(logger, "Conexion de escucha en socket:%d , puerto:%d.",socketEscucha,planificador->puerto);
	planificador->nombreNivel = (char*) nombreNivel;
	planificador->ip = "127.0.0.1"; //todavia no se como conseguirla
	planificador->personajes = list_create();
	planificador->bloqueados = list_create();
	log_debug(logger, "Se agrega en la lista compartida de planificadores.");
	agregarmeEnPlanificadores(planificador);
	pthread_t thread2;
	log_debug(logger, "Se crea un hilo que va a dirigir los movimientos.");
	pthread_create(&thread2, NULL, (void*) dirigirMovimientos, NULL );
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		log_debug(logger, "Bloqueados esperando una conexion.");
		if ((*socketNuevaConexion = accept(*socketEscucha, NULL, 0)) < 0) {
			log_error(logger, "Error al aceptar una conexión.");
			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;
		}
		log_info(logger, "Se acepto una conexion correctamente en el socket: %d.",*socketNuevaConexion);
		log_info(logger, "se crea un hilo para recibir al personaje.");
		pthread_create(thread, NULL, (void*) recibirPersonaje,
				socketNuevaConexion);

	}
	log_debug(logger, "Se cierra el planificar del nivel:%s",planificador->nombreNivel);
	log_destroy(logger);
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
	log_debug(logger, "Esperando msj para inicializar un personaje.");
	recibirMensaje(*socket, &mensajeInicializar);
	log_debug(logger, "Mensaje recibido, seteamos personaje.");
	personaje->simbolo = *(char*)mensajeInicializar.Payload;
	personaje->socket = *socket;
	personaje->quantum = quantumDefault;
	log_debug(logger, "Encolamos el personaje.");
	pthread_mutex_lock(&semaforo_listos);
	cantPersonajes++;
	queue_push(personajesListos, personaje);
	pthread_mutex_unlock(&semaforo_listos);
	log_debug(logger, "Personaje encolado.");
}
void dirigirMovimientos() {
	Personaje *pj;
	MPS_MSG respuesta;
	while (1) {
		log_debug(logger, "Mientras la cola de personajes este vacia, esperamos.");
		while (queue_is_empty(personajesListos))
			sleep(1);
		log_debug(logger, "Tomamos el primer personaje de la cola.");
		pj = queue_peek(personajesListos);
		log_debug(logger, "Le notificamos el movimiento permitido.");
		notificarMovimientoPermitido(pj->socket);
		pj->quantum--;
		log_debug(logger, "Esperamos la respuesta.");
		recibirMensaje(pj->socket, &respuesta);
		log_debug(logger, "Respuesta recibida:%c.",*(char*)respuesta.Payload);
		switch (respuesta.PayloadDescriptor) {
		case MOVIMIENTO_FINALIZADO:
			log_debug(logger, "El quantum del pj es:%d.",pj->quantum);
			if(pj->quantum==0){
				log_debug(logger, "se saca el personaje de la cola.");
				queue_pop(personajesListos);
				log_debug(logger, "se le asigna denuevo el quantum.");
				pj->quantum = quantumDefault;
				log_debug(logger, "Lo volvemos a encolar.");
				queue_push(personajesListos,pj);
			}else{
				log_debug(logger, "Notificamos que puede moverse nuevamente.");
				notificarMovimientoPermitido(pj->socket);
			}
			break;
		case BLOQUEADO:
			log_debug(logger, "Sacamos personaje de la cola.");
			queue_pop(personajesListos);
			log_debug(logger, "lo agregamos en la lista de bloqueados.");
			list_add(planificador->bloqueados,pj);
			break;
		case NIVEL_FINALIZADO:
			log_debug(logger, "Sacamos personaje de la cola.");
			queue_pop(personajesListos);
			log_debug(logger, "Cerramos el socket por el que nos comunicabamos con el personaje.");
			close(pj->socket);
			break;
		default:
			return;

		}
	}
}

