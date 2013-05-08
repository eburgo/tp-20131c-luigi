#include "Orquestador.h"
#include "structs.h"

#define MOVIMIENTO_PERMITIDO 1;

void agregarmeEnPlanificadores(Planificador* planificador);
int iniciarPlanificador(void* nombreNivel);
int notificarMovimientoPermitido(int socketPersonaje);
