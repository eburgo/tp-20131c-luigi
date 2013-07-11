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
#include <commons/socket.h>
#include "Planificador.h"
#include "Orquestador.h"



int main() {
	pthread_t thread;
	pthread_create(&thread,NULL,(void*)iniciarOrquestador,NULL);
	pthread_join(thread,NULL);
	return EXIT_SUCCESS;
}



