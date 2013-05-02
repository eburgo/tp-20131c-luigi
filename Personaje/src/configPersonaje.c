#include "configPersonaje.h"

Personaje* levantarConfiguracion(char *rutaArchivo) {

	Personaje* personaje;
	personaje = malloc(sizeof(Personaje));
	t_config *config = config_create(rutaArchivo);
	personaje->nombre = config_get_string_value(config, "nombre");
	personaje->vidas = config_get_int_value(config, "vidas");
	personaje->simbolo = config_get_string_value(config, "simbolo");
	char **niveles = config_get_array_value(config,"planDeNiveles");
	char *nombreNivel;
	personaje->listaNiveles = queue_create();
	if (niveles != NULL ) {
		int i;
		for(i=0;i<sizeof(niveles);i++){
			nombreNivel = niveles[i];
			if(nombreNivel != NULL){
			Nivel *nivelAAgregar;
			nivelAAgregar = malloc(sizeof(Nivel));
			char nivelABuscar[20] = "obj[";
			strcat(nivelABuscar, nombreNivel);
			strcat(nivelABuscar, "]");
			char *objetosNivel = config_get_string_value(config, nivelABuscar);
			nivelAAgregar->nombre = nombreNivel;
			nivelAAgregar->objetos = objetosNivel;
			queue_push(personaje->listaNiveles, nivelAAgregar);
			t_list *lista = list_create();
			list_add(lista, nivelAAgregar);
			}
		}
	}
	//La funcion strtok sirve para separar strings, aca la usamos para
	//separar la IP y el PUERTO
	char *ipCompleta = config_get_string_value(config, "orquestador");
	personaje->ip = strsep(&ipCompleta, ":");
	personaje->puerto = atoi(ipCompleta);

	return personaje;
}
/*
 * Devuelve la cantidad de vidas despues de sumar
 */
int sumarVida(Personaje *pj) {
	return (pj->vidas = pj->vidas + 1);
}
/*
 * devuelve la cantidad de vidas despues de restar
 * o -1 si ya no tiene mas vidas
 */
int sacarVida(Personaje *pj) {
	if (pj->vidas > 0)
		return (pj->vidas = pj->vidas - 1);
	return (-1);
}
/*
 * devuelve -1 si ya no tiene mas niveles
 * o 0 si se devolio el nivel
 */
Nivel* proximoNivel(Personaje *pj) {
	return (queue_pop(pj->listaNiveles));
}

//Este main esta para probarlo, despues hay que borrarlo porque se usa desde el personaje.
int mainTest() {
	Nivel *nivel = malloc(sizeof(Nivel));
	Personaje *personaje = levantarConfiguracion(
			"/home/utnso/git/tp-20131c-luigi/Personaje/personajeMario.conf");
	printf("Personaje cargado correctamente!\n");
	printf("Vidas:%d\n", personaje->vidas);
	printf("Nombre:%s\n", personaje->nombre);
	printf("IP:%s\n", personaje->ip);
	printf("Puerto:%d\n", personaje->puerto);
	printf("Simbolo:%s\n", personaje->simbolo);
	printf("Lista de niveles:\n");
	while ((nivel = proximoNivel(personaje)) != NULL ) {
		printf("-- Nivel:%s\n", nivel->nombre);
		printf("---- > Objetivos:%s\n", nivel->objetos);
	}
	puts("no hay mas nivels :(");

	return 0;
}
