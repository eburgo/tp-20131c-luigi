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

typedef struct nivelPersonaje {
	char *nombre;
	char *objetos;
} __attribute__((__packed__)) NivelPersonaje;

typedef struct personaje {
	char *nombre;
	char *simbolo;
	t_queue *listaNiveles;
	int vidas;
	char *ip;
	int puerto;
} __attribute__((__packed__)) Personaje;

typedef struct nivel {
	char *nombre;
	t_list *items;
	char *ip;
	int puerto;
	int tiempoChequeoDeadLock;
	int recovery;

} __attribute__((__packed__)) Nivel;
