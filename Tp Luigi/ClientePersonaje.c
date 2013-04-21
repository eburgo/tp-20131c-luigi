//Desarrollar un proceso cliente liviano que permita
//enviar y recibir mensajes paquetizados (personaje y nivel)

#include <stdio.h>
#include <stdlib.h>
#include <config.h>

int main() {
	typedef struct t_nivel {
		char objetos[5];
	} Nivel;

	typedef struct t_personaje {
		char nombre[15];
		char codigo;
		Nivel niveles[5];
		int vidas;
		char ipYPuerto[20];
	} Personaje;

	//Lee la configuracion de un personaje en particular(hardcodeado)

	Personaje *personaje;
	personaje = (Personaje*)malloc(sizeof(Personaje));

	t_config *config = config_create("/home/utnso/Mario.conf");

	char *nombre = config_get_string_value(config,"nombre");

	strcpy(personaje->nombre,nombre);

	puts(personaje->nombre);

	return 0;
}
