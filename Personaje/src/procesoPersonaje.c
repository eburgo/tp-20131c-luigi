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
#include <commons/log.h>
#include "configPersonaje.h"

// ----------     Funciones      -----------
// Recorre un nivel, recibe el socket del nivel y el socket del planificador de ese nivel.
void recorrerNivel(int socketNivel, int socketPlanificador);
void manejarSenial(int n);
//Procesamiento del personaje!
int procesar();
// Procesamiento que se realiza cuando se pierde una vida.
int perderVida();
// Conectar al orquestador, devuelve el socketOrquestador, y si hay error devuelve 1.
int conectarAlOrquestador();
// Recorre la lista de niveles del personaje.
int recorrerNiveles(int socketOrquestador);
// Le avisa al nivel que libere los recursos
void liberarRecursos(int socketNivel);
// Le notifica al planificador que se murio el personaje.
void notificarMuerte(int socketPlanificador);
// Finaliza el proceso
void finalizar();
// Levantar la configuracion del personaje, recibe el path.
int levantarPersonaje(char* path);
// Enviar un mensaje al nivel y a su planificador notificando de su finalizacion
int avisarFinalizacion(int socketNivel,int socketPlanificador);

//Globales
Personaje* personaje;
t_log* logger;
char* path;
int socketPlanificador;
int socketNivel;

//Tipos de mensaje a enviar
#define FINALIZAR 4

int main(int argc, char *argv[]) {

	logger = log_create("/home/utnso/personaje.log", "TEST", true,
			LOG_LEVEL_TRACE);
	log_info(logger,
			"Log creado con exito, se procede a loguear el proceso Personaje");

	log_debug(logger, "Chequeando el path del personaje...");
	if (argv[1] == NULL ) {
		log_error(logger, "El path del personaje no puede ser vacio.");
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	path = argv[1];

	int levantarConfig = levantarPersonaje(path);
	if (levantarConfig == 1) {
		return EXIT_FAILURE;
	}

	signal(SIGUSR1, manejarSenial);
	signal(SIGTERM, manejarSenial);

	int resultado = procesar();
	if (resultado == 1) {
		return EXIT_FAILURE;
	}

	finalizar();
	return EXIT_SUCCESS;
}

void manejarSenial(int n) {
	switch (n) {
	case SIGUSR1:
		personaje->vidas = personaje->vidas + 1;
		break;
	case SIGTERM:
		log_debug(logger, "El personaje %s recibio la señal SIGTERM.",personaje->nombre);
		int resultado = perderVida();
		if(resultado == 1) {
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
		break;
	}
}

void recorrerNivel(int socketNivel, int socketPlanificador) {

}

int perderVida() {
	if (sacarVida(personaje) > 0) {
		log_debug(logger, "El personaje %s perdio una vida",personaje->nombre);
		log_debug(logger, "Liberando recursos. Personaje:%s",personaje->nombre);
		liberarRecursos(socketNivel);
		log_debug(logger, "Notificando muerte. Personaje:%s",personaje->nombre);
		notificarMuerte(socketPlanificador);
		close(socketPlanificador);
		close(socketNivel);
		int resultado = procesar();
		if (resultado == 1) {
			return EXIT_FAILURE;
		}
		finalizar();
	} else {
		log_debug(logger, "El personaje %s se quedo sin vidas",personaje->nombre);
		log_debug(logger, "Liberando recursos. Personaje:%s",personaje->nombre);
		liberarRecursos(socketNivel);
		log_debug(logger, "Notificando muerte. Personaje:%s",personaje->nombre);
		notificarMuerte(socketPlanificador);
		close(socketPlanificador);
		close(socketNivel);
		int levantarConfig = levantarPersonaje(path);
		if (levantarConfig == 1) {
			return EXIT_FAILURE;
		}
		int resultado = procesar();
		if (resultado == 1) {
			return EXIT_FAILURE;
		}
		finalizar();
	}
	return EXIT_SUCCESS;
}

int procesar() {
	int socketOrquestador = conectarAlOrquestador();
	if (socketOrquestador == 1) {
		return EXIT_FAILURE;
	}

	int resultado = recorrerNiveles(socketOrquestador);
	if (resultado == 1) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int conectarAlOrquestador() {
	log_debug(logger, "Conectando al orquestador en el puerto:%d. Y la ip:%s",
			personaje->puerto, personaje->ip);
	int socketOrquestador = conectarAlServidor(personaje->ip,
			personaje->puerto);

	if (socketOrquestador < 0) {
		log_error(logger,
				"Error al conectarse con el orquestador en el puerto:%d. Y la ip:%s",
				personaje->puerto, personaje->ip);
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	return socketOrquestador;
}

int recorrerNiveles(int socketOrquestador) {
	log_debug(logger, "Arrancamos a recorrer los niveles del Personaje:%s",
			personaje->nombre);

	while (!queue_is_empty(personaje->listaNiveles)) {

		log_debug(logger, "Pidiendo el proximo nivel para realizar");
		NivelConexion* nivelConexion = pedirNivel(personaje, socketOrquestador);

		log_debug(logger,
				"Conectando al planificador en el puerto:%d. Y la ip:%s",
				nivelConexion->puertoPlanificador,
				nivelConexion->ipPlanificador);
		socketPlanificador = conectarAlServidor(nivelConexion->ipPlanificador,
				nivelConexion->puertoPlanificador);

		if (socketPlanificador < 0) {
			log_error(logger,
					"Error al conectarse con el planificador en el puerto:%d. Y la ip:%s",
					nivelConexion->puertoPlanificador,
					nivelConexion->ipPlanificador);
			log_destroy(logger);
			return EXIT_FAILURE;
		}

		log_debug(logger, "Conectando al Nivel en el puerto:%d. Y la ip:%s",
				nivelConexion->puertoNivel, nivelConexion->ipNivel);
		socketNivel = conectarAlServidor(nivelConexion->ipNivel,
				nivelConexion->puertoNivel);

		if (socketNivel < 0) {
			log_error(logger,
					"Error al conectarse con el Nivel en el puerto:%d. Y la ip:%s",
					nivelConexion->puertoNivel, nivelConexion->ipNivel);
			log_destroy(logger);
			close(socketPlanificador);
			return EXIT_FAILURE;
		}

		recorrerNivel(socketNivel, socketPlanificador);
	}
	return EXIT_SUCCESS;
}

void liberarRecursos(int socketNivel) {

}

void notificarMuerte(int socketPlanificador) {

}

void finalizar() {
	close(socketNivel);
	close(socketPlanificador);
	log_destroy(logger);
	log_debug(logger, "El personaje %s finalizo sus niveles de forma correcta.",personaje->nombre);
}

int avisarFinalizacion(int socketNivel,int socketPlanificador) {
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = FINALIZAR;
	mensajeAEnviar->PayLoadLength = sizeof("Fin") + 1;
	mensajeAEnviar->Payload = "Fin";
	enviarMensaje(socketNivel,mensajeAEnviar);
	enviarMensaje(socketPlanificador,mensajeAEnviar);
	return 0;
}

int levantarPersonaje(char* path) {
	log_debug(logger, "Levantando configuracion en el path:%s", path);
	personaje = levantarConfiguracion(path);
	log_debug(logger, "Configuracion del personaje levantada correctamente.");
	return EXIT_SUCCESS;
}
