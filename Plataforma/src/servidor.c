#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/collections/queue.h>
#include <commons/socket.h>

#define DIRECCION INADDR_ANY   //INADDR_ANY representa la direccion de cualquier
//interfaz conectada con la computadora
#define PUERTO 5000
#define BUFF_SIZE 1024

void manejarConexion(int* socket);

int main() {
	int socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	t_queue *colaHilos = queue_create();

	socketEscucha = iniciarServidor(PUERTO);

	printf("Escuchando conexiones entrantes.\n");

// Aceptar una nueva conexion entrante. Se genera un nuevo socket con la nueva conexion.
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {

			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;

		}

		pthread_create(thread, NULL, (void*) manejarConexion,
				socketNuevaConexion);
		queue_push(colaHilos, thread);
	}

	while (queue_is_empty(colaHilos)) {
		pthread_join((pthread_t) queue_pop(colaHilos), NULL );
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
			if (nbytesRecibidos == 3) {
				if (memcmp(buffer, "fin", nbytesRecibidos) == 0) {
					printf("El cliente %d se desconecto correctamente.\n",
							*socket);
					close(*socket);
					free(socket);
					break;
				}
			}

		} else {
			perror("Error al recibir datos");
			break;
		}
	}
	return;
}

