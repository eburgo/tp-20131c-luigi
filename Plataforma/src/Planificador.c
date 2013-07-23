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
#define INGRESA_NIVEL 9

int recibirPersonajes(Planificador *planificador);
int manejarPersonajes(Planificador *planificador);
int notificarMovimientoPermitido(Personaje *personaje);
void imprimirListas(Planificador *planificador, t_log *log);
char* imprimirLista(char* header, t_list* lista, t_log *log, int desde);
//Globales
extern int quantumDefault;
extern unsigned long tiempoAccion;
extern t_log* loggerOrquestador;
extern t_list *personajes;

int iniciarPlanificador(Planificador* planificador) {
	pthread_t threadRecibir, threadManejar;
	planificador->sem = malloc(sizeof(sem_t));
	sem_init(planificador->sem, 0, 0);
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
	log_debug(log, "Se procede a escuchar conexiones de personajes en el planificador (%s)", planificador->nombreNivel);
	while (1) {
		socketNuevaConexion = malloc(sizeof(int));
		if ((*socketNuevaConexion = accept(planificador->socketEscucha, NULL, 0)) < 0) {
			log_error(log, "Error al aceptar una conexión.");
			return EXIT_FAILURE;
		}
		mensaje = malloc(sizeof(MPS_MSG));
		recibirMensaje(*socketNuevaConexion, mensaje);
		if (mensaje->PayloadDescriptor != INGRESA_NIVEL) {
			log_error(log, "El personaje que acaba de entrar envio un mensaje con descriptor incorrecto: (%d)", mensaje->PayloadDescriptor);
		}

		// Agrega los personajes que estan ejecutandose en todos los niveles, para que el orquestador
		// sepa cuando ejecutar Koopa.
		if ((buscarSimboloPersonaje(personajes, mensaje->Payload)) == -1) {
			list_add(personajes, mensaje->Payload);
		}

		Personaje *personaje = malloc(sizeof(Personaje));
		personaje->simbolo = (char*) mensaje->Payload;
		personaje->quantum = quantumDefault;
		personaje->socket = *socketNuevaConexion;

		log_debug(log, "El personaje (%s) entro al nivel y se encolara a la cola de listos", personaje->simbolo);

		pthread_mutex_lock(&planificador->semaforo_personajes);
		list_add(planificador->personajes, personaje);
		pthread_mutex_unlock(&planificador->semaforo_personajes);
		pthread_mutex_lock(&planificador->semaforo_listos);
		queue_push(planificador->listos, personaje);
		pthread_mutex_unlock(&planificador->semaforo_listos);
		FD_SET(*socketNuevaConexion, planificador->set);
		imprimirListas(planificador, log);
		log_debug(log, "Informamos al personaje (%s) que se inicializo correctamente. ", personaje->simbolo);
		enviarMensaje(personaje->socket, mensaje);
		sem_post(planificador->sem);
		//free(socketNuevaConexion);
		free(mensaje);
	}
	log_destroy(log);
	return 0;
}
void manejarMensaje(Personaje* personaje, Planificador *planificador, t_log *log) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	recibirMensaje(personaje->socket, mensaje);
	log_debug(log, "Mensaje recibido de (%s) es el descriptor (%d)", personaje->simbolo, mensaje->PayloadDescriptor);
	while (personaje->quantum > 1 && mensaje->PayloadDescriptor == MOVIMIENTO_FINALIZADO) {
		personaje->quantum--;
		usleep(tiempoAccion);
		log_debug(log, "Notificando movimiento permitido a (%s)", personaje->simbolo);
		notificarMovimientoPermitido(personaje);
		free(mensaje);
		mensaje = malloc(sizeof(MPS_MSG));
		recibirMensaje(personaje->socket, mensaje);
		log_debug(log, "Mensaje recibido de (%s) es el descriptor (%d)", personaje->simbolo, mensaje->PayloadDescriptor);
	}
	switch (mensaje->PayloadDescriptor) {
	case OBTUVO_RECURSO:
		log_debug(log, "El personaje (%s) obtuvo un recurso, finaliza su quantum automaticamente.", personaje->simbolo);
		pthread_mutex_lock(&planificador->semaforo_listos);
		queue_pop(planificador->listos);
		personaje->quantum = quantumDefault;
		queue_push(planificador->listos, personaje);
		pthread_mutex_unlock(&planificador->semaforo_listos);
		sem_post(planificador->sem);
		break;
	case FINALIZADO:
		log_debug(log, "el personaje (%s) finalizo el nivel", personaje->simbolo);
		sacarPersonajeFueraDeTurno(planificador, personaje, FALSE);
		imprimirListas(planificador, log);
		close(personaje->socket);
		break;
	case MUERTE_PERSONAJE:
		log_debug(log, "El personaje (%s) murio. Lo sacamos del planificador.", personaje->simbolo);
		sacarPersonajeFueraDeTurno(planificador, personaje, TRUE);
		imprimirListas(planificador, log);
		close(personaje->socket);
		break;
	default:
		log_debug(log, "El personaje (%s) envio un mensaje no esperado, se cierra la conexion.", personaje->simbolo);
		int indice = buscarSimboloPersonaje(personajes, personaje->simbolo);
		list_remove_and_destroy_element(personajes, indice, free);
		sacarPersonajeFueraDeTurno(planificador, personaje, TRUE);
		imprimirListas(planificador, log);
		close(personaje->socket);
		break;
	}
	free(mensaje);
	usleep(tiempoAccion);
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
		readSet = *planificador->set;
		struct timeval espera;
		espera.tv_sec = 0;
		espera.tv_usec = 0;
		select(200, &readSet, NULL, NULL, &espera);
		for (i = 0; i < list_size(planificador->personajes); i++) {
			pthread_mutex_lock(&planificador->semaforo_personajes);
			Personaje *personajeAux = list_get(planificador->personajes, i);
			pthread_mutex_unlock(&planificador->semaforo_personajes);
			if (FD_ISSET(personajeAux->socket, &readSet)) {
				manejarMensaje(personajeAux, planificador, log);
			}
		}
		pthread_mutex_lock(&planificador->semaforo_listos);
		Personaje *personaje = queue_peek(planificador->listos);
		pthread_mutex_unlock(&planificador->semaforo_listos);
		imprimirListas(planificador, log);
		log_debug(log, "Notificando movimiento permitido a (%s)", personaje->simbolo);
		notificarMovimientoPermitido(personaje);
		mensaje = malloc(sizeof(MPS_MSG));
		recibirMensaje(personaje->socket, mensaje);
		log_debug(log, "Mensaje recibido de (%s) es el descriptor (%d)", personaje->simbolo, mensaje->PayloadDescriptor);
		while (personaje->quantum > 1 && mensaje->PayloadDescriptor == MOVIMIENTO_FINALIZADO) {
			personaje->quantum--;
			usleep(tiempoAccion);
			log_debug(log, "Notificando movimiento permitido a (%s)", personaje->simbolo);
			notificarMovimientoPermitido(personaje);
			free(mensaje);
			mensaje = malloc(sizeof(MPS_MSG));
			recibirMensaje(personaje->socket, mensaje);
			log_debug(log, "Mensaje recibido de (%s) es el descriptor (%d)", personaje->simbolo, mensaje->PayloadDescriptor);
		}
		switch (mensaje->PayloadDescriptor) {
		case BLOQUEADO:
			log_debug(log, "El personaje (%s) se bloqueo por el recurso (%s)", personaje->simbolo, (char*) mensaje->Payload);
			pthread_mutex_lock(&planificador->semaforo_listos);
			queue_pop(planificador->listos);
			pthread_mutex_unlock(&planificador->semaforo_listos);
			personaje->causaBloqueo = (char*) mensaje->Payload;
			pthread_mutex_lock(&planificador->semaforo_bloqueados);
			list_add(planificador->bloqueados, personaje);
			pthread_mutex_unlock(&planificador->semaforo_bloqueados);
			imprimirListas(planificador, log);
			break;
		case FINALIZADO:
			log_debug(log, "el personaje (%s) finalizo el nivel", personaje->simbolo);
			sacarPersonaje(planificador, personaje, FALSE);
			imprimirListas(planificador, log);
			close(personaje->socket);
			break;
		case OBTUVO_RECURSO:
			log_debug(log, "El personaje (%s) obtuvo un recurso, finaliza su quantum automaticamente.", personaje->simbolo);
			pthread_mutex_lock(&planificador->semaforo_listos);
			queue_pop(planificador->listos);
			personaje->quantum = quantumDefault;
			queue_push(planificador->listos, personaje);
			pthread_mutex_unlock(&planificador->semaforo_listos);
			sem_post(planificador->sem);
			break;
		case MUERTE_PERSONAJE:
			log_debug(log, "El personaje (%s) murio. Lo sacamos del planificador.", personaje->simbolo);
			sacarPersonaje(planificador, personaje, TRUE);
			imprimirListas(planificador, log);
			close(personaje->socket);
			break;
		case MOVIMIENTO_FINALIZADO:
			log_debug(log, "Al personaje (%s) se le termino el quantum", personaje->simbolo);
			pthread_mutex_lock(&planificador->semaforo_listos);
			queue_pop(planificador->listos);
			personaje->quantum = quantumDefault;
			queue_push(planificador->listos, personaje);
			pthread_mutex_unlock(&planificador->semaforo_listos);
			sem_post(planificador->sem);
			break;
		default:
			log_debug(log, "El personaje (%s) envio un mensaje no esperado, se cierra la conexion.", personaje->simbolo);
			int indice = buscarSimboloPersonaje(personajes, personaje->simbolo);
			list_remove_and_destroy_element(personajes, indice, free);
			sacarPersonaje(planificador, personaje, FALSE);
			imprimirListas(planificador, log);
			close(personaje->socket);
			break;
		}
		free(mensaje);
		usleep(tiempoAccion);
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

int notificarMuerte(Personaje* personaje) {
	MPS_MSG mensaje;

	mensaje.PayloadDescriptor = MUERTE_PERSONAJE;
	mensaje.PayLoadLength = sizeof(char);
	mensaje.Payload = "M";

	enviarMensaje(personaje->socket, &mensaje);
	return 0;
}

void sacarPersonaje(Planificador *planificador, Personaje *personaje, int leRespondoAlPersonaje) {
	int esElPersonaje(Personaje *pj) {
		return string_equals_ignore_case(pj->simbolo, personaje->simbolo);
	}
	Personaje *pj = NULL;
	pthread_mutex_lock(&planificador->semaforo_bloqueados);
	pj = list_remove_by_condition(planificador->bloqueados, (void*) esElPersonaje);
	pthread_mutex_unlock(&planificador->semaforo_bloqueados);
	if (!pj) {
		pthread_mutex_lock(&planificador->semaforo_listos);
		pj = list_remove_by_condition(planificador->listos->elements, (void*) esElPersonaje);
		pthread_mutex_unlock(&planificador->semaforo_listos);
	}
	pthread_mutex_lock(&planificador->semaforo_personajes);
	pj = list_remove_by_condition(planificador->personajes, (void*) esElPersonaje);
	pthread_mutex_unlock(&planificador->semaforo_personajes);
	FD_CLR(pj->socket, planificador->set);
	if (leRespondoAlPersonaje) {
		notificarMuerte(pj);
	}
	free(pj);
}
void sacarPersonajeFueraDeTurno(Planificador *planificador, Personaje *personaje, int leRespondoAlPersonaje) {
	int esElPersonaje(Personaje *pj) {
		return string_equals_ignore_case(pj->simbolo, personaje->simbolo);
	}
	Personaje *pj = NULL;
	pthread_mutex_lock(&planificador->semaforo_bloqueados);
	pj = list_remove_by_condition(planificador->bloqueados, (void*) esElPersonaje);
	pthread_mutex_unlock(&planificador->semaforo_bloqueados);
	pthread_mutex_lock(&planificador->semaforo_listos);
	if (!pj) {
		pj = list_remove_by_condition(planificador->listos->elements, (void*) esElPersonaje);
		sem_wait(planificador->sem);
	}
	pthread_mutex_unlock(&planificador->semaforo_listos);
	pthread_mutex_lock(&planificador->semaforo_personajes);
	pj = list_remove_by_condition(planificador->personajes, (void*) esElPersonaje);
	pthread_mutex_unlock(&planificador->semaforo_personajes);
	FD_CLR(pj->socket, planificador->set);
	if (leRespondoAlPersonaje) {
		notificarMuerte(pj);
	}
	free(pj);
}
int buscarSimboloPersonaje(t_list *self, char* nombrePersonaje) {
	t_link_element *aux = self->head;
	if (aux == NULL )
		return -1;

	char* nombre = (char*) aux->data;
	int index = 0;

	while ((aux != NULL )&& (!(string_equals_ignore_case(nombre, nombrePersonaje)))){
	aux = aux->next;
	if (aux != NULL) {
		nombre = (char*) aux->data;
	}
	index++;
}

	if (string_equals_ignore_case(nombre, nombrePersonaje))
		return index;
	else
		return -1;
}
void imprimirListas(Planificador *planificador, t_log *log) {
	char* listosLog = imprimirLista("Listos:", planificador->listos->elements, log, 1);
	char* bloqueadosLog = imprimirLista("Bloqueados:", planificador->bloqueados, log, 0);
	if (!queue_is_empty(planificador->listos)) {
		log_debug(log, "Ejecutando:->%s / %s / %s", ((Personaje*) queue_peek(planificador->listos))->simbolo, listosLog, bloqueadosLog);
	} else {
		log_debug(log, "%s / % s", listosLog, bloqueadosLog);
	}
}

char* imprimirLista(char* header, t_list* lista, t_log *log, int indice) {
	char *listaLog = string_new();
	string_append(&listaLog, header);
	for (; indice < list_size(lista); indice++) {
		string_append(&listaLog, "-> ");
		string_append(&listaLog, string_duplicate(((Personaje*) list_get(lista, indice))->simbolo));
	}
	string_append(&listaLog, ".");
	return listaLog;
}
