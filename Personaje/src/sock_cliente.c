#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define DIRECCION "127.0.0.1"
#define PUERTO 5000
#define BUFF_SIZE 1024

void recibeDatos(int* socket);
void enviaDatos(int* socket);

char buffer[BUFF_SIZE];
int nbytesRecibidos;

int main() {

	int unSocket;

	struct sockaddr_in socketInfo;


	printf("Conectando...\n");

// Crear un socket:
// AF_INET: Socket de internet IPv4
// SOCK_STREAM: Orientado a la conexion, TCP
// 0: Usar protocolo por defecto para AF_INET-SOCK_STREAM: Protocolo TCP/IPv4
	if ((unSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error al crear socket");
		return EXIT_FAILURE;
	}

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = inet_addr(DIRECCION);
	socketInfo.sin_port = htons(PUERTO);

// Conectar el socket con la direccion 'socketInfo'.
	if (connect(unSocket, (struct sockaddr*) &socketInfo, sizeof(socketInfo))
			!= 0) {
		perror("Error al conectar socket");
		return EXIT_FAILURE;
	}

	printf("Conectado!\n");

	pthread_t th1;

	pthread_create(&th1, NULL, (void*) enviaDatos, &unSocket);


	pthread_join(th1, NULL);

	close(unSocket);

	return EXIT_SUCCESS;

}

void recibeDatos(int* socket){
	while (1) {

			// Recibir hasta BUFF_SIZE datos y almacenarlos en 'buffer'.
			if ((nbytesRecibidos = recv(*socket, buffer, BUFF_SIZE, 0))
					> 0) {

				printf("Mensaje recibido: ");
				fwrite(buffer, 1, nbytesRecibidos, stdout);
				printf("\n");
				printf("Tamanio del buffer %d bytes!\n", nbytesRecibidos);
				fflush(stdout);

				if (memcmp(buffer, "fin", nbytesRecibidos) == 0) {

					printf("Server cerrado correctamente.\n");
					break;

				}

			} else {
				perror("Error al recibir datos");
				break;
			}
		}
}

void enviaDatos(int* socket){
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
