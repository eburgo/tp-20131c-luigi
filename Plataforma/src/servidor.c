#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/collections/queue.h>

#define DIRECCION INADDR_ANY   //INADDR_ANY representa la direccion de cualquier
//interfaz conectada con la computadora
#define PUERTO 5000
#define BUFF_SIZE 1024

void manejarConexion(int* socket);

int main() {

	int socketEscucha;
	int *socketNuevaConexion;

	struct sockaddr_in socketInfo;

	int optval = 1;

	// Crear un socket:
	// AF_INET: Socket de internet IPv4
	// SOCK_STREAM: Orientado a la conexion, TCP
	// 0: Usar protocolo por defecto para AF_INET-SOCK_STREAM: Protocolo TCP/IPv4
	if ((socketEscucha = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}

	// Hacer que el SO libere el puerto inmediatamente luego de cerrar el socket.
	setsockopt(socketEscucha, SOL_SOCKET, SO_REUSEADDR, &optval,
			sizeof(optval));

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = DIRECCION; //Notar que aca no se usa inet_addr()
	socketInfo.sin_port = htons(PUERTO);

// Vincular el socket con una direccion de red almacenada en 'socketInfo'.
	if (bind(socketEscucha, (struct sockaddr*) &socketInfo, sizeof(socketInfo))
			!= 0) {

		perror("Error al bindear socket escucha");
		return EXIT_FAILURE;
	}

	//pthread_t* thread= malloc(sizeof(pthread_t)*5);

// Escuchar nuevas conexiones entrantes.

	if (listen(socketEscucha, 10) != 0) {

		perror("Error al poner a escuchar socket");
		return EXIT_FAILURE;

	}

	printf("Escuchando conexiones entrantes.\n");

// Aceptar una nueva conexion entrante. Se genera un nuevo socket con la nueva conexion.
	pthread_t *thread;
	t_queue *colaHilos = queue_create();
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {

			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;

		}

		pthread_create(thread, NULL, (void*) manejarConexion,
				socketNuevaConexion);
		queue_push(colaHilos,thread);
	}

	//pthread_t *threadASacar;
	while(queue_is_empty(colaHilos)) {
		//threadASacar = queue_pop(colaHilos);
		pthread_join((pthread_t)queue_peek(colaHilos),NULL);
	}

	close(socketEscucha);

	return EXIT_SUCCESS;
}

void manejarConexion(int* socket) {
	char buffer[BUFF_SIZE];
	int nbytesRecibidos;
	while (1) {
		// Recibir hasta BUFF_SIZE datos y almacenarlos en 'buffer'.
		if ((nbytesRecibidos = recv(*socket, buffer, BUFF_SIZE, 0)) > 0) {

			printf("Datos recibido de este socket : %d\n", *socket);
			fwrite(buffer, 1, nbytesRecibidos, stdout);
			printf("\n");
			printf("Tamanio del buffer %d bytes!\n", nbytesRecibidos);
			fflush(stdout);

			if (memcmp(buffer, "fin", nbytesRecibidos) == 0) {

				printf("Server cerrado correctamente.\n");
				close(*socket);
				free(*socket);
				break;

			}

		} else {
			perror("Error al recibir datos");
			break;
		}
	}
	return;
}

