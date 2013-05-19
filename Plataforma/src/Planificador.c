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
void levantarConfiguracion(char* path,int *quantum,int *tiempoAccion);
t_queue *personajesListos;
int cantPersonajes = 0;
Planificador *planificador;

extern t_dictionary *planificadores;
extern pthread_mutex_t semaforo_planificadores;
int quantumDefault=0;
int tiempoAccion=0;
pthread_mutex_t semaforo_listos = PTHREAD_MUTEX_INITIALIZER;


t_log* loggerPlanificador;

int iniciarPlanificador(void* nombreNivel) {

	char nombreOrigen[16] = "PLANIFICADOR - ";
	char* nombreLog = strcat(nombreOrigen,nombreNivel);
	loggerPlanificador = log_create("/home/utnso/planificador.log", nombreLog , true,
			LOG_LEVEL_TRACE);

	planificador = malloc(sizeof(Planificador));
	int *socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	personajesListos = queue_create();
	log_debug(loggerPlanificador, "Levantando configuracion de planificadores.");
	levantarConfiguracion("/home/utnso/git/tp-20131c-luigi/Plataforma/Planificador.config", &quantumDefault,&tiempoAccion);
	log_debug(loggerPlanificador, "El quantum se definio a:%d segundos, el tiempo entre cada acción a:%d segundos", quantumDefault, tiempoAccion);
	socketEscucha = malloc(sizeof(int));
	log_debug(loggerPlanificador, "Se inicia la conexion de escucha del planificador.");
	planificador->puerto = realizarConexion(socketEscucha);
	log_debug(loggerPlanificador, "Conexion de escucha en socket:%d , puerto:%d.",*socketEscucha,planificador->puerto);
	planificador->nombreNivel = (char*) nombreNivel;
	planificador->ip = "127.0.0.1"; //todavia no se como conseguirla
	planificador->personajes = list_create();
	planificador->bloqueados = list_create();
	log_debug(loggerPlanificador, "Se agrega en la lista compartida de planificadores.");
	agregarmeEnPlanificadores(planificador);
	pthread_t thread2;
	log_debug(loggerPlanificador, "Se crea un hilo que va a dirigir los movimientos.");
	pthread_create(&thread2, NULL, (void*) dirigirMovimientos, NULL );
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		log_debug(loggerPlanificador, "Bloqueados esperando una conexion.");
		if ((*socketNuevaConexion = accept(*socketEscucha, NULL, 0)) < 0) {
			log_error(loggerPlanificador, "Error al aceptar una conexión.");
			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;
		}
		log_info(loggerPlanificador, "Se acepto una conexion correctamente en el socket: %d.",*socketNuevaConexion);
		log_info(loggerPlanificador, "se crea un hilo para recibir al personaje.");
		pthread_create(thread, NULL, (void*) recibirPersonaje,
				socketNuevaConexion);

	}
	log_debug(loggerPlanificador, "Se cierra el planificar del nivel:%s",planificador->nombreNivel);
	log_destroy(loggerPlanificador);
	return (EXIT_SUCCESS);
}

void agregarmeEnPlanificadores(Planificador* planificador) {
	pthread_mutex_lock(&semaforo_planificadores);
	dictionary_put(planificadores, planificador->nombreNivel,
			(void*) planificador);
	pthread_mutex_unlock(&semaforo_planificadores);
}

int notificarMovimientoPermitido(int socketPersonaje) {
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = MOVIMIENTO_PERMITIDO;
	mensajeAEnviar->PayLoadLength = strlen("Movete") + 1;
	mensajeAEnviar->Payload = "Movete";
	enviarMensaje(socketPersonaje, mensajeAEnviar);
	free(mensajeAEnviar);
	return 0;
}

void recibirPersonaje(int* socket) {
	MPS_MSG mensajeInicializar;
	Personaje* personaje = malloc(sizeof(Personaje));
	log_debug(loggerPlanificador, "Esperando mensaje para inicializar un personaje.");
	recibirMensaje(*socket, &mensajeInicializar);
	log_debug(loggerPlanificador, "Mensaje recibido, seteamos personaje.");
	personaje->simbolo = mensajeInicializar.Payload;
	personaje->socket = *socket;
	personaje->quantum = quantumDefault;
	log_debug(loggerPlanificador, "Encolamos el personaje(%c).",*(char*)mensajeInicializar.Payload);
	pthread_mutex_lock(&semaforo_listos);
	cantPersonajes++;
	queue_push(personajesListos, personaje);
	pthread_mutex_unlock(&semaforo_listos);
	log_debug(loggerPlanificador, "El personaje (%s) se recibio de manera correcta, notificando encolado exitoso",personaje->simbolo);
	enviarMensaje(*socket, &mensajeInicializar);
}
void dirigirMovimientos() {
	Personaje *pj;
	MPS_MSG respuesta;
	int loggearEsperando=1;
	while (1) {
		while (queue_is_empty(personajesListos)){
			if(loggearEsperando){
				log_debug(loggerPlanificador, "Mientras la cola de personajes este vacia, esperamos.");
				loggearEsperando=0;
			}
			sleep(1);
		}
		loggearEsperando=1;
		log_debug(loggerPlanificador, "Tomamos el primer personaje de la cola.");
		pj = queue_peek(personajesListos);
		log_debug(loggerPlanificador, "Le notificamos el movimiento permitido a (%s)",pj->simbolo);
		notificarMovimientoPermitido(pj->socket);
		pj->quantum--;
		log_debug(loggerPlanificador, "Esperamos la respuesta de (%s)", pj->simbolo);
		recibirMensaje(pj->socket, &respuesta);
		log_debug(loggerPlanificador, "Respuesta recibida (%d).",respuesta.PayloadDescriptor);
		switch (respuesta.PayloadDescriptor) {
		case MOVIMIENTO_FINALIZADO:
			log_debug(loggerPlanificador, "El quantum del pj (%s) es:%d.",pj->simbolo,pj->quantum);
			if(pj->quantum==0){
				log_debug(loggerPlanificador, "se saca el personaje (%s) de la cola.",pj->simbolo);
				queue_pop(personajesListos);
				log_debug(loggerPlanificador, "se le asigna de nuevo al personaje (%s) el quantum.",pj->simbolo);
				pj->quantum = quantumDefault;
				log_debug(loggerPlanificador, "Lo volvemos a encolar a (%s)",pj->simbolo);
				queue_push(personajesListos,pj);
			}else{
				log_debug(loggerPlanificador, "Notificamos que puede moverse nuevamente.");
				notificarMovimientoPermitido(pj->socket);
			}
			break;
		case BLOQUEADO:
			log_debug(loggerPlanificador, "Sacamos personaje (%s) de la cola.",pj->simbolo);
			queue_pop(personajesListos);
			log_debug(loggerPlanificador,"En la cola de listos quedan (%d) personajes",queue_size(personajesListos));
			log_debug(loggerPlanificador, "Agregamos en la lista de bloqueados al personaje (%s).",pj->simbolo);
			list_add(planificador->bloqueados,pj);
			break;
		case NIVEL_FINALIZADO:
			log_debug(loggerPlanificador, "Sacamos personaje de la cola.");
			queue_pop(personajesListos);
			log_debug(loggerPlanificador, "Cerramos el socket por el que nos comunicabamos con el personaje.");
			close(pj->socket);
			break;
		default:
			return;

		}
		sleep(tiempoAccion);
	}
}

void levantarConfiguracion(char* path,int *quantum,int *tiempoAccion){
	t_config *config = config_create(path);
	*tiempoAccion = atoi(config_get_string_value(config, "tiempoAccion"));
	*quantum = atoi(config_get_string_value(config, "quantum"));
	config_destroy(config);

}

