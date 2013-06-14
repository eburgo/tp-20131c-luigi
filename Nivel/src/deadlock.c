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
extern pthread_mutex_t semaforo_estadoPjs;
void actualizarDisponibles(t_list *auxRecDisponibles,Personaje *pj);
void simularEntregas(t_list *procesosPersonajes);
ITEM_NIVEL* buscarCaja2(char* id);
Personaje* buscarPj(t_list *estadoDePersonajes,char* nombre);

void detectarInterbloqueos(){
	log_debug(logger, "HILO DE INTERBLOQUEOS");
	t_list *pjsFiltrados = NULL;
	bool necesitaRecursos(Personaje *pj){
		return (pj->itemNecesitado!=NULL);
	}
	while(1){
		if(list_size(estadoDePersonajes)>1){
			pthread_mutex_lock(&semaforo_estadoPjs);
			log_debug(logger, "HILO DE INTERBLOQUEOS: se van a filtrar los personajes que necesitan recursos.");
			pjsFiltrados=(t_list*)list_filter(estadoDePersonajes,(void*)necesitaRecursos);
			if (list_size(pjsFiltrados) > 0) {
				log_debug(logger, "HILO DE INTERBLOQUEOS: se van a simularEntregas, tenemos (%d) personajes.",list_size(pjsFiltrados));
				simularEntregas(pjsFiltrados);
			}else{
				log_debug(logger,"HILO DE INTERBLOQUEOS:Hay (%d) personajes que necesitan recursos, se necesitan al menos 2.",list_size(pjsFiltrados));
			}
			pthread_mutex_unlock(&semaforo_estadoPjs);
		} else {
			log_debug(logger,"HILO DE INTERBLOQUEOS:Hay (%d) personajes en el Nivel, no chequeamos deadlock.");
		}
		sleep(nivel->tiempoChequeoDeadLock);
	}

}

void simularEntregas(t_list *procesosPersonajes) {
	MPS_MSG *mensaje = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = CHEQUEO_INTERBLOQUEO;
	void mostrarEstado(Personaje *pj) {
		if (list_size(pj->itemsAsignados) > 0) {
			log_debug(logger, "el pj(%s) esta en deadlock", pj->simbolo);
		} else {
			log_debug(logger, "el pj(%s) esta en starvation", pj->simbolo);
		}
	}
	bool pjEnDeadlock(Personaje *pj) {
		return (list_size(pj->itemsAsignados) > 0);
	}


		int i = 0;
		log_debug(logger,
				"HILO DE INTERBLOQUEOS: vamos a crear las listas auxiliaress");
		t_list *auxPersonajes = list_create();
		t_list *auxRecDisponibles = list_create();
		log_debug(logger,
				"HILO DE INTERBLOQUEOS: se copian las listas del nivel a listas auxiliares");
		list_add_all(auxRecDisponibles, nivel->items);

		list_add_all(auxPersonajes, procesosPersonajes);
		log_debug(logger,
				"HILO DE INTERBLOQUEOS: empezamos a iterar para simular entregas");
		for (; i < list_size(auxPersonajes);) {
			log_debug(logger, "HILO DE INTERBLOQUEOS:obtenemos el primero pj");
			Personaje *pj = list_get(auxPersonajes, i);

			log_debug(logger,
					"HILO DE INTERBLOQUEOS: buscamos la caja del item que necesita");
			ITEM_NIVEL *caja = buscarCaja2(pj->itemNecesitado);
			if (caja->quantity > 0) {
				log_debug(logger,
						"HILO DE INTERBLOQUEOS: esa caja tiene recursos");
				if (list_size(pj->itemsAsignados) > 0) {
					log_debug(logger,
							"HILO DE INTERBLOQUEOS: simulamos que el pj libera los recursos");
					actualizarDisponibles(auxRecDisponibles, pj);
				} else {
					log_debug(logger,
							"HILO DE INTERBLOQUEOS:simulamos que libera, pero no tiene nada para liberar");
					log_debug(logger,
							"el pj (%s) no tiene recursos que liberar",
							pj->simbolo);
				}
				log_debug(logger,
						"HILO DE INTERBLOQUEOS:lo sacamos de la lsita auxiliar");
				list_remove(auxPersonajes, i);
				i = 0;
			} else {
				log_debug(logger,
						"HILO DE INTERBLOQUEOS: no hay recursos del item que necesita este personaje");
				i++;
			}

		}

		//los que quedaron es porque estan en deadlock o starvation

		if (list_size(auxPersonajes) > 0)
			list_iterate(auxPersonajes, (void*) mostrarEstado);
		else
			log_debug(logger, "no hay pjs en deadlock o starvation");

		auxPersonajes = list_filter(auxPersonajes, (void*) pjEnDeadlock);
		if (list_size(auxPersonajes) > 0) {
			if (nivel->recovery == 1) {
				log_debug(logger,"se envia mensaje al socketOrquestadorpara resolver deadlock");
				t_stream *stream = pjsEnDeadlock_serializer(auxPersonajes);
				mensaje->Payload = stream->data;
				mensaje->PayLoadLength = stream->length;
				enviarMensaje(socketOrquestador, mensaje);
				recibirMensaje(socketOrquestador, mensaje);
				char* pjSimbolo = mensaje->Payload;
				log_debug(logger,"El orquestador mato al pj (%s)", pjSimbolo);
				Personaje *pj = buscarPj(estadoDePersonajes, pjSimbolo);
				log_debug(logger,"vamos a liberar recursos para el pj (%s)", pj->simbolo);
				log_debug(logger,"tamaño de la cola del pj (%s): %d",pj->simbolo,queue_size(pj->recursosObtenidos));
				liberarRecursos(pj,socketOrquestador);

			} else {
				log_debug(logger, "Recovery no activado");
			}
		}

	free(mensaje);

}

void actualizarDisponibles(t_list *recDisponibles,Personaje *pj){
	int esElRecurso(ITEM_NIVEL* recursoLista) {
		char* idABuscar = string_substring_until(&(recursoLista->id), 1);
		return string_equals_ignore_case(idABuscar,((RecursoAsignado*)list_get(pj->itemsAsignados,0))->nombre );
	}

	while(list_size(pj->itemsAsignados)!=0){

		ITEM_NIVEL *recurso = list_find(recDisponibles,(void*)esElRecurso);

		recurso->quantity = recurso->quantity +((RecursoAsignado*)list_get(pj->itemsAsignados,0))->cantidad;

		list_remove(pj->itemsAsignados,0);
	}
}

ITEM_NIVEL* buscarCaja2(char* id) {
	int esElRecurso(ITEM_NIVEL* recursoLista) {
		char* idABuscar = string_substring_until(&(recursoLista->id), 1);
		return string_equals_ignore_case(idABuscar, id);
	}
	return list_find(nivel->items, (void*) esElRecurso);
}

Personaje* buscarPj(t_list *estadoDePersonajes,char* nombre){
	bool esElPersonaje(Personaje *pj){
			return string_equals_ignore_case(pj->simbolo, nombre);
		}
	return list_find(estadoDePersonajes,(void*)esElPersonaje);
}

t_stream* pjsEnDeadlock_serializer(t_list *pjsEnDeadlock){
	char *data = malloc(	list_size(pjsEnDeadlock)	*	(sizeof(char) + 1) 	);
	t_stream *stream = malloc(sizeof(t_stream));
	int offset = 0, tmp_size = 0, i;
	for(i=0;i<list_size(pjsEnDeadlock);i++){
		Personaje *pj;
		pj=list_get(pjsEnDeadlock,i);
		log_debug(logger,"Serializando.... simbolo de pj : %s \n",pj->simbolo);
		memcpy(data, pj->simbolo, tmp_size = strlen(pj->simbolo) + 1);
		offset += tmp_size;
	}
	stream->data = data;
	stream->length = offset;
	return stream;
}
