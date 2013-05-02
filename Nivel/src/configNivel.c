#include "configNivel.h"
void paraIterar(ITEM_NIVEL* item);
Nivel* levantarConfiguracion(char *rutaArchivo) {
	Nivel *nivel;
	nivel = malloc(sizeof(Nivel));
	nivel->items = list_create();
	ITEM_NIVEL *itemNivel;
	t_config *config = config_create(rutaArchivo);
	nivel->nombre = config_get_string_value(config, "Nombre");
	nivel->recovery = config_get_int_value(config, "Recovery");
	nivel->tiempoChequeoDeadLock = config_get_int_value(config,
			"TiempoChequeoDeadlock");

	int masCajas = 1;
	int i = 1;
	while (masCajas) {

		itemNivel = malloc(sizeof(ITEM_NIVEL));
		//Armamos el string de la caja
		//que vamos a buscar
		char c[2];
		//Convertimos el int a un char
		sprintf(c, "%d", i);
		char caja[10] = "Caja";
		strcat(caja, c);
		char *datosCaja = config_get_string_value(config, caja);
		if (datosCaja) {
			strsep(&datosCaja, ",");
			itemNivel->id =* strsep(&datosCaja, ",");
			itemNivel->quantity = atoi(strsep(&datosCaja, ","));
			itemNivel->posx = atoi(strsep(&datosCaja, ","));
			itemNivel->posy = atoi(strsep(&datosCaja, ","));
			itemNivel->item_type = 'C';
			list_add(nivel->items,itemNivel);
			free(datosCaja);
			i++;
		} else {
			masCajas = 0;
		}
	}

	//La funcion strsep sirve para separar strings, aca la usamos para
	//separar la IP y el PUERTO
	char *ipCompleta = config_get_string_value(config, "orquestador");
	nivel->ip = strsep(&ipCompleta, ":");
	nivel->puerto = atoi(ipCompleta);

	return nivel;
}

//Este main esta para probarlo, despues hay que borrarlo porque se usa desde el personaje.
int mainTest() {
	Nivel *nivel = levantarConfiguracion(
			"/home/utnso/git/tp-20131c-luigi/Nivel/Nivel1.conf");
	printf("Nivel cargado correctamente!\n");
	printf("Nombre:%s\n", nivel->nombre);
	printf("Recovery:%d\n", nivel->recovery);
	printf("TiempoChequeoDeadLock:%d\n", nivel->tiempoChequeoDeadLock);
	printf("Ip:%s\n", nivel->ip);
	printf("Puerto:%d\n", nivel->puerto);
	puts("items!: \n");
	/*paso esa funcion paraIterar que va a recibir como parametro
	 * nivel->items. Es como cuando pasamos una funcion al pthread_create
	 */
	list_iterate(nivel->items,(void *)paraIterar);
	return 0;
}

void paraIterar(ITEM_NIVEL* item){

	printf("id: %c \n", item->id);
	printf("cantidad: %d \n", item->quantity);
	printf("posx: %d \n", item->posx);
	printf("posy: %d \n", item->posy);
	printf("tipo: %c \n\n",item->item_type);

}
