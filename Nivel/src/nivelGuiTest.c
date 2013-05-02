#include "configNivel.h"

void crearCajas(ITEM_NIVEL* item);

ITEM_NIVEL *itemsEnNivel = NULL;

int main() {

	Nivel *nivel = levantarConfiguracion(
			"/home/utnso/git/tp-20131c-luigi/Nivel/Nivel1.conf");

	int i;
	int j;

	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&i, &j);

	list_iterate(nivel->items,(void *)crearCajas);

	nivel_gui_dibujar(itemsEnNivel);

	sleep(5);

	nivel_gui_terminar();

	return 0;
}

void crearCajas(ITEM_NIVEL* item){
	CrearCaja(&itemsEnNivel, item->id, item->posx, item->posy, item->quantity);
}
