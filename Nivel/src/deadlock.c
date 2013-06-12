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


extern Nivel* nivel;
extern int socketOrquestador;
extern t_list *estadoDePersonajes;
extern t_log* logger;
extern pthread_mutex_t semaforo_estadoPjs;
void actualizarDisponibles(t_list *auxRecDisponibles,ProcesoPersonaje *pj);
void simularEntregas(t_list *procesosPersonajes);
ITEM_NIVEL* buscarCaja2(char* id);

void detectarInterbloqueos(){

	int tiempoDefinido=10;
	t_list *pjsFiltrados;
	bool necesitaRecursos(ProcesoPersonaje *pj){
		return (pj->itemNecesitado!=NULL);
	}
	while(1){
		pthread_mutex_lock(&semaforo_estadoPjs);
		pjsFiltrados=(t_list*)list_filter(estadoDePersonajes,(void*)necesitaRecursos);
		simularEntregas(pjsFiltrados);
		pthread_mutex_unlock(&semaforo_estadoPjs);
		sleep(tiempoDefinido);
	}

}

void simularEntregas(t_list *procesosPersonajes){
	MPS_MSG mensaje;
	mensaje.PayloadDescriptor = 8;
	mensaje.PayLoadLength = 2;
	mensaje.Payload = "8";
	void mostrarEstado(ProcesoPersonaje *pj){
		if(list_size(pj->itemsAsignados)>0){
			log_debug(logger, "el pj(%s) esta en deadlock",pj->nombre);
		}
		else {
			log_debug(logger, "el pj(%s) esta en starvation",pj->nombre);
		}
	}
	bool pjEnDeadlock(ProcesoPersonaje *pj){
		return (list_size(pj->itemsAsignados)>0);
	}
	int i=0;
	t_list *auxPersonajes=list_create();
	t_list *auxRecDisponibles=list_create();

	list_add_all(auxRecDisponibles,nivel->items);

	list_add_all(auxPersonajes,procesosPersonajes);
	for(;i<list_size(auxPersonajes);){

		ProcesoPersonaje *pj=list_get(auxPersonajes,i);


		ITEM_NIVEL *caja=buscarCaja2(pj->itemNecesitado);
		if(caja->quantity>0){

			if(list_size(pj->itemsAsignados)>0)
				actualizarDisponibles(auxRecDisponibles,pj);
			else
				log_debug(logger, "el pj (%s) no tiene recursos que liberar",pj->nombre);

			list_remove(auxPersonajes,i);
			i=0;
		}
		else{

			i++;
		}
	}
	//los que quedaron es porque estan en deadlock o starvation

	if (list_size(auxPersonajes)>0)
		list_iterate(auxPersonajes,(void*)mostrarEstado);
	else
		log_debug(logger, "no hay pjs en deadlock o starvation");

	auxPersonajes=list_filter(auxPersonajes,(void*)pjEnDeadlock);
	if(list_size(auxPersonajes)>0){
		log_debug(logger, "se envia mensaje al socketOrquestador (%d)para resolver deadlock",socketOrquestador);
		enviarMensaje(socketOrquestador,&mensaje);
		recibirMensaje(socketOrquestador,&mensaje);
	}

}

void actualizarDisponibles(t_list *recDisponibles,ProcesoPersonaje *pj){
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


