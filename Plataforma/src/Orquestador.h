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

// tal cual lo q dice. Se usa para lanzar en un hilo.
int iniciarOrquestador();
//Se usa al lanzar un hilo para crear un planificador para un nivel q le pasamos como nombre.
int iniciarUnPlanificador(char* nombre);
// envia un msj con PayloadDescriptor = ERROR_MENSAJE , para q siempre el error sea el mismo n°
void enviarMsjError(int *socket,char* msj);
//se usa en un hilo para manejar cada conexion entrante.
void manejarConexion(int* socket);
// mete un nivel en la lista de niveles y crea un hilo con "iniciarUnPlanificador"
int registrarNivel(NivelDatos *nivelDatos, int socket) ;
//agarra y busca los datos del nivel q le pide un personaje  y los mete en nivelConexion.
//devuelve -1 si no esta el nivel, -2 si no esta el planificador o 0 si se encontro.
int prepararNivelConexion(char* nombre, NivelConexion *nivelConexion);
//levanta la configuracion de los planificadores donde se inicializa quantum y tiempo de accion.
void levantarConfiguracion(char* path,int *quantum,int *tiempoAccion);


