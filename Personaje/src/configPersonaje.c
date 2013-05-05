#include "configPersonaje.h"

Personaje* levantarConfiguracion(char *rutaArchivo) {

	Personaje* personaje;
	personaje = malloc(sizeof(Personaje));
	t_config *config = config_create(rutaArchivo);
	personaje->nombre = config_get_string_value(config, "nombre");
	personaje->vidas = config_get_int_value(config, "vidas");
	personaje->simbolo = config_get_string_value(config, "simbolo");
	char **niveles = config_get_array_value(config, "planDeNiveles");
	char *nombreNivel;
	personaje->listaNiveles = queue_create();
	if (niveles != NULL ) {
		int i = 0;
		while (niveles[i] != NULL) {
			nombreNivel = niveles[i];
			if (nombreNivel != NULL ) {
				Nivel *nivelAAgregar;
				nivelAAgregar = malloc(sizeof(Nivel));
				nivelAAgregar->objetos = queue_create();
				char nivelABuscar[20] = "obj[";
				strcat(nivelABuscar, nombreNivel);
				strcat(nivelABuscar, "]");
				char **objetos = config_get_array_value(config,
						nivelABuscar);
				nivelAAgregar->nombre = nombreNivel;
				int j = 0;
				while(objetos[j] != NULL) {
					queue_push(nivelAAgregar->objetos,objetos[j]);
					j++;
				}
				queue_push(personaje->listaNiveles, nivelAAgregar);
				t_list *lista = list_create();
				list_add(lista, nivelAAgregar);
			}
			i++;
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
Nivel* extraerProximoNivel(Personaje *pj) {
	return (queue_pop(pj->listaNiveles));
}

Nivel* verProximoNivel(Personaje *pj) {
	return (queue_peek(pj->listaNiveles));
}

NivelConexion* pedirNivel(Personaje* personaje, int socketOrquestador) {
	NivelConexion* nivelConexion;
	MPS_MSG mensajeAEnviar, mensajeARecibir;
	Nivel* proximoNivel = verProximoNivel(personaje);
	mensajeAEnviar.PayloadDescriptor = 1;
	mensajeAEnviar.Payload = proximoNivel->nombre;
	mensajeAEnviar.PayLoadLength = strlen(proximoNivel->nombre);
	enviarMensaje(socketOrquestador, &mensajeAEnviar);

	int respondioMensaje = -1;
	while (respondioMensaje == -1) {
		respondioMensaje = recibirMensaje(socketOrquestador, &mensajeARecibir);
	}
	t_stream* stream = malloc(sizeof(t_stream));
	stream->length = mensajeARecibir.PayLoadLength;
	stream->data = mensajeARecibir.Payload;
	nivelConexion = nivelConexion_desserializer(stream);

	close(socketOrquestador);
	return nivelConexion;
}












