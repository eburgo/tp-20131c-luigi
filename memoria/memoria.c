#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#include "memoria.h"

t_list* listaParticiones;

t_memoria crear_memoria(int tamanio) {
	char* segmento = malloc(tamanio*sizeof(char)+1);
	memset(segmento,' ',(tamanio*sizeof(char)));
	// seteo porque la siguiente direccion apuntaba a basura (en el debug no lo hacia)
	// y me la contaba como dato
	memset((char*)((int)segmento+tamanio),'\0',1);

	listaParticiones = list_create();
	return segmento;
}

int almacenar_particion(t_memoria segmento, char id, int tamanio, char* contenido) {

	if (strlen(segmento) < tamanio){
		printf("Error: espacio insuficiente \n");
		return -1;
	}

	int indexID = buscarID(listaParticiones,id);
	if (indexID != -1){
		printf("Error: id duplicado \n");
		return -1;
	}

	if(tamanioPeorEspacio(segmento, listaParticiones) < strlen(contenido)){
		printf("Error: no hay una particion para almacenar la solicitud \n");
		return 0;
	}

	int direccion = grabarContenido(segmento, listaParticiones, contenido);

	if (buscarDireccion(listaParticiones, direccion) != -1){
		int index = buscarPeorParticion(listaParticiones);
		t_particion* particionLibre = list_get(listaParticiones,index);
		int tamanioParticion = particionLibre->tamanio;

		t_particion *particion = malloc( sizeof(t_particion) );
		particion->id = id;
		particion->inicio = (int)particionLibre->dato - (int)segmento;
		particion->dato = particionLibre->dato;
		particion->libre = false;
		particion->tamanio = tamanio;
		list_remove_and_destroy_element(listaParticiones, index,free);
		list_add(listaParticiones,particion);

		if(tamanioParticion > tamanio){
			t_particion *pLibre = malloc( sizeof(t_particion) );
			pLibre->id = ' ';
			pLibre->inicio = (int)particion->dato + particion->tamanio - (int)segmento;
			pLibre->dato = (char*)((int)particion->dato + particion->tamanio);
			pLibre->libre = true;
			pLibre->tamanio = tamanioParticion - tamanio;
			list_add(listaParticiones,pLibre);
		}
	}else{
		t_particion *particion = malloc( sizeof(t_particion) );
		particion->id = id;
		particion->inicio = direccion - (int)segmento;
		particion->dato = (char*)direccion;
		particion->libre = false;
		particion->tamanio = tamanio;
		list_add(listaParticiones,particion);
	}

	return 1;
}

int eliminar_particion(t_memoria segmento, char id) {
	int indexID = buscarID(listaParticiones, id);
	if (indexID != -1){
		t_particion* particion = (t_particion*)list_get(listaParticiones, indexID);
		memset(particion->dato,' ',particion->tamanio);
		particion->id = ' ';
 		particion->libre = true;
	}
	else{
		printf("Error: no se encontro una particion con ese ID \n");
		return 0;
	}
	return 1;
}

void liberar_memoria(t_memoria segmento) {
	free(segmento);
	list_clean(listaParticiones);
}

t_list* particiones(t_memoria segmento) {
	t_list* list = list_create();

	if (list_size(listaParticiones) == 0){
		t_particion *new = malloc( sizeof(t_particion) );
		new->id = ' ';
		new->inicio = 0;
		new->dato = segmento;
		new->libre = true;
		new->tamanio = strlen(segmento);
		list_add(list,new);
	}
	else{
		int direccionUltimoSegmentoVacio = buscarMayorDireccion(listaParticiones);
		if ((direccionUltimoSegmentoVacio - (int) segmento) < strlen(segmento)){
			t_particion *new = malloc( sizeof(t_particion) );
			new->id = ' ';
			new->inicio = direccionUltimoSegmentoVacio - (int) segmento;
			new->dato = (char *) direccionUltimoSegmentoVacio;
			new->libre = true;
			new->tamanio = strlen((char*) direccionUltimoSegmentoVacio);
			list_add(listaParticiones,new);
		}

		list=listaParticiones;
		list_sort(list, (void*) _particion_menor);
	}
    return list;
}

int grabarContenido(t_memoria segmento, t_list *listaParticiones , char* contenido){
	int direccion = buscarPeorEspacio(segmento, listaParticiones);
	memcpy((char*)direccion,contenido,strlen(contenido));
return direccion;
}

int buscarID(t_list *self, char id) {
	t_link_element *aux = self->head;
	if(aux == NULL)
		return -1;

	t_particion* particion = (t_particion*)aux->data;
	int index = 0;

	while ((aux != NULL) & (particion->id != id)){
		aux = aux->next;
		if (aux != NULL){
			particion = (t_particion*)aux->data;
		}
		index++;
	}

	if(particion->id == id)
		return index;
	else
		return -1;
}

int buscarDireccion(t_list *self, int direccion) {
	t_link_element *aux = self->head;
	if(aux == NULL)
		return -1;

	t_particion* particion = (t_particion*)aux->data;
	int index = 0;

	while ((aux != NULL) & ((int)particion->dato != direccion)){
		aux = aux->next;
		if (aux != NULL){
			particion = (t_particion*)aux->data;
		}
		index++;
	}

	if((int)particion->dato == direccion)
		return index;
	else
		return -1;
}

int buscarMayorDireccion(t_list *self) {
	t_link_element *aux = self->head;
	if(aux == NULL)
		return -1;

	t_particion* particion;
	int mayorDireccion = 0;
	int direccion;

	while ((aux!=NULL)){
		particion = (t_particion*)aux->data;
		direccion = (int)particion->dato + particion->tamanio;
		if (direccion > mayorDireccion){
			mayorDireccion = direccion;
		}
		aux = aux->next;
	}

	return mayorDireccion;
}

int buscarPeorParticion(t_list *self) {
	t_link_element *aux = self->head;
	if(aux == NULL)
		return -1;

	t_particion* particion;
	int mayorEspacio = 0;
	int tamanio = 0;
	int index = 0;
	int indexMayorEspacio = -1;

	while ((aux != NULL)){
		particion = (t_particion*)aux->data;
		if (particion->libre==true){
			tamanio = (int)particion->tamanio;
			if (tamanio > mayorEspacio){
				mayorEspacio = tamanio;
				indexMayorEspacio = index;
			}
		}
		aux=aux->next;
		index++;
	}

	if (indexMayorEspacio == -1)
		return -1;
	else
		return indexMayorEspacio;
}

int buscarPeorEspacio(t_memoria segmento, t_list *listaParticiones) {
	int dirEspacioLibre;
	int espacioLibre = 0;
	int index = buscarPeorParticion(listaParticiones);
	int dirUltimaParticion = buscarMayorDireccion(listaParticiones);

	if (dirUltimaParticion == -1){
		dirEspacioLibre = (int)segmento;
	}else{
		int segmentoLibre = dirUltimaParticion;
		if (index == -1){
			dirEspacioLibre = segmentoLibre;
		}else{
			char* direccion = (char*)segmentoLibre;
			while ((*direccion == ' ') & (*direccion != '\0')) {
				espacioLibre++;
				direccion++;
			}

			t_particion* particionLibre = list_get(listaParticiones,index);
			if (particionLibre->tamanio >= espacioLibre)
				dirEspacioLibre = (int)particionLibre->dato;
			else
				dirEspacioLibre = segmentoLibre;
		}
	}

return dirEspacioLibre;
}

int tamanioPeorEspacio(t_memoria segmento, t_list *listaParticiones) {
	int espacioLibre=0;

	char* direccion = (char *)buscarPeorEspacio(segmento,listaParticiones);
	int index = buscarDireccion(listaParticiones, (int)direccion);
	if (index == -1){
		while ((*direccion == ' ') & (*direccion != '\0')) {
			espacioLibre++;
			direccion++;
		}
	}else{
		t_particion *particionLibre = list_get(listaParticiones, index);
		espacioLibre = (int)particionLibre->tamanio;
	}

return espacioLibre;
}

bool _particion_menor(t_particion *list, t_particion *listaParticiones) {
	return list->inicio < listaParticiones->inicio;
}
