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
void notificarIngresoAlNivel(int socketNivel);
// Consulta la ubicacion de una caja de recursos
void consultarUbicacionCaja(char cajaABuscar, int socketNivel,Posicion* posicion);
// Espera un mensaje departe del planificador para poder realizar un movimiento
int esperarConfirmacionDelPlanificador(int socketPlanificador);
// Realiza la logica del movimiento y le avisa al nivel a donde moverse
void realizarMovimiento(int ubicacionEnNivelX,int ubicacionEnNivelY,Posicion* posicion,int socketNivel);
// Le avisa al planificador que realizo un movimiento
void movimientoRealizado(int socketPlanificador);
// Pide un recurso al nivel
int pedirRecurso(char recursoAPedir, int socketNivel);
// Notifica al planificador del bloqueo
void avisarDelBloqueo(int socketPlanificador);
// Espera que el planificador lo desbloquee.
void esperarDesbloqueo(int socketPlanificador);
// Avisa al nivel que termino el nivel
void nivelTerminado(int socketNivel);
// Retorna 1 si el personaje esta en la ubicacion de la caja que necesita.
int estaEnPosicionDeLaCaja(Posicion* posicion,int ubicacionEnNivelX,int ubicacionEnNivelY);

//Globales
Personaje* personaje;
t_log* logger;
char* path;
int socketPlanificador;
int socketNivel;

//Tipos de mensaje a enviar
#define UBICACION_CAJA 2 // Pide la ubicacion de la caja de recursos que necesita.
#define AVISO_MOVIMIENTO 3 // Le avisa que se va a mover
#define FINALIZAR 4 // Avisod el personaje que no tiene mas recursos que obtener, por ende termina el nivel
#define PEDIR_RECURSO 5

#define MOVIMIENTO_PERMITIDO 1

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
		log_debug(logger, "El personaje %s recibio la señal SIGTERM.",
				personaje->nombre);
		int resultado = perderVida();
		if (resultado == 1) {
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
		break;
	}
}

void notificarIngresoAlNivel(int socketNivel){

}

void consultarUbicacionCaja(char cajaABuscar, int socketNivel,Posicion* posicion){
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	MPS_MSG* mensajeARecibir = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = UBICACION_CAJA;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = &cajaABuscar;
	enviarMensaje(socketNivel,mensaje);
	recibirMensaje(socketNivel,mensajeARecibir);
	posicion = mensajeARecibir->Payload;
}

int esperarConfirmacionDelPlanificador(int socketPlanificador){
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	recibirMensaje(socketPlanificador,mensaje);
	if (mensaje->PayloadDescriptor == MOVIMIENTO_PERMITIDO){
		return 1;
	}
	return 0;
}
void realizarMovimiento(int ubicacionEnNivelX, int ubicacionEnNivelY, Posicion* posicion,int socketNivel){
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	if(ubicacionEnNivelX != posicion->x){
		if(ubicacionEnNivelX < posicion->x){
			ubicacionEnNivelX++;
		} else if(ubicacionEnNivelX > posicion->x){
			ubicacionEnNivelX--;
		}
	} else if(ubicacionEnNivelY != posicion->y){
		if(ubicacionEnNivelY < posicion->y){
			ubicacionEnNivelY++;
		} else if(ubicacionEnNivelY > posicion->y){
			ubicacionEnNivelY--;
		}
	}
	Posicion* posicionNueva = malloc(sizeof(Posicion));
	posicionNueva->x = ubicacionEnNivelX;
	posicionNueva->y = ubicacionEnNivelY;
	mensaje->PayloadDescriptor = AVISO_MOVIMIENTO;
	mensaje->PayLoadLength = sizeof(Posicion);
	mensaje->Payload = posicionNueva;
	enviarMensaje(socketNivel,mensaje);
}

void movimientoRealizado(int socketPlanificador){
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = AVISO_MOVIMIENTO;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = "3";
	enviarMensaje(socketPlanificador,mensaje);
}

int pedirRecurso(char recursoAPedir, int socketNivel){
	return 0;
}
void avisarDelBloqueo(int socketPlanificador){

}
void nivelTerminado(int socketNivel){

}

void recorrerNivel(int socketNivel, int socketPlanificador) {
	Nivel *nivel = queue_peek(personaje->listaNiveles);
	Posicion* posicion = malloc(sizeof(Posicion));
	int ubicacionEnNivelX = 0;
	int ubicacionEnNivelY = 0;
	notificarIngresoAlNivel(socketNivel);
	log_debug(logger, "El personaje:(%s) empieza a recorrer el nivel (%s)",personaje->nombre,nivel->nombre);
	while(!queue_is_empty(nivel->objetos)){
		char *cajaABuscar = queue_pop(nivel->objetos);
		consultarUbicacionCaja(*cajaABuscar,socketNivel,posicion);
		int recursoAsignado;
		while(!estaEnPosicionDeLaCaja(posicion,ubicacionEnNivelX,ubicacionEnNivelY)){
			int movimientoPermitido = 0;
			log_debug(logger, "El personaje:(%s) esta a la espera de la confirmacion de movimiento",personaje->nombre);
			while(movimientoPermitido == 0){
				movimientoPermitido = esperarConfirmacionDelPlanificador(socketPlanificador);
			}
			log_debug(logger, "El personaje:(%s) tiene movimiento permitido, se procede a moverse",personaje->nombre);
			realizarMovimiento(ubicacionEnNivelX,ubicacionEnNivelY,posicion,socketNivel);
			log_debug(logger, "El personaje:(%s) se movio satisfactoriamente",personaje->nombre);
			if(estaEnPosicionDeLaCaja(posicion,ubicacionEnNivelX,ubicacionEnNivelY)){
				log_debug(logger, "El personaje:(%s) pedira el recurso (%s)",personaje->nombre,cajaABuscar);
				recursoAsignado = pedirRecurso(*cajaABuscar,socketNivel);
				if (recursoAsignado == 0) {
					log_debug(logger, "El personaje:(%s) se bloqueo a causa de que el recurso (%s) no esta disponible",personaje->nombre,cajaABuscar);
					avisarDelBloqueo(socketPlanificador);
					esperarDesbloqueo(socketPlanificador);
				} else {
					log_debug(logger, "El personaje:(%s) procede a avisrale al planificador de su movimiento",personaje->nombre);
					movimientoRealizado(socketPlanificador);
				}
			}
		}
	}
	nivelTerminado(socketNivel);
	queue_pop(personaje->listaNiveles);
	free(posicion);
	free(nivel);
}

int estaEnPosicionDeLaCaja(Posicion* posicion,int ubicacionEnNivelX,int ubicacionEnNivelY){
	return (ubicacionEnNivelX == posicion->x && ubicacionEnNivelY == posicion->y);
}

void esperarDesbloqueo(int socketPlanificador){
}

int perderVida() {
	if (sacarVida(personaje) > 0) {
		log_debug(logger, "El personaje %s perdio una vida", personaje->nombre);
		log_debug(logger, "Liberando recursos. Personaje:%s",
				personaje->nombre);
		liberarRecursos(socketNivel);
		log_debug(logger, "Notificando muerte. Personaje:%s",
				personaje->nombre);
		notificarMuerte(socketPlanificador);
		close(socketPlanificador);
		close(socketNivel);
		int resultado = procesar();
		if (resultado == 1) {
			return EXIT_FAILURE;
		}
		finalizar();
	} else {
		log_debug(logger, "El personaje %s se quedo sin vidas",
				personaje->nombre);
		log_debug(logger, "Liberando recursos. Personaje:%s",
				personaje->nombre);
		liberarRecursos(socketNivel);
		log_debug(logger, "Notificando muerte. Personaje:%s",
				personaje->nombre);
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
	log_debug(logger, "El personaje %s finalizo sus niveles de forma correcta.",
			personaje->nombre);
}

int avisarFinalizacion(int socketNivel, int socketPlanificador) {
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = FINALIZAR;
	mensajeAEnviar->PayLoadLength = strlen("Fin") + 1;
	mensajeAEnviar->Payload = "Fin";
	enviarMensaje(socketNivel, mensajeAEnviar);
	enviarMensaje(socketPlanificador, mensajeAEnviar);
	return 0;
}

int levantarPersonaje(char* path) {
	log_debug(logger, "Levantando configuracion en el path:%s", path);
	personaje = levantarConfiguracion(path);
	log_debug(logger, "Configuracion del personaje levantada correctamente.");
	return EXIT_SUCCESS;
}
