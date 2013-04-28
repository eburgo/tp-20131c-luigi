#include "configPersonaje.h"

Personaje levantarConfiguracion(char *rutaArchivo) {

	Personaje personaje;
	t_config *config = config_create(rutaArchivo);
	personaje.nombre = config_get_string_value(config,"nombre");
	personaje.vidas = config_get_int_value(config,"vidas");
	personaje.simbolo = config_get_string_value(config,"simbolo");
	char *niveles = config_get_string_value(config,"planDeNiveles");
	// Aca faltaria procesar los niveles, hacer un for y fijarse por cada nivel los diferentes
	// objetos que necesita agarrar, y luego asignar los niveles a personaje.niveles.

	//La funcion strtok sirve para separar strings, aca la usamos para
	//separar la IP y el PUERTO
	char *ipCompleta = config_get_string_value(config,"orquestador");
	personaje.ip = strtok(ipCompleta,":");
	personaje.puerto = strtok(NULL,":");

	return personaje;
}

//Este main esta para probarlo, despues hay que borrarlo porque se usa desde el personaje.
int mainTest () {
	Personaje personaje = levantarConfiguracion("/home/utnso/git/tp-20131c-luigi/Personaje/personajeMario.conf");
	printf("Hola!");
	printf("Vidas:%d",personaje.vidas);
	puts(personaje.nombre);
	puts(personaje.ip);
	puts(personaje.puerto);
	puts(personaje.simbolo);
	puts(personaje.niveles);

	return 0;
}
