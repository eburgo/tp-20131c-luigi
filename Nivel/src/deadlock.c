#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <commons/log.h>
#include "configNivel.h"
#include <commons/socket.h>
#include <commons/string.h>
#include <commons/collections/queue.h>

#define CHEQUEO_INTERBLOQUEO 8

extern Nivel* nivel;
extern int socketOrquestador;
extern t_list *estadoDePersonajes;
extern t_log* logger;
extern pthread_mutex_t semaforoEstadoPersonajes;
void actualizarDisponibles(t_list *auxRecursosDisponibles, Personaje *pj);
void simularEntregas(t_list *procesosPersonajes);
ITEM_NIVEL* buscarCajaAux(t_list* auxRecursosDisponibles, char* id);
Personaje* buscarPersonaje(t_list *estadoDePersonajes, char* nombre);

void detectarInterbloqueos() {
	log_debug(logger, "HILO DE INTERBLOQUEOS");
	t_list *personajesFiltrados = NULL;
	bool necesitaRecursos(Personaje *pj) {
		return (pj->itemNecesitado != NULL );
	}
	while (1) {
		if (list_size(estadoDePersonajes) > 1) {
			pthread_mutex_lock(&semaforoEstadoPersonajes);
			log_debug(logger, "HILO DE INTERBLOQUEOS: se van a filtrar los personajes que necesitan recursos.");
			personajesFiltrados = (t_list*) list_filter(estadoDePersonajes, (void*) necesitaRecursos);
			if (list_size(personajesFiltrados) > 1) {
				log_debug(logger, "HILO DE INTERBLOQUEOS: se van a simularEntregas, tenemos (%d) personajes.", list_size(personajesFiltrados));
				simularEntregas(estadoDePersonajes);
			} else {
				log_debug(logger, "HILO DE INTERBLOQUEOS:Hay (%d) personajes que necesitan recursos, se necesitan al menos 2.", list_size(personajesFiltrados));
			}
			pthread_mutex_unlock(&semaforoEstadoPersonajes);
		} else {

			log_debug(logger, "HILO DE INTERBLOQUEOS:Hay (%d) personajes en el Nivel, no chequeamos deadlock.", list_size(estadoDePersonajes));
		}
		sleep(nivel->tiempoChequeoDeadLock);
	}

}

void simularEntregas(t_list *procesosPersonajes) {
	MPS_MSG *mensaje = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = CHEQUEO_INTERBLOQUEO;
	void mostrarEstado(Personaje *pj) {
		log_debug(logger, "el pj(%s) tiene (%d)", pj->simbolo, list_size(pj->itemsAsignados));
		if (list_size(pj->itemsAsignados) > 0) {
			log_debug(logger, "el pj(%s) esta en deadlock", pj->simbolo);
		} else {
			log_debug(logger, "el pj(%s) esta en starvation", pj->simbolo);
		}
	}
	bool estaPersonajeEnDeadlock(Personaje *pj) {
		return (list_size(pj->itemsAsignados) > 0);
	}

	int i = 0;
	t_list *auxPersonajes = list_create();
	t_list *auxRecursosDisponibles = list_create();
	int j;
	for (j = 0;j < list_size(nivel->items); j++){
		ITEM_NIVEL *itemNivelNuevo = malloc(sizeof(ITEM_NIVEL));
		ITEM_NIVEL *itemNivel= list_get(nivel->items, j);
		memcpy(itemNivelNuevo,itemNivel,sizeof(ITEM_NIVEL));
		list_add(auxRecursosDisponibles, itemNivelNuevo);
	}

	list_add_all(auxPersonajes, procesosPersonajes);
	log_debug(logger, "HILO DE INTERBLOQUEOS: empezamos a iterar para simular entregas");
	for (; i < list_size(auxPersonajes);) {

		Personaje *pj = list_get(auxPersonajes, i);
		Personaje *pj2 = list_get(procesosPersonajes, i);

		log_debug(logger, "ROGELIO (%d) (%d)", list_size(pj->itemsAsignados),list_size(pj2->itemsAsignados));

		log_debug(logger, "HILO DE INTERBLOQUEOS:obtenemos el primer pj (%s)", pj->simbolo);


		if (pj->itemNecesitado == NULL ) {
			log_debug(logger, "HILO DE INTERBLOQUEOS:(%s) no necesita recursos, simulamos que libera.", pj->simbolo);
			actualizarDisponibles(auxRecursosDisponibles, pj);
			list_remove(auxPersonajes, i);
			i = 0;
		} else {
			ITEM_NIVEL *caja = buscarCajaAux(auxRecursosDisponibles, pj->itemNecesitado);
			log_debug(logger, "HILO DE INTERBLOQUEOS: El pj (%s) necesita el recurso (%c)", pj->simbolo, caja->id);
			if (caja->quantity > 0) {
				log_debug(logger, "HILO DE INTERBLOQUEOS: La caja (%c) tiene recursos", caja->id);
				if (list_size(pj->itemsAsignados) > 0) {
					log_debug(logger, "HILO DE INTERBLOQUEOS: simulamos que el pj (%s) libera los recursos asignados. Cantidad: (%d)", pj->simbolo, list_size(pj->itemsAsignados));
					actualizarDisponibles(auxRecursosDisponibles, pj);
				} else {
					log_debug(logger, "HILO DE INTERBLOQUEOS: El pj (%s) no tiene recursos que liberar", pj->simbolo);
				}
				list_remove(auxPersonajes, i);
				i = 0;
			} else {
				log_debug(logger, "HILO DE INTERBLOQUEOS: no hay recursos del item que necesita el personaje (%s)", pj->simbolo);
				i++;
			}
		}

	}

	//los que quedaron es porque estan en deadlock o starvation

	if (list_size(auxPersonajes) > 0)
		list_iterate(auxPersonajes, (void*) mostrarEstado);
	else
		log_debug(logger, "HILO DE INTERBLOQUEOS: no hay pjs en deadlock o starvation");

	auxPersonajes = list_filter(auxPersonajes, (void*) estaPersonajeEnDeadlock);
	if (list_size(auxPersonajes) > 0) {
		if (nivel->recovery == 1) {
			log_debug(logger, "HILO DE INTERBLOQUEOS: se envia mensaje al socketOrquestadorpara resolver deadlock");
			t_stream *stream = pjsEnDeadlock_serializer(auxPersonajes);
			mensaje->Payload = stream->data;
			mensaje->PayLoadLength = stream->length;
			enviarMensaje(socketOrquestador, mensaje);
			recibirMensaje(socketOrquestador, mensaje);
			char* pjSimbolo = mensaje->Payload;
			log_debug(logger, "HILO DE INTERBLOQUEOS: El orquestador mato al pj (%s)", pjSimbolo);
			Personaje *pj = buscarPersonaje(estadoDePersonajes, pjSimbolo);
			log_debug(logger, "HILO DE INTERBLOQUEOS: vamos a liberar recursos para el pj (%s)", pj->simbolo);
			liberarRecursos(pj, socketOrquestador);

		} else {
			log_debug(logger, "HILO DE INTERBLOQUEOS: Recovery no activado");
		}
	}

	free(mensaje);

}

void actualizarDisponibles(t_list *recDisponibles, Personaje *pj) {
	t_list* itemsAsignadosAux =list_create();

	int esElRecurso(ITEM_NIVEL* recursoLista) {
		char* idABuscar = string_substring_until(&(recursoLista->id), 1);
		return string_equals_ignore_case(idABuscar, ((RecursoAsignado*) list_get(itemsAsignadosAux, 0))->nombre);
	}

	list_add_all(itemsAsignadosAux,pj->itemsAsignados);
	while (list_size(itemsAsignadosAux) != 0) {
		ITEM_NIVEL *recurso = list_find(recDisponibles, (void*) esElRecurso);
		recurso->quantity = recurso->quantity + ((RecursoAsignado*) list_get(itemsAsignadosAux, 0))->cantidad;
		list_remove(itemsAsignadosAux, 0);
	}
}

ITEM_NIVEL* buscarCajaAux(t_list* auxRecursosDisponibles, char* id) {
	int esElRecurso(ITEM_NIVEL* recursoLista) {
		char* idABuscar = string_substring_until(&(recursoLista->id), 1);
		return string_equals_ignore_case(idABuscar, id);
	}
	return list_find(auxRecursosDisponibles, (void*) esElRecurso);
}

Personaje* buscarPersonaje(t_list *estadoDePersonajes, char* nombre) {
	bool esElPersonaje(Personaje *pj) {
		return string_equals_ignore_case(pj->simbolo, nombre);
	}
	return list_find(estadoDePersonajes, (void*) esElPersonaje);
}

t_stream* pjsEnDeadlock_serializer(t_list *pjsEnDeadlock) {
	char *data = malloc(list_size(pjsEnDeadlock) * (sizeof(char) + 1));
	t_stream *stream = malloc(sizeof(t_stream));
	int offset = 0, tmp_size = 0, i;
	for (i = 0; i < list_size(pjsEnDeadlock); i++) {
		Personaje *pj;
		pj = list_get(pjsEnDeadlock, i);
		log_debug(logger, "Personaje en Deadlock a enviar (%s)", pj->simbolo);
		memcpy(data + offset, pj->simbolo, tmp_size = strlen(pj->simbolo) + 1);
		offset += tmp_size;
	}
	stream->data = data;
	stream->length = offset;
	return stream;
}
