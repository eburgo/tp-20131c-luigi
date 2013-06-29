#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/socket.h>
#include <commons/string.h>
#include "Planificador.h"

#define MOVIMIENTO_PERMITIDO 1
#define MOVIMIENTO_FINALIZADO 3
#define BLOQUEADO 8
#define FINALIZADO 4
#define OBTUVO_RECURSO 6
#define MUERTE_PERSONAJE 7

#define TRUE 1
#define FALSE 0


int recibirPersonajes(Planificador *planificador);
int manejarPersonajes(Planificador *planificador);
int notificarMovimientoPermitido(Personaje *personaje);


//Globales
extern int quantumDefault;
extern int tiempoAccion;
extern t_log* loggerOrquestador;

sem_t sem_test;
pthread_mutex_t semaforo_listos = PTHREAD_MUTEX_INITIALIZER;

extern t_list *personajes;

int iniciarPlanificador(Planificador* planificador) {
	pthread_t threadRecibir, threadManejar;
	planificador->sem = malloc(sizeof(sem_t));
	sem_init(planificador->sem,0,0);
	log_debug(loggerOrquestador, "Se procede a generar hilo de recepcion de personajes para planificador (%s)", planificador->nombreNivel);
	pthread_create(&threadRecibir, NULL, (void*) recibirPersonajes, planificador);
	log_debug(loggerOrquestador, "Se procede a generar hilo de manejo de personajes para planificador (%s)", planificador->nombreNivel);
	pthread_create(&threadManejar, NULL, (void*) manejarPersonajes, planificador);
	pthread_join(threadRecibir, NULL );
	pthread_join(threadManejar, NULL );
	return 0;
}
int recibirPersonajes(Planificador *planificador) {
	int *socketNuevaConexion;
	MPS_MSG *mensaje;
	char nombreOrigen[16] = "PLANIFICADOR - ";
	char* nombreLog = strcat(nombreOrigen, planificador->nombreNivel);
	t_log *log = log_create("/home/utnso/planificador.log", nombreLog, true, LOG_LEVEL_TRACE);
	while (1) {
		socketNuevaConexion = malloc(sizeof(int));
		log_debug(log, "Se procede a escuchar conexiones de personajes en el planificador (%s) en el socket (%d)", planificador->nombreNivel, planificador->socketEscucha);
		if ((*socketNuevaConexion = accept(planificador->socketEscucha, NULL, 0)) < 0) {
			log_error(log, "Error al aceptar una conexión.");
			return EXIT_FAILURE;
		}
		mensaje = malloc(sizeof(MPS_MSG));
		recibirMensaje(*socketNuevaConexion, mensaje);
		log_debug(log, "El personaje (%s) entro al nivel", (char*) mensaje->Payload);

        // Agrega los personajes que estan ejecutandose
		if ((buscarSimboloPersonaje(personajes, mensaje->Payload)) == -1){
		list_add(personajes,mensaje->Payload);
		}
		// ............................................

		Personaje *personaje = malloc(sizeof(Personaje));
		personaje->simbolo = (char*) mensaje->Payload;
		personaje->quantum = quantumDefault;
		personaje->socket = *socketNuevaConexion;

		log_debug(log, "El personaje (%s) se encolara a la cola de listos", personaje->simbolo);
		pthread_mutex_lock(&semaforo_listos);
		list_add(planificador->personajes, personaje);
		log_debug(log,"\n\ntamaño de la lista (%d)!! \n\n",list_size(planificador->personajes));
		queue_push(planificador->listos, personaje);
		log_debug(log,"\n\ntamaño de la cola (recibirPersonajes) (%d)!! \n\n",queue_size(planificador->listos));
		FD_SET(*socketNuevaConexion, planificador->set);
		log_debug(log,"paso el set");
		pthread_mutex_unlock(&semaforo_listos);
		sem_post(planificador->sem);
		enviarMensaje(personaje->socket, mensaje); //para confirmarle q inicializo bien;
		free(socketNuevaConexion);
		free(mensaje);
	}
	log_destroy(log);
	return 0;
}
int manejarPersonajes(Planificador *planificador) {
	MPS_MSG *mensaje;
	fd_set readSet;
	FD_ZERO(&readSet);
	int i;
	char nombreOrigen[16] = "PLANIFICADOR - ";
	char* nombreLog = strcat(nombreOrigen, planificador->nombreNivel);
	t_log *log = log_create("/home/utnso/planificador.log", nombreLog, true, LOG_LEVEL_TRACE);
	while (1) {

		log_debug(log, "Esperando personajes");
		sem_wait(planificador->sem);
		Personaje *personaje = queue_peek(planificador->listos);
		log_debug(log, "Personaje que se movera (%s)", personaje->simbolo);

		log_debug(log, "Notificando movimiento permitido a (%s)", personaje->simbolo);
		notificarMovimientoPermitido(personaje);
		readSet=*planificador->set;
			select(200, &readSet, NULL, NULL, NULL );
			for (i = 0; i < list_size(planificador->personajes); i++) {
				Personaje *personajeAux = list_get(planificador->personajes, i);
				if (personaje->socket == personajeAux->socket){
					break;
				}
				else if (FD_ISSET(personajeAux->socket, &readSet)) {
				}
			}
		mensaje = malloc(sizeof(MPS_MSG));
		recibirMensaje(personaje->socket, mensaje);
		log_debug(log, "Mensaje recibido de (%s) es el descriptor (%d)", personaje->simbolo, mensaje->PayloadDescriptor);
		while (personaje->quantum > 1 && mensaje->PayloadDescriptor == MOVIMIENTO_FINALIZADO) {

			personaje->quantum--;
			sleep(tiempoAccion);
			log_debug(log, "Notificando movimiento permitido a (%s)", personaje->simbolo);
			notificarMovimientoPermitido(personaje);
			free(mensaje);
			mensaje = malloc(sizeof(MPS_MSG));
			recibirMensaje(personaje->socket, mensaje);
			log_debug(log, "Mensaje recibido de (%s) es el descriptor (%d)", personaje->simbolo, mensaje->PayloadDescriptor);
		}
		switch (mensaje->PayloadDescriptor) {
		case BLOQUEADO:
			log_debug(log, "El personaje (%s) se bloqueo por el recurso (%s)", personaje->simbolo,(char*)mensaje->Payload);
			queue_pop(planificador->listos);
			personaje->causaBloqueo=(char*)mensaje->Payload;
			list_add(planificador->bloqueados, personaje);
			break;
		case FINALIZADO:
			log_debug(log, "el personaje (%s) finalizo el nivel", personaje->simbolo);
			sacarPersonaje(planificador, personaje,FALSE);
			close(personaje->socket);
			break;
		case OBTUVO_RECURSO:
			log_debug(log, "El personaje (%s) obtuvo un recurso, finaliza su quantum automaticamente.", personaje->simbolo);
			queue_pop(planificador->listos);
			personaje->quantum = quantumDefault;
			queue_push(planificador->listos, personaje);
			sem_post(planificador->sem);
			break;
		case MUERTE_PERSONAJE:
			log_debug(log, "El personaje  murio. Lo sacamos del planificador.");
			sacarPersonaje(planificador, personaje, FALSE);
			close(personaje->socket);
			break;
		case MOVIMIENTO_FINALIZADO:
			log_debug(log, "Al personaje (%s) se le termino el quantum", personaje->simbolo);
			queue_pop(planificador->listos);
			personaje->quantum = quantumDefault;
			queue_push(planificador->listos, personaje);
			sem_post(planificador->sem);
			break;
		default:
			log_debug(log, "El personaje (%s) envio un mensaje no esperado, se cierra la conexion.", personaje->simbolo);
			sacarPersonaje(planificador, personaje,TRUE);
			close(personaje->socket);
			break;
		}
		free(mensaje);
		sleep(tiempoAccion);
	}
	log_destroy(log);
	return 0;
}
int notificarMovimientoPermitido(Personaje* personaje) {
	MPS_MSG mensaje;

	mensaje.PayloadDescriptor = MOVIMIENTO_PERMITIDO;
	mensaje.PayLoadLength = sizeof(char);
	mensaje.Payload = "P";

	enviarMensaje(personaje->socket, &mensaje);
	return 0;
}

void sacarPersonaje(Planificador *planificador,Personaje *personaje,int porError){
	int esElPersonaje(Personaje *pj){
		return string_equals_ignore_case(pj->simbolo, personaje->simbolo);
	}
	Personaje *pj=NULL;
	pj=list_remove_by_condition(planificador->bloqueados,(void*)esElPersonaje);
	if(!pj){
		pj=queue_pop(planificador->listos);
	}
	pj=list_remove_by_condition(planificador->personajes,(void*)esElPersonaje);
	FD_CLR(pj->socket,planificador->set);
	if(!porError)
		notificarMovimientoPermitido(pj);
	free(pj);
}
int buscarSimboloPersonaje(t_list *self, char* nombrePersonaje) {
	t_link_element *aux = self->head;
	if(aux == NULL)
		return -1;

	char* nombre = (char*) aux->data;
	int index = 0;

	while ((aux != NULL) && (!(string_equals_ignore_case(nombre, nombrePersonaje)))){
		aux = aux->next;
		if (aux != NULL){
			nombre = (char*) aux->data;
		}
		index++;
	}

	if(string_equals_ignore_case(nombre, nombrePersonaje))
		return index;
	else
		return -1;
}
