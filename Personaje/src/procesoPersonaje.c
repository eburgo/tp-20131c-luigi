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
int recorrerNiveles();
// Le avisa al nivel que libere los recursos
void liberarRecursos();
// Le notifica al planificador que se murio el personaje.
void notificarMuerte();
// Finaliza el proceso
void finalizar();
// Levantar la configuracion del personaje, recibe el path.
int levantarPersonaje(char* path);
// Enviar un mensaje al nivel y a su planificador notificando de su finalizacion
void notificarIngresoAlNivel(int socketNivel);
// Consulta la ubicacion de una caja de recursos
void consultarUbicacionCaja(char cajaABuscar, int socketNivel,
		Posicion* posicion);
// Espera un mensaje departe del planificador para poder realizar un movimiento
int esperarConfirmacionDelPlanificador(int socketPlanificador);
// Realiza la logica del movimiento y le avisa al nivel a donde moverse
void realizarMovimiento(Posicion* posicionActual, Posicion* posicion,
		int socketNivel);
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
int estaEnPosicionDeLaCaja(Posicion* posicion, Posicion* posicionActual);
// Espera la confirmacion.
void esperarConfirmacion(int socket);
//encapsula toda la logica de pedir recurso
int procesarPedidoDeRecurso(char *cajaABuscar, Nivel *nivel,int socketNivel, int socketPlanificador,int meMovi);

//Globales
Personaje* personaje;
t_log* logger;
char* path;
int socketPlanificador;
int socketNivel;

//Tipos de mensaje a enviar
#define UBICACION_CAJA 2 // Pide la ubicacion de la caja de recursos que necesita.
#define AVISO_MOVIMIENTO 3 // Le avisa que se va a mover
#define FINALIZAR 4 // Avisa el personaje que no tiene mas recursos que obtener, por ende termina el nivel
#define PEDIR_RECURSO 5
#define LIBERAR_RECURSOS 6 // Le avisa que libere los recursos del personaje
#define MUERTE_PERSONAJE 7 // Notifica la muerte del personaje
#define BLOQUEO_PERSONAJE 8 // Notifica el bloqueo del personaje
#define INGRESA_NIVEL 9 // Avisa que el personaje ingresa al nivel
#define MOVIMIENTO_PERMITIDO 1
#define DESBLOQUEAR 5
#define SIN_RECURSOS 6
#define CON_RECURSOS 7

int main(int argc, char *argv[]) {

	logger = log_create("/home/utnso/personaje.log", "PERSONAJE", true,
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

void notificarIngresoAlNivel(int socketNivel) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = INGRESA_NIVEL;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = personaje->simbolo;
	enviarMensaje(socketNivel, mensaje);
	free(mensaje);
}

void consultarUbicacionCaja(char cajaABuscar, int socketNivel,
		Posicion* posicion) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	MPS_MSG* mensajeARecibir = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = UBICACION_CAJA;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = &cajaABuscar;
	enviarMensaje(socketNivel, mensaje);
	recibirMensaje(socketNivel, mensajeARecibir);
	*posicion = *(Posicion*) mensajeARecibir->Payload;
	free(mensaje);
	free(mensajeARecibir);
}

int esperarConfirmacionDelPlanificador(int socketPlanificador) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	recibirMensaje(socketPlanificador, mensaje);
	if (mensaje->PayloadDescriptor == MOVIMIENTO_PERMITIDO) {
		free(mensaje);
		return 1;
	}
	free(mensaje);
	return 0;
}
void realizarMovimiento(Posicion* posicionActual, Posicion* posicion,
		int socketNivel) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	log_debug(logger, "El personaje esta ubicado en X:(%d) Y:(%d)",
			posicionActual->x, posicionActual->y);
	log_debug(logger,
			"El personaje procede a moverse a la caja en X:(%d) Y:(%d)",
			posicion->x, posicion->y);
	if (posicionActual->x != posicion->x) {
		if (posicionActual->x < posicion->x) {
			posicionActual->x++;
		} else if (posicionActual->x > posicion->x) {
			posicionActual->x--;
		}
	} else if (posicionActual->y != posicion->y) {
		if (posicionActual->y < posicion->y) {
			posicionActual->y++;
		} else if (posicionActual->y > posicion->y) {
			posicionActual->y--;
		}
	}
	log_debug(logger, "El personaje se movio a X:(%d) Y:(%d)",
			posicionActual->x, posicionActual->y);
	Posicion* posicionNueva = malloc(sizeof(Posicion));
	posicionNueva->x = posicionActual->x;
	posicionNueva->y = posicionActual->y;
	mensaje->PayloadDescriptor = AVISO_MOVIMIENTO;
	mensaje->PayLoadLength = sizeof(Posicion);
	mensaje->Payload = posicionNueva;
	enviarMensaje(socketNivel, mensaje);
	free(mensaje);
	free(posicionNueva);
}

void movimientoRealizado(int socketPlanificador) {
	log_debug(logger,
			"El personaje:(%s) procede a avisarle al planificador de su movimiento",
			personaje->nombre);
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = AVISO_MOVIMIENTO;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = "3";
	enviarMensaje(socketPlanificador, mensaje);
	free(mensaje);
}

int pedirRecurso(char recursoAPedir, int socketNivel) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	mensaje->PayloadDescriptor = PEDIR_RECURSO;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = &recursoAPedir;
	enviarMensaje(socketNivel, mensaje);
	recibirMensaje(socketNivel, mensaje);
	if (mensaje->PayloadDescriptor == SIN_RECURSOS) {
		log_debug(logger, "El recurso (%c) no esta disponible.",
				recursoAPedir);
		free(mensaje);
		return 0;
	}
	free(mensaje);
	return 1;
}
void avisarDelBloqueo(int socketPlanificador) {
	log_debug(logger, "El personaje:(%s) procede a avisar que esta bloqueado",
			personaje->nombre);
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = BLOQUEO_PERSONAJE;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = personaje->simbolo;
	enviarMensaje(socketPlanificador, mensajeAEnviar);
	free(mensajeAEnviar);
}
void nivelTerminado(int socket) {
	log_debug(logger, "El personaje:(%s) procede a avisar el fin del nivel",
			personaje->nombre);
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = FINALIZAR;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = "F";
	enviarMensaje(socket, mensajeAEnviar);
	free(mensajeAEnviar);
}

void recorrerNivel(int socketNivel, int socketPlanificador) {
	Nivel *nivel = queue_peek(personaje->listaNiveles);
	Posicion* posicion = malloc(sizeof(Posicion));
	Posicion* posicionActual = malloc(sizeof(Posicion));
	posicionActual->x = 0;
	posicionActual->y = 0;
	notificarIngresoAlNivel(socketNivel);
	esperarConfirmacion(socketNivel);
	notificarIngresoAlNivel(socketPlanificador);
	esperarConfirmacion(socketPlanificador);
	log_debug(logger, "El personaje:(%s) empieza a recorrer el nivel (%s)",
			personaje->nombre, nivel->nombre);
	int meMovi;
	while (!queue_is_empty(nivel->objetos)) {

		char *cajaABuscar = queue_pop(nivel->objetos);
		log_debug(logger,
				"Consultando ubicacion de la proxima caja del recurso (%s)",
				cajaABuscar);
		consultarUbicacionCaja(*cajaABuscar, socketNivel, posicion);
		log_debug(logger, "La caja necesaria esta en X:(%d) Y:(%d)",
				posicion->x, posicion->y);
		if (estaEnPosicionDeLaCaja(posicion, posicionActual)) {
			meMovi = 0;
			procesarPedidoDeRecurso(cajaABuscar, nivel, socketNivel, socketPlanificador,meMovi);
		}
		while (!estaEnPosicionDeLaCaja(posicion, posicionActual)) {

			log_debug(logger,
					"El personaje:(%s) esta a la espera de la confirmacion de movimiento",
					personaje->nombre);

			esperarConfirmacionDelPlanificador(socketPlanificador);

			log_debug(logger,
					"El personaje:(%s) tiene movimiento permitido, se procede a moverse",
					personaje->nombre);
			realizarMovimiento(posicionActual, posicion, socketNivel);
			meMovi = 1;
			log_debug(logger, "El personaje:(%s) se movio satisfactoriamente",
					personaje->nombre);
			if (estaEnPosicionDeLaCaja(posicion, posicionActual)) {
				procesarPedidoDeRecurso(cajaABuscar, nivel,socketNivel,socketPlanificador,meMovi);
			} else {
				movimientoRealizado(socketPlanificador);
			}
		}
	}
	nivelTerminado(socketNivel);
	close(socketNivel);
	queue_pop(personaje->listaNiveles);
	free(posicion);
	free(nivel);
}

int estaEnPosicionDeLaCaja(Posicion* posicion, Posicion* posicionActual) {
	return (posicionActual->x == posicion->x && posicionActual->y == posicion->y);
}

void esperarDesbloqueo(int socketPlanificador) {
	MPS_MSG mensajeARecibir;
	int bloqueado = 1;
	while (bloqueado) {
		recibirMensaje(socketPlanificador, &mensajeARecibir);
		if (mensajeARecibir.PayloadDescriptor == DESBLOQUEAR) {
			bloqueado = 0;
		}
	}
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

	int resultado = recorrerNiveles();
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

int recorrerNiveles() {

	log_debug(logger, "Arrancamos a recorrer los niveles del Personaje:%s",
			personaje->nombre);

	while (!queue_is_empty(personaje->listaNiveles)) {
		int socketOrquestador = conectarAlOrquestador();
		if (socketOrquestador == 1) {
			return EXIT_FAILURE;
		}
		log_debug(logger, "Pidiendo el proximo nivel para realizar");
		t_stream* stream = pedirNivel(personaje, socketOrquestador);
		close(socketOrquestador);
		if (stream->length == 0) {
			log_error(logger,
					"El nivel no se encontro, se procede a terminar el proceso del personaje (%s)",
					personaje->nombre);
			return EXIT_FAILURE;
		}

		NivelConexion*nivelConexion = nivelConexion_desserializer(stream);

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
	log_debug(logger, "Se termino el plan de niveles con exito.");
	return EXIT_SUCCESS;
}

void liberarRecursos() {
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = MUERTE_PERSONAJE;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = personaje->simbolo;
	enviarMensaje(socketNivel, mensajeAEnviar);
	free(mensajeAEnviar);
}

void esperarConfirmacion(int socket) {
	MPS_MSG mensajeARecibir;
	recibirMensaje(socket, &mensajeARecibir);
}

void notificarMuerte() {
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = MUERTE_PERSONAJE;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = personaje->simbolo;
	enviarMensaje(socketPlanificador, mensajeAEnviar);
	free(mensajeAEnviar);
}

void finalizar() {
	close(socketNivel);
	close(socketPlanificador);
	log_debug(logger, "El personaje %s finalizo sus niveles de forma correcta.",
			personaje->nombre);
	log_destroy(logger);

}

int avisarFinalizacion(int socketNivel, int socketPlanificador) {
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = FINALIZAR;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = "F";
	enviarMensaje(socketNivel, mensajeAEnviar);
	enviarMensaje(socketPlanificador, mensajeAEnviar);
	free(mensajeAEnviar);
	return 0;
}

int levantarPersonaje(char* path) {
	log_debug(logger, "Levantando configuracion en el path:%s", path);
	personaje = levantarConfiguracion(path);
	log_debug(logger, "Configuracion del personaje levantada correctamente.");
	return EXIT_SUCCESS;
}

int procesarPedidoDeRecurso(char *cajaABuscar, Nivel *nivel,int socketNivel, int socketPlanificador,int meMovi) {
	int recursoAsignado;
	log_debug(logger,
			"El personaje: (%s) pedira el recurso (%s) porque llego a la caja correspondiente.",
			personaje->nombre, cajaABuscar);
	recursoAsignado = pedirRecurso(*cajaABuscar, socketNivel);
	if (!recursoAsignado) {
		log_debug(logger,
				"El personaje:(%s) se bloqueo a causa de que el recurso (%s) no esta disponible",
				personaje->nombre,cajaABuscar);
		avisarDelBloqueo(socketPlanificador);
		esperarDesbloqueo(socketPlanificador);
		log_debug(logger,
				"El personaje (%s) fue desbloqueado, se continua con el nivel.",
				personaje->nombre);
	} else {
		if (queue_is_empty(nivel->objetos)) {
			nivelTerminado(socketPlanificador);
			close(socketPlanificador);
		} else {
			log_debug(logger,
						"El personaje: (%s) recibio el recurso(%s) con exito!",
						personaje->nombre, cajaABuscar);
			(meMovi==1)?movimientoRealizado(socketPlanificador):"";
		}
	}
	return 0;
}

