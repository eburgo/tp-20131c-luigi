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
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <commons/socket.h>
#include "Orquestador.h"
#include "Planificador.h"

pthread_mutex_t semaforo_niveles = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t semaforo_planificadores = PTHREAD_MUTEX_INITIALIZER;
//Listas compartidas.
t_dictionary *planificadores;
t_dictionary *niveles;
//t_queue *personajesBloqueados;

t_log* loggerOrquestador;



// PROTOTIPOS
void armarNivelConexion(NivelConexion *nivelConexion,Nivel *nivel,Planificador *planificador);

int iniciarOrquestador() {
	int socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	//t_queue *colaHilos = queue_create();
	niveles = dictionary_create();
	planificadores = dictionary_create();

	loggerOrquestador = log_create("/home/utnso/orquestador.log", "ORQUESTADOR", true,
			LOG_LEVEL_TRACE);

	log_debug(loggerOrquestador, "Iniciando servidor...");
	socketEscucha = iniciarServidor(PUERTO);
	log_debug(loggerOrquestador, "Servidor escuchando en el puerto (%d)",PUERTO);

	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		log_debug(loggerOrquestador, "El servidor esta esperando conexiones.");
		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {
			log_error(loggerOrquestador, "Error al aceptar una conexión.");
			return EXIT_FAILURE;
		}
		log_debug(loggerOrquestador, "Conexion aceptada. se crea un hilo para manejar la conexion con el socket (%d)",*socketNuevaConexion);
		pthread_create(thread, NULL, (void*) manejarConexion,
				socketNuevaConexion);
	}

	close(socketEscucha);
	log_info(loggerOrquestador, "Se cerro el socket de escuchar de la plataforma.");
	log_destroy(loggerOrquestador);
	return EXIT_SUCCESS;
}

void manejarConexion(int* socket) {
	MPS_MSG mensaje;
	log_debug(loggerOrquestador, "Socket (%d) - Esperando mensaje inicializador ( Personaje o Nivel )",*socket);
	recibirMensaje(*socket, &mensaje);
	log_info(loggerOrquestador, "Se recibio un mensaje tipo: %d",
			mensaje.PayloadDescriptor);
	switch (mensaje.PayloadDescriptor) {

	case REGISTRAR_NIVEL:
		log_info(loggerOrquestador, "Socket (%d) - Se conecto un nivel",*socket);
		log_debug(loggerOrquestador, "Socket (%d) - Desserializando mensaje.",*socket);
		NivelDatos *nivelDatos = NivelDatos_desserializer(mensaje.Payload);
		log_debug(loggerOrquestador, "Socket (%d) - Se va a registrar el nivel.",*socket);
		registrarNivel(nivelDatos, *socket);
		log_debug(loggerOrquestador, "Socket (%d) - Iniciando planificador del nivel (%s)",*socket,nivelDatos->nombre);
		iniciarUnPlanificador(nivelDatos->nombre);
		log_info(loggerOrquestador, "Socket (%d) - Hilo del nivel (%s) generado con exito",*socket,nivelDatos->nombre);
		break;
	case PJ_PIDE_NIVEL:
		log_info(loggerOrquestador, "Socket (%d) - Se conecto un Personaje",*socket);
		NivelConexion *nivel = malloc(sizeof(NivelConexion));
		char* nombreNivel = (char*) mensaje.Payload;
		log_debug(loggerOrquestador, "Socket (%d) - Se van a preparar los datos del Nivel (%s)",*socket,nombreNivel);
		int resultado=prepararNivelConexion(nombreNivel, nivel);

		if (resultado == -1){
			log_error(loggerOrquestador, "Socket (%d) - Nivel no encontrado",*socket);
			enviarMsjError(socket,"Nivel no encontrado");
			break;
		}
		if (resultado == -2) {
			log_error(loggerOrquestador, "Socket (%d) - Planificador no encontrado",*socket);
			enviarMsjError(socket,"No esta el planificador");
			break;
		}
		log_debug(loggerOrquestador, "Socket (%d) - Serializando mensaje a enviar con el Nivel: (%s)",*socket,nombreNivel);
		t_stream *stream = nivelConexion_serializer(nivel);
		mensaje.PayloadDescriptor=PJ_PIDE_NIVEL;
		mensaje.PayLoadLength=stream->length;
		mensaje.Payload=stream->data;
		log_error(loggerOrquestador, "Socket (%d) - Enviando mensaje con info del nivel",*socket);
		enviarMensaje(*socket, &mensaje);
		log_error(loggerOrquestador, "Socket (%d) - Info enviada con exito",*socket);
		free(nivel);
		break;
	default:
		log_error(loggerOrquestador, "Tipo de mensaje desconocido.");
		enviarMsjError(socket, "Tipo de mensaje desconocido.");
		break;
	}
	close(*socket);
}

void enviarMsjError(int *socket, char* msjError) {
	MPS_MSG respuestaError;
	respuestaError.PayloadDescriptor = ERROR_MENSAJE;
	respuestaError.PayLoadLength = strlen("msjError") + 1;
	respuestaError.Payload = msjError;
	enviarMensaje(*socket, &respuestaError);
}

int registrarNivel(NivelDatos *nivelDatos, int socket) {
	log_info(loggerOrquestador, "Socket (%d) - Registrando nivel (%s)",socket,nivelDatos->nombre);
	Nivel* nivel = malloc(sizeof(Nivel));
	nivel->ip = nivelDatos->ip;
	nivel->nombre = nivelDatos->nombre;
	nivel->puerto = nivelDatos->puerto;
	nivel->socket = socket;
	pthread_mutex_lock( &semaforo_niveles);
	dictionary_put(niveles, nivel->nombre, nivel);
	pthread_mutex_unlock( &semaforo_niveles);
	log_info(loggerOrquestador, "Socket (%d) - Registro completo del nivel (%s,puerto:%d,ip:%s)",socket,nivel->nombre,nivel->puerto,nivel->ip);
	return 0;
}

int prepararNivelConexion(char* nombre, NivelConexion *nivelConexion) {

	Nivel *nivel = malloc(sizeof(Nivel));
	Planificador *planificador = malloc(sizeof(Planificador));
	log_debug(loggerOrquestador, "Se busca el nivel (%s)",nombre);
	pthread_mutex_lock( &semaforo_niveles);
	nivel=(Nivel*)dictionary_get(niveles,nombre);
	pthread_mutex_unlock( &semaforo_niveles);
	if(nivel==NULL)
		return -1;
	log_debug(loggerOrquestador, "Nivel ok.");
	log_debug(loggerOrquestador, "Se busca el planificador (%s)",nombre);
	pthread_mutex_lock( &semaforo_planificadores);
	planificador=(Planificador*)dictionary_get(planificadores,nombre);
	pthread_mutex_unlock( &semaforo_planificadores);
	if (planificador==NULL)
		return -2;
	log_debug(loggerOrquestador, "Se va a armar la struct de NivelConexion con los datos del nivel (%s)",nombre);
	armarNivelConexion(nivelConexion,nivel,planificador);
	return 0;
}
void armarNivelConexion(NivelConexion *nivelConexion,Nivel *nivel,Planificador *planificador){
	nivelConexion->ipNivel=nivel->ip;
	nivelConexion->puertoNivel=nivel->puerto;
	nivelConexion->ipPlanificador=planificador->ip;
	nivelConexion->puertoPlanificador= planificador->puerto;
}

int iniciarUnPlanificador(char* nombreNivel) {
	pthread_t* thread = malloc(sizeof(pthread_t));
	pthread_create(thread, NULL, (void*) iniciarPlanificador, (void*) nombreNivel);
	return 0;
}

