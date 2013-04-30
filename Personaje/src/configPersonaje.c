#include "configPersonaje.h"

Personaje* levantarConfiguracion(char *rutaArchivo) {

	Personaje* personaje;
	personaje = malloc(sizeof(Personaje));
	t_config *config = config_create(rutaArchivo);
	personaje->nombre = config_get_string_value(config, "nombre");
	personaje->vidas = config_get_int_value(config, "vidas");
	personaje->simbolo = config_get_string_value(config, "simbolo");
	char *niveles = config_get_string_value(config, "planDeNiveles");
	//Esta parte le saca los corchetes al string que recibe como String
	//de los niveles, despues va agarrando cada nombre de nivel (que
	//vienen separados por coma, luego los busca en la configuracion
	//agregandole el obj[NivelNumero]. Luego los va metiendo en la lista
	//del personaje.
	niveles = strtok(niveles, "[");
	niveles = strtok(niveles, "]");
	char *nombreNivel;
	personaje->listaNiveles = queue_create();
	if (niveles != NULL ) {
		while ((nombreNivel = strsep(&niveles, ",")) != NULL ) {
			Nivel *nivelAAgregar;
			nivelAAgregar = malloc(sizeof(Nivel));
			char nivelABuscar[20] = "obj[";
			strcat(nivelABuscar, nombreNivel);
			strcat(nivelABuscar, "]");
			char *objetosNivel = config_get_string_value(config,nivelABuscar);
			nivelAAgregar->nombre = nombreNivel;
			nivelAAgregar->objetos = objetosNivel;
			queue_push(personaje->listaNiveles,nivelAAgregar);
			t_list *lista = list_create();
			list_add(lista,nivelAAgregar);
			free(niveles);
		}
	}
	//La funcion strtok sirve para separar strings, aca la usamos para
	//separar la IP y el PUERTO
	char *ipCompleta = config_get_string_value(config, "orquestador");
	personaje->ip = strtok(ipCompleta, ":");
	personaje->puerto = strtok(NULL, ":");

	return personaje;
}

//Este main esta para probarlo, despues hay que borrarlo porque se usa desde el personaje.
int main(){
	Nivel *nivel;
	Personaje *personaje = levantarConfiguracion(
			"/home/utnso/git/tp-20131c-luigi/Personaje/personajeMario.conf");
	printf("Personaje cargado correctamente!\n");
	printf("Vidas:%d\n", personaje->vidas);
	printf("Nombre:%s\n", personaje->nombre);
	printf("IP:%s\n", personaje->ip);
	printf("Puerto:%s\n", personaje->puerto);
	printf("Simbolo:%s\n", personaje->simbolo);
	printf("Lista de niveles:\n");
	while(!queue_is_empty(personaje->listaNiveles)){
		nivel = (Nivel*)queue_pop(personaje->listaNiveles);
		printf("-- Nivel:%s\n",nivel->nombre);
		printf("---- > Objetivos:%s\n",nivel->objetos);
	}
	return 0;
}
