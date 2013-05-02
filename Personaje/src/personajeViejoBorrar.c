#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <commons/socket.h>
#include "configPersonaje.h"

#define DIRECCION "127.0.0.1"
#define PUERTO 5000
#define BUFF_SIZE 1024
#define SOLICITUDNIVEL 101

void recibeDatos(int* socket);
void enviaDatos(int* socket);
void solicitudNivel(Personaje* personaje);

char buffer[BUFF_SIZE];
int nbytesRecibidos;

int mainn() {

	//int unSocket;

	Personaje *personaje = levantarConfiguracion("/home/utnso/git/tp-20131c-luigi/Personaje/personajeMario.conf");

	printf("Conectando...\n");

	//unSocket = conectarAlServidor(DIRECCION, PUERTO);

	printf("Conectado!\n");

	solicitudNivel(personaje);

	//close(unSocket);

	return EXIT_SUCCESS;

}


void solicitudNivel(Personaje *personaje) {
	socket_t sOrquestador,sNivel;
	MPS_MSG msjEnv,msjRecvN;
	msjEnv.PayloadDescriptor = SOLICITUDNIVEL;
	msjEnv.PayLoadLength = sizeof(msjEnv.Payload);
	// aca le paso todo el nivel, pero supongo que solo mandamos el nombre
	msjEnv.Payload = proximoNivel(personaje);

	// Se conecta al orquestador solicitando el nivel
	sOrquestador = conectarAlServidor(personaje->ip,atoi(personaje->puerto));
	enviarMensaje(sOrquestador,&msjEnv);

	int rMensaje = -1;
	while (rMensaje == -1){
	// recibe ip/puerto del nivel
		rMensaje = recibirMensaje(sOrquestador,&msjRecvN);
	}

	// tambien del planificador de ese nivel
    // mejor hacerlo en 2 mensajes separados

	/*
	rMensaje = -1;
	while (rMensaje == -1){
		rMensaje = recibirMensaje(sOrquestador,&msjRecvP);
	}
	*/

	// Se conecta al nivel
	char* payload = msjRecvN.Payload;
	personaje->ip = strsep(&payload, ":");
	personaje->puerto = payload;
	sNivel = conectarAlServidor(personaje->ip,atoi(personaje->puerto));
	if (sNivel == -1)
		printf("Error");
	else
		printf("Conectado al nivel");

	/*
	// Se conecta al planificador y queda a la espera de instrucciones
	payload = msjRecvP.Payload;
	personaje->ip = strsep(&payload, ":");
	personaje->puerto = payload;
	// sPlanificador = conectarAlServidor(personaje->ip,atoi(personaje->puerto));
	*/

	close(sOrquestador);
	return;
}


void recibeDatos(int* socket) {
	//Implementacion de la recepcion de datos. Probablemente
	//con la funcion recibirMensaje de socket.h
}

void enviaDatos(int* socket) {
	while (1) {

		scanf("%s", buffer);

		// Enviar los datos leidos por teclado a traves del socket.
		if (send(*socket, buffer, strlen(buffer), 0) >= 0) {
			printf("Datos enviados!\n");

			if (strcmp(buffer, "fin") == 0) {

				printf("Cliente cerrado correctamente.\n");
				close(*socket);
				break;

			}

		} else {
			perror("Error al enviar datos. Server no encontrado.\n");
			break;
		}
	}
}
