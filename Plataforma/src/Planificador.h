#include "Orquestador.h"
#include "structs.h"
#include <commons/config.h>

#define MOVIMIENTO_PERMITIDO 1
#define MOVIMIENTO_FINALIZADO 3
#define BLOQUEADO 8
#define NIVEL_FINALIZADO 4
void agregarmeEnPlanificadores(Planificador* planificador);
int iniciarPlanificador(void* nombreNivel);
int notificarMovimientoPermitido(int socketPersonaje);
