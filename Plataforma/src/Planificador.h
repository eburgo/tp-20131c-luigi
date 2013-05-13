#include "Orquestador.h"
#include "structs.h"

#define MOVIMIENTO_PERMITIDO 1
#define MOVIMIENTO_FINALIZADO 2
#define BLOQUEADO 3
#define NIVEL_FINALIZADO 4
void agregarmeEnPlanificadores(Planificador* planificador);
int iniciarPlanificador(void* nombreNivel);
int notificarMovimientoPermitido(int socketPersonaje);
