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
#define REGISTRAR_NIVEL 2	//mensaje recibido de un nivel
#define PJ_BLOQUEADO 3
#define RESOLVER_INTERBLOQUEO 4
#define RECURSOS_LIBERADOS 9
#define CHEQUEO_INTERBLOQUEO 8
#define RECURSOS_ASIGNADOS 10
#define FINALIZO_NIVELES 20 // Notifica que el personaje termino el plan de niveles
//#define MSJ_XX 5
//#define MSJ_XX 6
//#define MSJ_XX 7

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
//Se queda escuchando mensajes del nivel
void esperarMensajesDeNivel(char* nombreNivel, int socket);
//busca el primero de la lista de personajes del planificador que corresponde
void* buscarPjAMatar(char* nombreNivel,t_list *pjsEnDeadlock);
//devuelve la ip de un socket
char * ipDelSocket(int socket);
//llama al binario koopa
void llamarKoopa();
//Obtengo la posicion del 1er personaje que fué bloqueado por unRecurso.
int buscarPersonajeQueEsteBloqueadoPor(t_list* personajesBloqueados,char* unRecurso);



