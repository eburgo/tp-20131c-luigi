// TIPO DE DATO DE LOS BUFFER
#ifndef CONEX_H_
#define CONEX_H_


#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/unistd.h>

typedef int socket_t;

typedef unsigned char* buffer_t;

/* Estructuras NIPC */

typedef struct {
	int length;
	char* data;
} t_stream ;

typedef struct nivelConexion {
	char* ipPlanificador;
	int puertoPlanificador;
	char* ipNivel;
	int puertoNivel;
} __attribute__((__packed__)) NivelConexion;

typedef struct nivelDatos {
	char* nombre;
	char* ip;
	int puerto;
} __attribute__((__packed__)) NivelDatos;

typedef struct posicion {
	int x; // Ubicacion H en el eje coordenadas
	int y; // Ubicacion S en el eje coordenadas ;)
} __attribute__((__packed__)) Posicion;

typedef struct _MPS_MSG
{
 int8_t PayloadDescriptor;
 int16_t PayLoadLength;
 void *Payload;
} __attribute__((__packed__)) MPS_MSG;


typedef struct _MPS_MSG_HEADER
{
   int8_t PayloadDescriptor;
   int16_t PayLoadLength;
} __attribute__((__packed__)) MPS_MSG_HEADER;

socket_t iniciarServidor(int puerto);
socket_t conectarAlServidor(char *IPServidor, int PuertoServidor);
int enviarMensaje(socket_t Socket, MPS_MSG *mensaje);
int recibirMensaje(int Socket, MPS_MSG *mensaje);
int recibirInfo(int Socket, buffer_t Buffer, int CantidadARecibir);
int enviarInfo(int Socket, buffer_t Buffer, int CantidadAEnviar);
t_stream* nivelConexion_serializer(NivelConexion *self);
NivelConexion* nivelConexion_desserializer(t_stream *stream);
t_stream* NivelDatos_serializer(NivelDatos *self);
NivelDatos* NivelDatos_desserializer(t_stream *stream);

// Inicia el servidor y devuelve el puerto asignado aleatoreamente.
int realizarConexion(int* socketEscucha);

#endif /* CONEX_H_ */
