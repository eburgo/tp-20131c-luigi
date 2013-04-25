/*
 * conex.h
 *
 *  Created on: 22/04/2013
 *      Author: Esteban
 */

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

/****                 D E F I N I C I O N   D E   C O N S T A N T E S                                   ****/

#define LOG_INFO 					1
#define LOG_WARN					2
#define LOG_ERROR 					3
#define LOG_DEBUG					4


/****       D E F I N I C I O N   D E   E S T R U C T U R A S   Y   T I P O S   D E   D A T O S         ****/

typedef int socket_t;

// TIPO DE DATO DE LOS BUFFER
typedef unsigned char* buffer_t;

/* Estructuras NIPC */

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





/****                    P R O T O T I P O   D E   F U N C I O N E S                     ****/
int printL(int cTipoLog, char* cpMensaje);
char *horaActual();
socket_t iniciarServidor(int puerto);
socket_t aceptarConexion(socket_t iSocket);
socket_t conectarAlServidor(char *IPServidor, int PuertoServidor);
int enviarMensaje(socket_t Socket, MPS_MSG *mensaje);
int recibirMensaje(int Socket, MPS_MSG *mensaje);
int recibirInfo(int Socket, buffer_t Buffer, int CantidadARecibir);
int enviarInfo(int Socket, buffer_t Buffer, int CantidadAEnviar);



#endif /* CONEX_H_ */
