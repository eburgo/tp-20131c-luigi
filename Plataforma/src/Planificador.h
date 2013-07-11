#include "Orquestador.h"
#include "structs.h"
#include <commons/config.h>

//Inicializa un planificador
int iniciarPlanificador(Planificador* planificador);
void sacarPersonaje(Planificador *planificador,Personaje *personaje,int porError);
void sacarPersonajeFueraDeTurno(Planificador *planificador, Personaje *personaje, int leRespondoAlPersonaje);
int buscarSimboloPersonaje(t_list *self, char* nombrePersonaje);

#define TRUE 1
#define FALSE 0
