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
void recorrerNivel(int socketNivel, int socketPlanificador, int socketOrquestador);
void enviarSeniales();
void manejarSenial(int sig, siginfo_t *siginfo, void *context);
//Procesamiento del personaje!
int procesar();
// Procesamiento que se realiza cuando se pierde una vida.
int perderVida(bool porOrquestador);
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
int consultarUbicacionCaja(char cajaABuscar, int socketNivel, Posicion* posicion);
// Espera un mensaje departe del planificador para poder realizar un movimiento
int esperarConfirmacionDelPlanificador(int socketPlanificador);
// Realiza la logica del movimiento y le avisa al nivel a donde moverse
void realizarMovimiento(Posicion* posicionActual, Posicion* posicion, int socketNivel);
// Le avisa al planificador que realizo un movimiento
void movimientoRealizado(int socketPlanificador);
// Le avisa al planificador que obtuvo un recurso
void recursoObtenido(int socketPlanificador);
// Pide un recurso al nivel
int pedirRecurso(char recursoAPedir, int socketNivel);
// Notifica al planificador del bloqueo
void avisarDelBloqueo(int socketPlanificador, char* recursoPedido);
// Espera que el planificador lo desbloquee.
void esperarDesbloqueo(int socketOrquestador);
// Avisa al nivel que termino el nivel
void nivelTerminado(int socketNivel);
// Retorna 1 si el personaje esta en la ubicacion de la caja que necesita.
int estaEnPosicionDeLaCaja(Posicion* posicion, Posicion* posicionActual);
// Espera la confirmacion.
void esperarConfirmacion(int socket);
//encapsula toda la logica de pedir recurso
int procesarPedidoDeRecurso(char *cajaABuscar, Nivel *nivel, int socketNivel, t_queue *objetosABuscar, int socketPlanificador, int socketOrquestador);
//copia los objetos de la cola 1 a la cola 2;
void copiarCola(t_queue *cola1, t_queue *cola2);
//le avisa al orquestador que el personaje termino con su plan de niveles
void avisarFinalizacionDelPersonaje();
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
#define OBTUVO_RECURSO 6 // descriptor que envia al planificador si obtuvo un recurso
#define CAJAFUERALIMITE 8 // mensaje q recibe en la consulta de la ubicacion de una caja si la misma esta fuera del limite
#define FINALIZO_NIVELES 20 // Notifica que el personaje termino el plan de niveles
#define MOVIMIENTO_EXITO 12 // MEnsaje del nivel por si el movimiento se dio con exito
#define MUERTE_POR_DEADLOCK 13 // Mensaje por si muere por deadlock
#define MUERTE_CORRECTA 19
#define CONTINUA_NIVEL 30
#define TERMINA_NIVEL 31
int main(int argc, char *argv[]) {
	signal(SIGUSR1, (void*) manejarSenial);
	logger = log_create("/home/utnso/personaje.log", "PERSONAJE", true, LOG_LEVEL_TRACE);
	log_info(logger, "Log creado con exito, se procede a loguear el proceso Personaje");

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

	enviarSeniales();

	int resultado = procesar();
	if (resultado == 1) {
		return EXIT_FAILURE;
	}

	finalizar();
	return EXIT_SUCCESS;
}

void enviarSeniales() {

	struct sigaction act;

	memset(&act, '\0', sizeof(act));

	act.sa_sigaction = &manejarSenial;

	act.sa_flags = SA_NODEFER;

	if (sigaction(SIGTERM, &act, NULL ) < 0) {
		perror("sigaction");
	}

	//if (sigaction(SIGUSR1, &act, NULL ) == -1) {
	//	perror("sigaction");
	//}

}

void manejarSenial(int sig, siginfo_t *siginfo, void *context) {
	switch (sig) {
	case SIGUSR1:
		log_debug(logger, "El personaje (%s) recibio la señal SIGUSR1. Vidas actuales: (%d)", personaje->nombre, personaje->vidas);
		personaje->vidas = personaje->vidas + 1;
		break;
	case SIGTERM:
		log_debug(logger, "El personaje %s recibio la señal SIGTERM.", personaje->nombre);
		int resultado = perderVida(false);
		if (resultado == 1) {
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
		break;
	}
}

void notificarIngresoAlNivel(int socketNivel) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	armarMensaje(mensaje, INGRESA_NIVEL, sizeof(char), personaje->simbolo);
	enviarMensaje(socketNivel, mensaje);
	free(mensaje);
}

int consultarUbicacionCaja(char cajaABuscar, int socketNivel, Posicion* posicion) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	MPS_MSG* mensajeARecibir = malloc(sizeof(MPS_MSG));
	armarMensaje(mensaje, UBICACION_CAJA, sizeof(char), &cajaABuscar);
	enviarMensaje(socketNivel, mensaje);
	recibirMensaje(socketNivel, mensajeARecibir);
	if (mensajeARecibir->PayloadDescriptor == CAJAFUERALIMITE) {
		log_debug(logger, "El personaje %s no puede acceder a la caja (%c) porque esta fuera del limite", personaje->nombre, cajaABuscar);
		free(mensaje);
		free(mensajeARecibir);
		return 0;
	}
	*posicion = *(Posicion*) mensajeARecibir->Payload;
	log_debug(logger, "La caja necesaria esta en X:(%d) Y:(%d)", posicion->x, posicion->y);
	free(mensaje);
	free(mensajeARecibir);
	return 1;
}

int esperarConfirmacionDelPlanificador(int socketPlanificador) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	recibirMensaje(socketPlanificador, mensaje);
	// En el caso de que le confirme el movimiento al personaje
	if (mensaje->PayloadDescriptor == MOVIMIENTO_PERMITIDO) {
		free(mensaje);
		return 1;
	}
	// En el caso de que le confirme la muerte correcta.
	else if (mensaje->PayloadDescriptor == MUERTE_PERSONAJE) {
		free(mensaje);
		log_debug(logger, "El personaje (%s) fue sacado del planificador con exito", personaje->simbolo);
		return 1;
	}
	log_error(logger, "Se recibio un mensaje del planificador inesperado. Descriptor: (%d). Personaje (%s)", mensaje->PayloadDescriptor, personaje->nombre);
	free(mensaje);
	exit(EXIT_FAILURE);
}
void realizarMovimiento(Posicion* posicionActual, Posicion* posicion, int socketNivel) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	MPS_MSG mensajeConfirmacion;
	log_debug(logger, "El personaje esta ubicado en X:(%d) Y:(%d)", posicionActual->x, posicionActual->y);
	log_debug(logger, "El personaje procede a moverse a la caja en X:(%d) Y:(%d)", posicion->x, posicion->y);
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
	Posicion* posicionNueva = malloc(sizeof(Posicion));
	posicionNueva->x = posicionActual->x;
	posicionNueva->y = posicionActual->y;
	armarMensaje(mensaje, AVISO_MOVIMIENTO, sizeof(Posicion), posicionNueva);
	enviarMensaje(socketNivel, mensaje);
	recibirMensaje(socketNivel, &mensajeConfirmacion);
	if (mensajeConfirmacion.PayloadDescriptor == MOVIMIENTO_EXITO) {
		log_debug(logger, "El personaje se movio a X:(%d) Y:(%d)", posicionActual->x, posicionActual->y);
	}
	free(mensaje);
	free(posicionNueva);
}

void movimientoRealizado(int socketPlanificador) {
	log_debug(logger, "El personaje:(%s) procede a avisarle al planificador de su movimiento", personaje->nombre);
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	armarMensaje(mensaje, AVISO_MOVIMIENTO, sizeof(char), "3");
	enviarMensaje(socketPlanificador, mensaje);
	free(mensaje);
}

void recursoObtenido(int socketPlanificador) {
	log_debug(logger, "El personaje:(%s) procede a avisarle al planificador que obtuvo un recurso", personaje->nombre);
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	armarMensaje(mensaje, OBTUVO_RECURSO, sizeof(char), "6");
	enviarMensaje(socketPlanificador, mensaje);
	free(mensaje);
}

int pedirRecurso(char recursoAPedir, int socketNivel) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	armarMensaje(mensaje, PEDIR_RECURSO, sizeof(char), &recursoAPedir);
	enviarMensaje(socketNivel, mensaje);
	recibirMensaje(socketNivel, mensaje);
	if (mensaje->PayloadDescriptor == SIN_RECURSOS) {
		log_debug(logger, "El recurso (%c) no esta disponible.", recursoAPedir);
		free(mensaje);
		return 0;
	}
	free(mensaje);
	return 1;
}
void avisarDelBloqueo(int socketPlanificador, char* recursoPedido) {
	log_debug(logger, "El personaje:(%s) procede a avisar que esta bloqueado.", personaje->nombre);
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = BLOQUEO_PERSONAJE;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = recursoPedido;
	enviarMensaje(socketPlanificador, mensajeAEnviar);
	free(mensajeAEnviar);
}
void nivelTerminado(int socket) {
	log_debug(logger, "El personaje:(%s) procede a avisar el fin del nivel al nivel", personaje->nombre);
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = FINALIZAR;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = "F";
	enviarMensaje(socket, mensajeAEnviar);
	free(mensajeAEnviar);
}

void recorrerNivel(int socketNivel, int socketPlanificador, int socketOrquestador) {
	Nivel *nivel = queue_peek(personaje->listaNiveles);
	Posicion* posicion = malloc(sizeof(Posicion));
	Posicion* posicionActual = malloc(sizeof(Posicion));
	t_queue *objetosABuscar = queue_create();
	copiarCola(nivel->objetos, objetosABuscar);
	posicionActual->x = 0;
	posicionActual->y = 0;
	notificarIngresoAlNivel(socketNivel);
	esperarConfirmacion(socketNivel);
	log_debug(logger, "Confirmacion del nivel");
	notificarIngresoAlNivel(socketPlanificador);
	esperarConfirmacion(socketPlanificador);
	log_debug(logger, "Confirmacion del Planificador");
	int ubicacionCorrecta;
	int terminoPorDesbloqueo = false;
	log_debug(logger, "El personaje:(%s) empieza a recorrer el nivel (%s)", personaje->nombre, nivel->nombre);
	while (!queue_is_empty(objetosABuscar)) {
		char *cajaABuscar;
		ubicacionCorrecta = 0;
		while (!ubicacionCorrecta && !queue_is_empty(objetosABuscar)) {
			cajaABuscar = queue_pop(objetosABuscar);
			log_debug(logger, "Consultando ubicacion de la proxima caja del recurso (%s)", cajaABuscar);
			ubicacionCorrecta = consultarUbicacionCaja(*cajaABuscar, socketNivel, posicion);
		}
		if (estaEnPosicionDeLaCaja(posicion, posicionActual) && ubicacionCorrecta) {
			esperarConfirmacionDelPlanificador(socketPlanificador);
			terminoPorDesbloqueo = procesarPedidoDeRecurso(cajaABuscar, nivel, socketNivel, objetosABuscar, socketPlanificador, socketOrquestador);
		}
		while (!estaEnPosicionDeLaCaja(posicion, posicionActual) && ubicacionCorrecta) {

			log_debug(logger, "El personaje:(%s) esta a la espera de la confirmacion de movimiento", personaje->nombre);

			esperarConfirmacionDelPlanificador(socketPlanificador);

			log_debug(logger, "El personaje:(%s) tiene movimiento permitido, se procede a moverse", personaje->nombre);
			realizarMovimiento(posicionActual, posicion, socketNivel);
			log_debug(logger, "El personaje:(%s) se movio satisfactoriamente", personaje->nombre);
			if (estaEnPosicionDeLaCaja(posicion, posicionActual)) {
				terminoPorDesbloqueo = procesarPedidoDeRecurso(cajaABuscar, nivel, socketNivel, objetosABuscar, socketPlanificador, socketOrquestador);
			} else {
				movimientoRealizado(socketPlanificador);
			}
		}
		if (queue_is_empty(objetosABuscar) && !terminoPorDesbloqueo) {
			log_debug(logger, "El personaje: (%s) procede a informar la finalizacion del nivel al planificador.", personaje->nombre);
			MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
			mensajeAEnviar->PayloadDescriptor = FINALIZAR;
			mensajeAEnviar->PayLoadLength = sizeof(char);
			mensajeAEnviar->Payload = "4";
			enviarMensaje(socketPlanificador, mensajeAEnviar);
			close(socketPlanificador);
		}
	}
	nivelTerminado(socketNivel);
	close(socketNivel);
	queue_pop(personaje->listaNiveles);
	free(posicion);
	free(nivel);
}

void copiarCola(t_queue *cola1, t_queue *cola2) {
	int i;
	char *objeto;
	char *objetocopia;
	int size = queue_size(cola1);
	for (i = 0; i < size; i++) {
		objeto = queue_pop(cola1);
		objetocopia = malloc(sizeof(char));
		strcpy(objetocopia, objeto);
		queue_push(cola2, objetocopia);
		queue_push(cola1, objeto);
	}
}

int estaEnPosicionDeLaCaja(Posicion* posicion, Posicion* posicionActual) {
	return (posicionActual->x == posicion->x && posicionActual->y == posicion->y);
}

void esperarDesbloqueo(int socketOrquestador) {
	MPS_MSG mensajeARecibir;
	int bloqueado = 1;
	log_debug(logger, "El personaje está a la espera de ser desbloqueado");
	while (bloqueado) {
		recibirMensaje(socketOrquestador, &mensajeARecibir);
		if (mensajeARecibir.PayloadDescriptor == DESBLOQUEAR) {
			bloqueado = 0;
		}
		if (mensajeARecibir.PayloadDescriptor == MUERTE_PERSONAJE) {
			int resultado = perderVida(true);
			if (resultado == 1) {
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		}
	}
}

int perderVida(bool porDeadlock) {
	if (sacarVida(personaje) > 0) {
		if (porDeadlock) {
			log_debug(logger, "El personaje %s perdio una vida, ahora le quedan (%d), causa:DEADLOCK", personaje->nombre, personaje->vidas);
		} else {
			log_debug(logger, "El personaje %s perdio una vida, ahora le quedan (%d) causa:SIGTERM", personaje->nombre, personaje->vidas);
		}

		if (!porDeadlock) {
			log_debug(logger, "Notificando muerte al planificador. Personaje:%s", personaje->nombre);
			notificarMuerte(socketPlanificador);
			esperarConfirmacionDelPlanificador(socketPlanificador);
			log_debug(logger, "Notifico la liberacion de recursos. Personaje:%s", personaje->nombre);
			liberarRecursos(socketNivel);
		} else if (porDeadlock) {
			MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
			mensajeAEnviar->PayloadDescriptor = MUERTE_POR_DEADLOCK;
			mensajeAEnviar->PayLoadLength = sizeof(char);
			mensajeAEnviar->Payload = personaje->simbolo;
			log_debug(logger, "Notifico muerte por deadlock al nivel. Personaje:%s", personaje->nombre);
			enviarMensaje(socketNivel, mensajeAEnviar);
			free(mensajeAEnviar);
		}
		MPS_MSG* mensajeARecibir = malloc(sizeof(MPS_MSG));
		recibirMensaje(socketNivel, mensajeARecibir);
		if (mensajeARecibir->PayloadDescriptor != MUERTE_CORRECTA) {
			return EXIT_FAILURE;
		}
		free(mensajeARecibir);
		close(socketPlanificador);
		close(socketNivel);
		int resultado = procesar();
		if (resultado == 1) {
			return EXIT_FAILURE;
		}
		finalizar();
	} else {
		if (porDeadlock) {
			log_debug(logger, "El personaje %s se quedo sin vidas, causa:DEADLOCK", personaje->nombre, personaje->vidas);
		} else {
			log_debug(logger, "El personaje %s se quedo sin vidas, causa:SIGTERM", personaje->nombre, personaje->vidas);
		}
		if (!porDeadlock) {
			log_debug(logger, "Notificando muerte al planificador. Personaje:%s", personaje->nombre);
			notificarMuerte(socketPlanificador);
			esperarConfirmacionDelPlanificador(socketPlanificador);
			log_debug(logger, "Notifico la liberacion de recursos. Personaje:%s", personaje->nombre);
			liberarRecursos(socketNivel);
		} else if (porDeadlock) {
			MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
			mensajeAEnviar->PayloadDescriptor = MUERTE_POR_DEADLOCK;
			mensajeAEnviar->PayLoadLength = sizeof(char);
			mensajeAEnviar->Payload = personaje->simbolo;
			log_debug(logger, "Notifico muerte por deadlock al nivel. Personaje:%s", personaje->nombre);
			enviarMensaje(socketNivel, mensajeAEnviar);
			free(mensajeAEnviar);
		}
		MPS_MSG* mensajeARecibir = malloc(sizeof(MPS_MSG));
		recibirMensaje(socketNivel, mensajeARecibir);
		if (mensajeARecibir->PayloadDescriptor != MUERTE_CORRECTA) {
			return EXIT_FAILURE;
		}
		free(mensajeARecibir);
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
	log_debug(logger, "Conectando al orquestador en el puerto:%d. Y la ip:%s", personaje->puerto, personaje->ip);
	int socketOrquestador = conectarAlServidor(personaje->ip, personaje->puerto);

	if (socketOrquestador < 0) {
		log_error(logger, "Error al conectarse con el orquestador en el puerto:%d. Y la ip:%s", personaje->puerto, personaje->ip);
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	return socketOrquestador;
}

int recorrerNiveles() {

	log_debug(logger, "Arrancamos a recorrer los niveles del Personaje:%s", personaje->nombre);

	while (!queue_is_empty(personaje->listaNiveles)) {
		int socketOrquestador = conectarAlOrquestador();
		if (socketOrquestador == 1) {
			return EXIT_FAILURE;
		}
		log_debug(logger, "Pidiendo el proximo nivel para realizar (%s)", verProximoNivel(personaje)->nombre);
		t_stream* stream = pedirNivel(personaje, socketOrquestador);
		close(socketOrquestador);
		if (stream->length == 0) {
			log_error(logger, "El nivel no se encontro, se procede a terminar el proceso del personaje (%s)", personaje->nombre);
			return EXIT_FAILURE;
		}

		NivelConexion*nivelConexion = nivelConexion_desserializer(stream);

		log_debug(logger, "Conectando al planificador en el puerto:%d. Y la ip:%s", nivelConexion->puertoPlanificador, nivelConexion->ipPlanificador);
		socketPlanificador = conectarAlServidor(nivelConexion->ipPlanificador, nivelConexion->puertoPlanificador);

		if (socketPlanificador < 0) {
			log_error(logger, "Error al conectarse con el planificador en el puerto:%d. Y la ip:%s", nivelConexion->puertoPlanificador, nivelConexion->ipPlanificador);
			log_destroy(logger);
			return EXIT_FAILURE;
		}

		log_debug(logger, "Conectando al Nivel en el puerto:%d. Y la ip:%s", nivelConexion->puertoNivel, nivelConexion->ipNivel);
		socketNivel = conectarAlServidor(nivelConexion->ipNivel, nivelConexion->puertoNivel);

		if (socketNivel < 0) {
			log_error(logger, "Error al conectarse con el Nivel en el puerto:%d. Y la ip:%s", nivelConexion->puertoNivel, nivelConexion->ipNivel);
			log_destroy(logger);
			close(socketPlanificador);
			return EXIT_FAILURE;
		}

		recorrerNivel(socketNivel, socketPlanificador, socketOrquestador);
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
	log_debug(logger, "Recibimos confirmacion (%d)", mensajeARecibir.PayloadDescriptor);
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
	log_debug(logger, "El personaje %s finalizo sus niveles de forma correcta.", personaje->nombre);
	avisarFinalizacionDelPersonaje(); // le avisa al orquestador que termino el plan de niveles
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

int procesarPedidoDeRecurso(char *cajaABuscar, Nivel *nivel, int socketNivel, t_queue *objetosABuscar, int socketPlanificador, int socketOrquestador) {
	int recursoAsignado;
	log_debug(logger, "El personaje: (%s) pedira el recurso (%s) porque llego a la caja correspondiente.", personaje->nombre, cajaABuscar);
	recursoAsignado = pedirRecurso(*cajaABuscar, socketNivel);
	if (!recursoAsignado) {
		log_debug(logger, "El personaje:(%s) se bloqueo a causa de que el recurso (%s) no esta disponible", personaje->nombre, cajaABuscar);
		avisarDelBloqueo(socketPlanificador, cajaABuscar);
		esperarDesbloqueo(socketOrquestador);
		if (!queue_is_empty(objetosABuscar)) {
			log_debug(logger, "El personaje (%s) fue desbloqueado y continua con el nivel.", personaje->nombre);
			MPS_MSG msgAEnviar;
			armarMensaje(&msgAEnviar,CONTINUA_NIVEL,1,"1");
			enviarMensaje(socketPlanificador,&msgAEnviar);
		}else{
			log_debug(logger, "El personaje (%s) fue desbloqueado y termina el nivel.", personaje->nombre);
			MPS_MSG msgAEnviar;
			armarMensaje(&msgAEnviar,TERMINA_NIVEL,1,"1");
			enviarMensaje(socketPlanificador,&msgAEnviar);
			close(socketPlanificador);
			return true;
		}

	} else {
		log_debug(logger, "El personaje: (%s) recibio el recurso(%s) con exito!", personaje->nombre, cajaABuscar);
		if (!queue_is_empty(objetosABuscar)) {
			recursoObtenido(socketPlanificador);
		}
	}

	return false;
}

void avisarFinalizacionDelPersonaje() {
	int socketOrquestador = conectarAlOrquestador();
	if (socketOrquestador == 1) {
		return;
	}
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = FINALIZO_NIVELES;
	mensajeAEnviar->PayLoadLength = sizeof(char);
	mensajeAEnviar->Payload = personaje->simbolo;
	log_debug(logger, "Se le avisa al orquestador de la finalizacion de todos los niveles");
	enviarMensaje(socketOrquestador, mensajeAEnviar);
	free(mensajeAEnviar);
}
