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

typedef struct ubicacionP {
	int x;
	int y;
	char* simbolo;
	t_queue* recursosObtenidos;
} Personaje;

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
int inicializarPersonaje(char* simbolo);
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
// Libera los recursos
void liberarRecursos(Personaje* personaje);

//Globales
Nivel* nivel;
t_log* logger;
struct sockaddr_in sAddr;
ITEM_NIVEL *itemsEnNivel = NULL;

pthread_mutex_t semaforo_listaNiveles = PTHREAD_MUTEX_INITIALIZER;

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

// A recibir
#define MUERTE_PERSONAJE 7

int main(int argc, char **argv) {
	int *socketEscucha;
	int socketOrquestador;
	int miPuerto;
	logger = log_create("/home/utnso/nivel.log", "NIVEL", true,
			LOG_LEVEL_TRACE);
	log_info(logger,
			"Log creado con exito, se procede a loguear el proceso Nivel");

	log_debug(logger, "Chequeando el path del nivel...");
	if (argv[1] == NULL ) {
		log_error(logger, "El path del nivel no puede ser vacio.");
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	log_debug(logger, "Levantando configuracion en el path:%s", argv[1]);
	nivel = levantarConfiguracion(argv[1]);
	log_debug(logger, "Configuracion del nivel levantada correctamente.");

	list_iterate(nivel->items, (void *) crearCajasInit);

	log_debug(logger, "Levantando el servidor del Nivel:%s", nivel->nombre);
	socketEscucha = malloc(sizeof(int));
	miPuerto = realizarConexion(socketEscucha);
	log_debug(logger,
			"Servidor del nivel %s se levanto con exito en el puerto:%d",
			nivel->nombre, miPuerto);

	log_debug(logger,
			"Conectando al %s con el Orquestador, IpNivel: %s PuertoNivel: %d",
			nivel->nombre, nivel->ip, miPuerto);
	socketOrquestador = conectarConOrquestador(miPuerto);
	if (socketOrquestador == 1) {
		log_error(logger,
				"Error al conectar al orquestador IpNivel: %s PuertoNivel: %d",
				nivel->ip, miPuerto);
		return EXIT_FAILURE;
	}
	log_debug(logger, "El nivel %s se conecto al Orquestador con exito.",
			nivel->nombre);

	pthread_t hiloOrquestador;
	pthread_t hiloPersonajes;

	pthread_create(&hiloOrquestador, NULL, (void*) detectarInterbloqueos,
			&socketOrquestador);
	pthread_create(&hiloPersonajes, NULL, (void*) comunicarPersonajes,
			socketEscucha);

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
	free(nivelDatos);
	return socketOrquestador;
}

int detectarInterbloqueos(int* socketOrquestador) {
	return 0;
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
		log_info(logger,
				"se crea un hilo para manejar la conexion.Y se los almacena en una cola de hilos");

		pthread_create(thread, NULL, (void*) interactuarConPersonaje,
				socketConPersonaje);
		queue_push(colaHilos, thread);
	}
	// recibir una señal para cortar el ciclo-
	while(!queue_is_empty(colaHilos)){
		thread = queue_pop(colaHilos);
		free(thread);
	}
	return 0;
}
int interactuarConPersonaje(int* socketConPersonaje) {
	int terminoElNivel = 0;
	MPS_MSG mensajeARecibir;
	MPS_MSG mensajeInicializar;
	Personaje* personaje = malloc(sizeof(Personaje));
	personaje->recursosObtenidos = queue_create();
	recibirMensaje(*socketConPersonaje, &mensajeInicializar);
	log_debug(logger, "Se recibio mensaje de inicializacion.");
	personaje->simbolo = mensajeInicializar.Payload;
	inicializarPersonaje(personaje->simbolo);
	log_debug(logger, "Personaje inicializado correctamente. Se procede a avisarle al personaje (%s) que puede recorrer el nivel. ", personaje->simbolo);
	enviarMensaje(*socketConPersonaje, &mensajeInicializar);
	while (terminoElNivel == 0) {
		recibirMensaje(*socketConPersonaje, &mensajeARecibir);

		log_debug(logger, "Se recibio un mensaje tipo: %d",
				mensajeARecibir.PayloadDescriptor);

		switch (mensajeARecibir.PayloadDescriptor) {
		case UBICACION_CAJA:
			administrarPeticionDeCaja(&mensajeARecibir, socketConPersonaje);
			break;
		case AVISO_MOVIMIENTO:
			realizarMovimiento(mensajeARecibir.Payload, personaje);
			break;
		case PEDIR_RECURSO:
			darRecurso(mensajeARecibir.Payload, personaje, socketConPersonaje);
			break;
		case MUERTE_PERSONAJE:
			log_debug(logger, "El personaje ( %s ) murio, se procede a liberar recursos.",personaje->simbolo);
			terminoElNivel = 1;
			break;
		case FINALIZAR:
			log_debug(logger, "El personaje ( %s ) termino el nivel satisfactoriamente, se procede a liberar recursos.",personaje->simbolo);
			terminoElNivel = 1;
			log_debug(logger, "El personaje termino el nivel.");
			break;
		default:
			informarError(socketConPersonaje);
			break;
		}
	}
	liberarRecursos(personaje);
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
	log_debug(logger,"El personaje (%s) pide un recurso (%c)",personaje->simbolo, caja->id);
	if (caja->quantity == 0) {
		mensaje->PayloadDescriptor = SIN_RECURSOS;
		mensaje->PayLoadLength = 2;
		mensaje->Payload = "0";
		enviarMensaje(*socketPersonaje,mensaje);
		free(mensaje);
		log_debug(logger,"No hay recursos (%c) para otorgarle al personaje (%s)", caja->id,personaje->simbolo);
		return 0;
	}
	if (!(caja->posx == personaje->x && caja->posy == personaje->y)) {
		mensaje->PayloadDescriptor = 9;
		mensaje->PayLoadLength = sizeof(char);
		mensaje->Payload = "0";
		enviarMensaje(*socketPersonaje,mensaje);
		free(mensaje);
		return 0;
	}
	pthread_mutex_lock(&semaforo_listaNiveles);
	//restarRecurso(itemsEnNivel,caja->id);
	caja->quantity--;
	log_debug(logger,"Al personaje (%s) se le dio el recurso (%c) del que quedan(%d)",personaje->simbolo, caja->id,caja->quantity);
	pthread_mutex_unlock(&semaforo_listaNiveles);
	char* unRecurso=malloc(sizeof(char));
	*unRecurso=caja->id;
	queue_push(personaje->recursosObtenidos,unRecurso);
	mensaje->PayloadDescriptor = HAY_RECURSOS;
	mensaje->PayLoadLength = sizeof(char);
	mensaje->Payload = "0";
	enviarMensaje(*socketPersonaje,mensaje);
	free(mensaje);
	return 0;
}

int realizarMovimiento(Posicion* posicion, Personaje* personaje) {
	pthread_mutex_lock(&semaforo_listaNiveles);
	MoverPersonaje(itemsEnNivel, *personaje->simbolo, posicion->x, posicion->y);
	pthread_mutex_unlock(&semaforo_listaNiveles);
	personaje->x = posicion->x;
	personaje->y = posicion->y;
	log_debug(logger,"El personaje(%s) se movio a X:(%d) Y:(%d)",personaje->simbolo,personaje->x,personaje->y);
	return 0;
}

int administrarPeticionDeCaja(MPS_MSG* mensajeARecibir, int* socketConPersonaje) {
	log_debug(logger, "Se consulta la ubicacion de una caja.");
	ITEM_NIVEL* caja = buscarCaja(mensajeARecibir->Payload);
	Posicion* posicion = malloc(sizeof(Posicion));
	posicion->x = caja->posx;
	posicion->y = caja->posy;
	MPS_MSG* mensajeAEnviar = malloc(sizeof(MPS_MSG));
	mensajeAEnviar->PayloadDescriptor = UBICACION_CAJA;
	mensajeAEnviar->PayLoadLength = sizeof(Posicion);
	mensajeAEnviar->Payload = posicion;
	enviarMensaje(*socketConPersonaje, mensajeAEnviar);
	log_debug(logger, "La caja consultada esta en X:(%d) Y:(%d)",posicion->x,posicion->y);
	free(posicion);
	free(mensajeAEnviar);
	return EXIT_SUCCESS;
}

ITEM_NIVEL* buscarCaja(char* id) {
	int esElRecurso(ITEM_NIVEL* recursoLista) {
		char* idABuscar = string_substring_until(&(recursoLista->id), 1);
		return string_equals_ignore_case(idABuscar, id);
	}
	log_debug(logger,"Se busca el recurso (%s)",id);
	return list_find(nivel->items, (void*) esElRecurso);
}

int inicializarPersonaje(char* simbolo) {
	pthread_mutex_lock(&semaforo_listaNiveles);
	CrearPersonaje(&itemsEnNivel, *simbolo, 0, 0);
	pthread_mutex_unlock(&semaforo_listaNiveles);
	return EXIT_SUCCESS;
}

void liberarRecursos(Personaje* personaje){
	while(!queue_is_empty(personaje->recursosObtenidos)){
		char* recurso = queue_pop(personaje->recursosObtenidos);
		log_debug(logger, "Se va a liberar una instancia del recurso(%s).",recurso);
		ITEM_NIVEL* caja=buscarCaja(recurso);
		pthread_mutex_lock(&semaforo_listaNiveles);
		//sumarRecurso(itemsEnNivel,*recurso);
		caja->quantity++;
		log_debug(logger, "La caja(%c) ahora tiene(%d) instancias",caja->id,caja->quantity);
		pthread_mutex_unlock(&semaforo_listaNiveles);
	}

}

void crearCajasInit(ITEM_NIVEL* item) {
	CrearCaja(&itemsEnNivel, item->id, item->posx, item->posy, item->quantity);
}
