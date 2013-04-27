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

#define DIRECCION "127.0.0.1"
#define PUERTO 5000
#define BUFF_SIZE 1024

void recibeDatos(int* socket);
void enviaDatos(int* socket);

char buffer[BUFF_SIZE];
int nbytesRecibidos;

int main() {

	int unSocket;

	printf("Conectando...\n");

	unSocket = conectarAlServidor(DIRECCION, PUERTO);

	printf("Conectado!\n");

	pthread_t th1;

	pthread_create(&th1, NULL, (void*) enviaDatos, &unSocket);

	pthread_join(th1, NULL );

	close(unSocket);

	return EXIT_SUCCESS;

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
