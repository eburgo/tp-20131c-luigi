#include "configNivel.h"

Nivel* levantarConfiguracion(char *rutaArchivo) {
	Nivel *nivel;
	nivel = malloc(sizeof(Nivel));
	nivel->cajas = queue_create();
	Caja *cajaNivel;
	t_config *config = config_create(rutaArchivo);
	nivel->nombre = config_get_string_value(config, "Nombre");
	nivel->recovery = config_get_int_value(config, "Recovery");
	nivel->tiempoChequeoDeadLock = config_get_int_value(config,
			"TiempoChequeoDeadlock");

	int masCajas = 1;
	int i = 1;
	while (masCajas) {

		cajaNivel = malloc(sizeof(Caja));
		//Armamos el string de la caja
		//que vamos a buscar
		char c[2];
		//Convertimos el int a un char
		sprintf(c, "%d", i);
		char caja[10] = "Caja";
		strcat(caja, c);
		char *datosCaja = config_get_string_value(config, caja);

		if (datosCaja) {
			cajaNivel->nombre = caja;
			cajaNivel->nombreObjeto = strsep(&datosCaja, ",");
			cajaNivel->simboloObjeto = strsep(&datosCaja, ",");
			cajaNivel->cantidad = atoi(strsep(&datosCaja, ","));
			cajaNivel->posX = atoi(strsep(&datosCaja, ","));
			cajaNivel->posY = atoi(strsep(&datosCaja, ","));
			queue_push(nivel->cajas,cajaNivel);
			free(datosCaja);
			i++;
		} else {
			masCajas = 0;
		}
	}

	//La funcion strtok sirve para separar strings, aca la usamos para
	//separar la IP y el PUERTO
	char *ipCompleta = config_get_string_value(config, "orquestador");
	nivel->ip = strtok(ipCompleta, ":");
	nivel->puerto = strtok(NULL, ":");

	return nivel;
}

//Este main esta para probarlo, despues hay que borrarlo porque se usa desde el personaje.
int main() {
	Nivel *nivel = levantarConfiguracion(
			"/home/utnso/git/tp-20131c-luigi/Nivel/Nivel1.conf");
	printf("Nivel cargado correctamente!\n");
	printf("Nombre:%s\n", nivel->nombre);
	printf("Recovery:%d\n", nivel->recovery);
	printf("TiempoChequeoDeadLock:%d\n", nivel->tiempoChequeoDeadLock);
	printf("Ip:%s\n", nivel->ip);
	printf("Puerto:%s\n", nivel->puerto);
	int i = 1;
	while (!queue_is_empty(nivel->cajas)) {
		Caja *caja = queue_pop(nivel->cajas);
		printf("--Nombre Caja:%s\n", caja->nombre);
		printf("--Nombre Objeto:%s\n", caja->nombreObjeto);
		printf("--Simbolo:%s\n", caja->simboloObjeto);
		printf("--Cantidad::%d\n", caja->cantidad);
		printf("--Pos X:%d\n", caja->posX);
		printf("--Pos Y:%d\n", caja->posY);
		i++;
	}

	return 0;
}
