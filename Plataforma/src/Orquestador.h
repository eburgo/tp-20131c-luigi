#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/socket.h>

#define DIRECCION INADDR_ANY   //INADDR_ANY representa la direccion de cualquier
								//interfaz conectada con la computadora
#define PUERTO 5000
#define BUFF_SIZE 1024

//------ TIPOS DE MENSAJES!!------
#define ERROR_MENSAJE 0
#define PJ_PIDE_NIVEL 1 	//pj pide ip y puerto del nivel y planificador
#define REGISTRAR_NIVEL 2	//mensaje de un nivel
#define PJ_BLOQUEADO 3
#define RESOLVER_INTERBLOQUEO 4
//#define MSJ_XX 5
//#define MSJ_XX 6
//#define MSJ_XX 7
//#define MSJ_XX 8
//#define MSJ_XX 9


int iniciarOrquestador();
int iniciarUnPlanificador(char* nombre);
void enviarMsjError(int *socket,char* msj);
void manejarConexion(int* socket);
int registrarNivel(NivelDatos *nivelDatos, int socket) ;
int prepararNivelConexion(char* nombre, NivelConexion *nivelConexion);


