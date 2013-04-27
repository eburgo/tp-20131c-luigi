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
	personaje.ip = config_get_string_value(config,"orquestador");
	// aca lo ideal seria separar la ip que llega completa, y asignar a persona.puerto el puerto
	// La separacion seria en los ":" que indica el puerto (ej: separar 192.168.0.100:5000
	// quedaria persona.ip = 192.168.0.100 y persona.puerto = 5000

	return personaje;
}

//Este main esta para probarlo, despues hay que borrarlo porque se usa desde el personaje.
int mainPrueba () {
	Personaje personaje = levantarConfiguracion("/home/utnso/git/tp-20131c-luigi/Personaje/personajeMario.conf");
	printf("Hola!");
	printf("Vidas:%d",personaje.vidas);
	puts(personaje.nombre);
	puts(personaje.ip);
	puts(personaje.simbolo);
	puts(personaje.puerto);
	puts(personaje.niveles);

	return 0;
}
