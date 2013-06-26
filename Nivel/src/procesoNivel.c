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
#include "configNivel.h"
#include <commons/socket.h>
#include <commons/string.h>
#include <commons/collections/queue.h>

//Funciones
//Funcion que sirve para iterar los items del nivel e ingresarlos adentro de los items del nivel,
//al momento de levantar el nivel.
void crearCajasInit(ITEM_NIVEL* item);
//Esta funcion se encargara de detectar los interbloqueos de los personajes
int detectarInterbloqueos();
//Funcion para conectarse al orquestador y enviarle un mensaje con una estructura que tendra
//su nombre, su ip y su puerto, luego devolvera el socket de la conexion.
int conectarConOrquestador(int miPuerto);
//Funcion principal que se utiliza cuando un personaje se conecta al nivel.
int comunicarPersonajes();
// Inicializara al personaje, lo guardara en la lista de items que estan en el nivel.
Personaje* inicializarPersonaje(char* simbolo);
//Se fija si el recurso esta disponible y le responde por si o por no.
int administrarPeticionDeCaja(MPS_MSG* mensajeARecibir, int* socketConPersonaje);
//Se comunicara con el personaje.
int interactuarConPersonaje(int* socketNuevaConexion);
//En caso de que se ingrese un recurso qe no existe.
int informarError(int* socketConPersonaje);
//Realiza el movimiento del Personaje
int realizarMovimiento(Posicion* posicion, Personaje* personaje);
//busca una caja en la lista de cajas del nivel
ITEM_NIVEL* buscarCaja(char* cajaABuscar);
// da un recurso si el nivel los tiene
int darRecurso(char* recurso, Personaje* personaje, int* socketPersonaje);
// le setea al Personaje el recurso que necesita
void actualizarRecursosNecesitadosAlPersonaje(Personaje *personaje, char *recurso);
// le agrega al personaje el recurso que recibio
void actualizarRecursosRecibidosAlPersonaje(Personaje *personaje, char *recurso);
//Actualiza la caja de recursos segun los recursos que fueron re-asignados
void actualizarRecursos (t_list* recAsignados);

//Globales
Nivel* nivel;
t_log* logger;
struct sockaddr_in sAddr;
ITEM_NIVEL *itemsEnNivel = NULL;
Posicion* limiteMapa;
pthread_mutex_t semaforoListaNiveles = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t semaforoEstadoPersonajes = PTHREAD_MUTEX_INITIALIZER;
int socketOrquestador;

t_list *estadoDePersonajes;

#define IP "127.0.0.1";
//------ TIPOS DE MENSAJES!!------
#define ERROR_MENSAJE 0
#define UBICACION_CAJA 2 // Pide la ubicacion de la caja de recursos que necesita.
#define AVISO_MOVIMIENTO 3 // Le avisa que se va a mover
#define FINALIZAR 4 // Avisod el personaje que no tiene mas recursos que obtener, por ende termina el nivel
#define PEDIR_RECURSO 5
//---- Mensajes a enviar ----
#define REGISTRAR_NIVEL 2
#define SIN_RECURSOS 6
#define HAY_RECURSOS 7
#define CAJAFUERALIMITE 8
#define RECURSOS_LIBERADOS 9
#define RECURSOS_ASIGNADOS 10
#define RECURSOS_NO_ASIGNADOS 11

// A recibir
#define MUERTE_PERSONAJE 7
#define REGISTRO_FAIL 0

int main(int argc, char **argv) {
	int *socketEscucha;
	estadoDePersonajes = list_create();
	int miPuerto;
	logger = log_create("/home/utnso/nivel.log", "NIVEL", false, LOG_LEVEL_TRACE);
	log_info(logger, "Log creado con exito, se procede a loguear el proceso Nivel");

	log_debug(logger, "Chequeando el path del nivel...");
	if (argv[1] == NULL ) {
		log_error(logger, "El path del nivel no puede ser vacio.");
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	log_debug(logger, "Levantando configuracion en el path:%s", argv[1]);
	nivel = levantarConfiguracion(argv[1]);
	log_debug(logger, "Configuracion del nivel levantada correctamente.");

	log_debug(logger, "Iniciando el GUI del Nivel (%s)", nivel->nombre);
	nivel_gui_inicializar();
	limiteMapa = malloc(sizeof(Posicion));
	nivel_gui_get_area_nivel(&limiteMapa->x, &limiteMapa->y);
	list_iterate(nivel->items, (void *) crearCajasInit);
	nivel_gui_dibujar(itemsEnNivel);
	log_debug(logger, "El GUI del Nivel (%s) levantada correctamente, limites X:(%d) Y:(%d)", nivel->nombre, limiteMapa->x, limiteMapa->y);

	log_debug(logger, "Levantando el servidor del Nivel:%s", nivel->nombre);
	socketEscucha = malloc(sizeof(int));
	miPuerto = realizarConexion(socketEscucha);
	log_debug(logger, "Servidor del nivel %s se levanto con exito en el puerto:%d", nivel->nombre, miPuerto);

	log_destroy(logger);
	char nombreOrigen[16] = "NIVEL - ";
	char* nombreLog = strcat(nombreOrigen, nivel->nombre);
	logger = log_create("/home/utnso/nivel.log", nombreLog, false, LOG_LEVEL_TRACE);

	log_debug(logger, "Conectando al %s con el Orquestador, IpNivel: %s PuertoNivel: %d", nivel->nombre, nivel->ip, miPuerto);
	socketOrquestador = conectarConOrquestador(miPuerto);
	if (socketOrquestador == 1) {
		log_error(logger, "Error al conectar al orquestador IpNivel: %s PuertoNivel: %d", nivel->ip, miPuerto);
		return EXIT_FAILURE;
	}
	log_debug(logger, "El nivel %s se conecto al Orquestador con exito.", nivel->nombre);

	pthread_t hiloOrquestador;
	pthread_t hiloPersonajes;

	pthread_create(&hiloOrquestador, NULL, (void*) detectarInterbloqueos, &socketOrquestador);
	pthread_create(&hiloPersonajes, NULL, (void*) comunicarPersonajes, socketEscucha);

	pthread_join(hiloOrquestador, NULL );
	pthread_join(hiloPersonajes, NULL );

	free(socketEscucha);
	close(socketOrquestador);
	close(*socketEscucha);
	log_destroy(logger);
	return EXIT_SUCCESS;
}

int conectarConOrquestador(int miPuerto) {
	MPS_MSG mensajeAEnviar;
	MPS_MSG mensajeARecibir;
	int socketOrquestador;
	NivelDatos* nivelDatos = malloc(sizeof(NivelDatos));
	nivelDatos->ip = nivel->ip;
	nivelDatos->nombre = nivel->nombre;
	nivelDatos->puerto = miPuerto;
	t_stream* stream = NivelDatos_serializer(nivelDatos);
	mensajeAEnviar.PayloadDescriptor = REGISTRAR_NIVEL;
	mensajeAEnviar.PayLoadLength = stream->length;
	mensajeAEnviar.Payload = stream->data;

	socketOrquestador = conectarAlServidor(nivel->ip, nivel->puerto);
	if (socketOrquestador == -1) {
		return EXIT_FAILURE;
	}
	enviarMensaje(socketOrquestador, &mensajeAEnviar);
	recibirMensaje(socketOrquestador, &mensajeARecibir);
	if (mensajeARecibir.PayloadDescriptor == REGISTRO_FAIL) {
		return EXIT_FAILURE;
	}
	free(nivelDatos);
	return socketOrquestador;
}

int comunicarPersonajes(int *socketEscucha) {
	int *socketConPersonaje;
	pthread_t *thread;
	t_queue *colaHilos = queue_create();

	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketConPersonaje = malloc(sizeof(int));
		log_info(logger, "Esperando conexiones de personajes.");
		if ((*socketConPersonaje = accept(*socketEscucha, NULL, 0)) < 0) {
			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;

		}
		log_info(logger, "Se acepto una conexion correctamente.");
		log_info(logger, "se crea un hilo para manejar la conexion.Y se los almacena en una cola de hilos");

		pthread_create(thread, NULL, (void*) interactuarConPersonaje, socketConPersonaje);
		queue_push(colaHilos, thread);
	}
	// recibir una señal para cortar el ciclo-
	while (!queue_is_empty(colaHilos)) {
		thread = queue_pop(colaHilos);
		free(thread);
	}
	return 0;
}
int interactuarConPersonaje(int* socketConPersonaje) {
	int terminoElNivel = 0;
	MPS_MSG *mensajeARecibir;
	MPS_MSG mensajeInicializar;

	recibirMensaje(*socketConPersonaje, &mensajeInicializar);
	log_debug(logger, "Se recibio mensaje de inicializacion.");

	Personaje*personaje = inicializarPersonaje(mensajeInicializar.Payload);
	log_debug(logger, "Personaje inicializado correctamente. Se procede a avisarle al personaje (%s) que puede recorrer el nivel. ", personaje->simbolo);
	enviarMensaje(*socketConPersonaje, &mensajeInicializar);
	while (terminoElNivel == 0) {
		mensajeARecibir=malloc(sizeof(MPS_MSG));
		nivel_gui_dibujar(itemsEnNivel);
		recibirMensaje(*socketConPersonaje, mensajeARecibir);

		log_debug(logger, "Se recibio un mensaje tipo: %d", mensajeARecibir->PayloadDescriptor);

		switch (mensajeARecibir->PayloadDescriptor) {
		case UBICACION_CAJA:
			administrarPeticionDeCaja(mensajeARecibir, socketConPersonaje);
			break;
		case AVISO_MOVIMIENTO:
			realizarMovimiento(mensajeARecibir->Payload, personaje);
			break;
		case PEDIR_RECURSO:
			darRecurso(mensajeARecibir->Payload, personaje, socketConPersonaje);
			break;
		case MUERTE_PERSONAJE:
			log_debug(logger, "El personaje ( %s ) murio, se procede a liberar recursos.", personaje->simbolo);
			liberarRecursos(personaje, socketOrquestador);
			terminoElNivel = 1;
			break;
		case FINALIZAR:
			log_debug(logger, "El personaje (%s) termino el nivel satisfactoriamente, se procede a liberar recursos.", personaje->simbolo);
			liberarRecursos(personaje, socketOrquestador);
			terminoElNivel = 1;
			break;
		default:
			log_debug(logger, "El personaje (%s) envio un mensaje no esperado, se cierra la conexion.", personaje->simbolo);
			terminoElNivel = 1;
			break;
		}
		free(mensajeARecibir);
	}
	liberarRecursos(personaje, socketOrquestador);
	close(*socketConPersonaje);
	free(personaje);
	return 0;
}

int informarError(int* socketConPersonaje) {
	MPS_MSG respuestaError;
	respuestaError.PayloadDescriptor = ERROR_MENSAJE;
	respuestaError.PayLoadLength = strlen("payloadDescriptor incorrecto.") + 1;
	respuestaError.Payload = "payloadDescriptor incorrecto.";
	enviarMensaje(*socketConPersonaje, &respuestaError);
	return 0;
}

int darRecurso(char* recurso, Personaje* personaje, int* socketPersonaje) {
	MPS_MSG* mensaje = malloc(sizeof(MPS_MSG));
	ITEM_NIVEL* caja = buscarCaja(recurso);
	log_debug(logger, "El personaje (%s) pide un recurso (%c)", personaje->simbolo, caja->id);
	if (caja->quantity == 0) {
		mensaje->PayloadDescriptor = SIN_RECURSOS;
		mensaje->PayLoadLength = 2;
		mensaje->Payload = "0";
		enviarMensaje(*socketPersonaje, mensaje);
		free(mensaje);
		log_debug(logger, "No hay recursos (%c) para otorgarle al personaje (%s)", caja->id, personaje->simbolo);
		pthread_mutex_lock(&semaforoEstadoPersonajes);
		actualizarRecursosNecesitadosAlPersonaje(personaje, recurso);
		pthread_mutex_unlock(&semaforoEstadoPersonajes);
		return 0;
	}
	if (!(caja->posx == personaje->x && caja->posy == personaje->y)) {
		log_debug(logger, "La caja del recurso (%c) no esta en la posicion de (%s)", caja->id, personaje->simbolo);
		return 0;
	}

	pthread_mutex_lock(&semaforoListaNiveles);
	caja->quantity--;
	restarRecurso(itemsEnNivel, caja->id);
	log_debug(logger, "Al personaje (%s) se le dio el recurso (%c) del que quedan(%d)", personaje->simbolo, caja->id, caja->quantity);
	pthread_mutex_unlock(&semaforoListaNiveles);
	mensaje->PayloadDescriptor = HAY_RECURSOS;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = "0";
	pthread_mutex_lock(&semaforoEstadoPersonajes);
	queue_push(personaje->recursosObtenidos, string_substring_until(&caja->id, 1));
	actualizarRecursosRecibidosAlPersonaje(personaje, recurso);
	pthread_mutex_unlock(&semaforoEstadoPersonajes);
	enviarMensaje(*socketPersonaje, mensaje);
	free(mensaje);
	return 0;
}

int realizarMovimiento(Posicion* posicion, Personaje* personaje) {
	pthread_mutex_lock(&semaforoListaNiveles);
	MoverPersonaje(itemsEnNivel, *personaje->simbolo, posicion->x, posicion->y);
	pthread_mutex_unlock(&semaforoListaNiveles);
	pthread_mutex_lock(&semaforoEstadoPersonajes);
	personaje->x = posicion->x;
	personaje->y = posicion->y;
	pthread_mutex_unlock(&semaforoEstadoPersonajes);
	log_debug(logger, "El personaje(%s) se movio a X:(%d) Y:(%d)", personaje->simbolo, personaje->x, personaje->y);
	return 0;
}

int administrarPeticionDeCaja(MPS_MSG* mensajeARecibir, int* socketConPersonaje) {
	log_debug(logger, "Se consulta la ubicacion de una caja.(%s)", mensajeARecibir->Payload);
	ITEM_NIVEL* caja = buscarCaja(mensajeARecibir->Payload);
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	if (limiteMapa->x < caja->posx || limiteMapa->y < caja->posy) {
		mensajeAEnviar->PayloadDescriptor = CAJAFUERALIMITE;
		mensajeAEnviar->PayLoadLength = sizeof(char);
		mensajeAEnviar->Payload = "8";
		enviarMensaje(*socketConPersonaje, mensajeAEnviar);
		free(mensajeAEnviar);
		log_debug(logger, "La caja (%c) esta fuera del limite en X:(%d) Y:(%d)", caja->id, caja->posx, caja->posy);
		return EXIT_SUCCESS;
	}
	Posicion* posicion = malloc(sizeof(Posicion));
	posicion->x = caja->posx;
	posicion->y = caja->posy;
	mensajeAEnviar->PayloadDescriptor = UBICACION_CAJA;
	mensajeAEnviar->PayLoadLength = sizeof(Posicion);
	mensajeAEnviar->Payload = posicion;
	enviarMensaje(*socketConPersonaje, mensajeAEnviar);
	log_debug(logger, "La caja (%c) esta en X:(%d) Y:(%d)", caja->id, posicion->x, posicion->y);
	free(posicion);
	free(mensajeAEnviar);
	return EXIT_SUCCESS;
}

Personaje* inicializarPersonaje(char* simbolo) {
	pthread_mutex_lock(&semaforoListaNiveles);
	CrearPersonaje(&itemsEnNivel, *simbolo, 0, 0);
	Personaje* pj = malloc(sizeof(Personaje));
	pj->simbolo = simbolo;
	pj->itemsAsignados = list_create();
	pj->recursosObtenidos = queue_create();
	pj->itemNecesitado = NULL;
	pj->x = 0;
	pj->y = 0;
	list_add(estadoDePersonajes, pj);
	pthread_mutex_unlock(&semaforoListaNiveles);
	return pj;
}

void liberarRecursos(Personaje* personaje, int socketOrquestador) {
	MPS_MSG mensajeARecibir;
	t_list* recursosAasignar;
	recursosAasignar = list_create();
	list_add_all(recursosAasignar, personaje->recursosObtenidos->elements);
	t_stream* stream = NivelRecursosLiberados_serializer(recursosAasignar);
	MPS_MSG mensajeRecursosLiberados;
	mensajeRecursosLiberados.PayloadDescriptor = RECURSOS_LIBERADOS;
	mensajeRecursosLiberados.PayLoadLength = stream->length;
	mensajeRecursosLiberados.Payload = stream->data;
	log_debug(logger, "Se enviaran los recursos obtenidos al orquestador:(%d) para que se asignen a otros personajes ", socketOrquestador);
	enviarMensaje(socketOrquestador, &mensajeRecursosLiberados);
	recibirMensaje(socketOrquestador, &mensajeARecibir);
	if( mensajeARecibir.PayloadDescriptor == RECURSOS_NO_ASIGNADOS ){
		log_debug(logger, "El nivel recibio la lista de recursos No asignados para su actulizar los recursos");
		t_stream* streamA=malloc(sizeof(t_stream));
		t_list* recNoAsignados;
		recNoAsignados = list_create();
			streamA->length = mensajeARecibir.PayLoadLength;
			streamA->data = mensajeARecibir.Payload;
			list_add_all(recNoAsignados, pjsEnDeadlock_desserializer(streamA));
			log_debug(logger,"Se deserializo con exito una lista de tamaño:(%d)", list_size(recNoAsignados));
			actualizarRecursos (recNoAsignados);
	} else {
		log_debug(logger,"Se recibio un mensaje equivocado");
	}
	log_debug(logger, "El personaje(%s) fue eliminado del nivel.", personaje->simbolo);
	BorrarItem(&itemsEnNivel, *personaje->simbolo);
}

void actualizarRecursos (t_list* recAsignados){
	while (!list_is_empty(recAsignados)) {
		char* recurso = list_remove(recAsignados,0);
		log_debug(logger, "Se va a liberar una instancia del recurso(%s).", recurso);
		ITEM_NIVEL* caja = buscarCaja(recurso);
		pthread_mutex_lock(&semaforoListaNiveles);
		caja->quantity++;
		sumarRecurso(itemsEnNivel, caja->id);
		nivel_gui_dibujar(itemsEnNivel);
		log_debug(logger, "La caja(%c) ahora tiene(%d) instancias", caja->id, caja->quantity);
		free(recurso);
		pthread_mutex_unlock(&semaforoListaNiveles);
		}
	}


void crearCajasInit(ITEM_NIVEL* item) {
	CrearCaja(&itemsEnNivel, item->id, item->posx, item->posy, item->quantity);
}

void actualizarRecursosNecesitadosAlPersonaje(Personaje *personaje, char *recurso) {
	bool esElPersonaje(Personaje *pj) {
		return string_equals_ignore_case(pj->simbolo, personaje->simbolo);
	}
	Personaje *pj = list_find(estadoDePersonajes, (void*) esElPersonaje);
	pj->itemNecesitado = recurso;
}
void actualizarRecursosRecibidosAlPersonaje(Personaje *personaje, char *recurso) {
	bool esElPersonaje(Personaje *pj) {
		return string_equals_ignore_case(pj->simbolo, personaje->simbolo);
	}
	bool esElRecurso(RecursoAsignado *rec) {
		return string_equals_ignore_case(rec->nombre, recurso);
	}
	log_debug(logger, "el pj recibio un recurso, actualizamos su estado");

	Personaje *pj = list_find(estadoDePersonajes, (void*) esElPersonaje);
	log_debug(logger, "encontramos el pj en la lista de estados");
	RecursoAsignado *rec = list_find(pj->itemsAsignados, (void*) esElRecurso);
	if (rec == NULL ) {
		log_debug(logger, "No encontramos el recurso en la lista de items asignados");
		rec = malloc(sizeof(RecursoAsignado));
		rec->nombre = recurso;
		rec->cantidad = 1;
		list_add(pj->itemsAsignados, rec);
	} else {
		log_debug(logger, "encontramos el recurso en la lista de items asignados");
		rec->cantidad++;
	}
}

