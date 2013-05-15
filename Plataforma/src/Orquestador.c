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

t_log* logger;



// PROTOTIPOS
void armarNivelConexion(NivelConexion *nivelConexion,Nivel *nivel,Planificador *planificador);

int iniciarOrquestador() {
	int socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	//t_queue *colaHilos = queue_create();
	niveles = dictionary_create();
	planificadores = dictionary_create();

	logger = log_create("/home/utnso/orquestador.log", "ORQUESTADOR", true,
			LOG_LEVEL_TRACE);

	log_debug(logger, "Iniciando servidor...");
	socketEscucha = iniciarServidor(PUERTO);
	log_debug(logger, "Servidor escuchando en el puerto (%d)",PUERTO);

	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		log_debug(logger, "El servidor esta esperando conexiones.");
		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {
			log_error(logger, "Error al aceptar una conexión.");
			return EXIT_FAILURE;
		}
		log_debug(logger, "Conexion aceptada. se crea un hilo para manejar la conexion con el socket (%d)",*socketNuevaConexion);
		pthread_create(thread, NULL, (void*) manejarConexion,
				socketNuevaConexion);
	}

	close(socketEscucha);
	log_info(logger, "Se cerro el socket de escuchar de la plataforma.");
	log_destroy(logger);
	return EXIT_SUCCESS;
}

void manejarConexion(int* socket) {
	MPS_MSG mensaje;
	log_debug(logger, "Socket (%d) - Esperando mensaje inicializador ( Personaje o Nivel )",*socket);
	recibirMensaje(*socket, &mensaje);
	log_info(logger, "Se recibio un mensaje tipo: %d",
			mensaje.PayloadDescriptor);
	switch (mensaje.PayloadDescriptor) {

	case REGISTRAR_NIVEL:
		log_info(logger, "Socket (%d) - Se conecto un nivel",*socket);
		log_debug(logger, "Socket (%d) - Desserializando mensaje.",*socket);
		NivelDatos *nivelDatos = NivelDatos_desserializer(mensaje.Payload);
		log_debug(logger, "Socket (%d) - Se va a registrar el nivel.",*socket);
		registrarNivel(nivelDatos, *socket);
		log_debug(logger, "Socket (%d) - Iniciando planificador del nivel (%s)",*socket,nivelDatos->nombre);
		iniciarUnPlanificador(nivelDatos->nombre);
		log_info(logger, "Socket (%d) - Hilo del nivel (%s) generado con exito",*socket,nivelDatos->nombre);
		break;
	case PJ_PIDE_NIVEL:
		log_info(logger, "Socket (%d) - Se conecto un Personaje",*socket);
		NivelConexion *nivel = malloc(sizeof(NivelConexion));
		char* nombreNivel = (char*) mensaje.Payload;
		log_debug(logger, "Socket (%d) - Se van a preparar los datos del Nivel (%s)",*socket,nombreNivel);
		int resultado=prepararNivelConexion(nombreNivel, nivel);

		if (resultado == -1){
			log_error(logger, "Socket (%d) - Nivel no encontrado",*socket);
			enviarMsjError(socket,"Nivel no encontrado");
			break;
		}
		if (resultado == -2) {
			log_error(logger, "Socket (%d) - Planificador no encontrado",*socket);
			enviarMsjError(socket,"No esta el planificador");
			break;
		}
		log_debug(logger, "Socket (%d) - Serializando mensaje a enviar con el Nivel: (%s)",*socket,nombreNivel);
		t_stream *stream = nivelConexion_serializer(nivel);
		mensaje.PayloadDescriptor=PJ_PIDE_NIVEL;
		mensaje.PayLoadLength=stream->length;
		mensaje.Payload=stream->data;
		log_error(logger, "Socket (%d) - Enviando mensaje con info del nivel",*socket);
		enviarMensaje(*socket, &mensaje);
		log_error(logger, "Socket (%d) - Info enviada con exito",*socket);
		free(nivel);
		break;
	default:
		log_error(logger, "Tipo de mensaje desconocido.");
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
	log_info(logger, "Socket (%d) - Registrando nivel...",socket);
	Nivel* nivel = malloc(sizeof(Nivel));
	nivel->ip = nivelDatos->ip;
	nivel->nombre = nivelDatos->nombre;
	nivel->puerto = nivelDatos->puerto;
	nivel->socket = socket;
	pthread_mutex_lock( &semaforo_niveles);
	dictionary_put(niveles, nivel->nombre, nivel);
	pthread_mutex_unlock( &semaforo_niveles);
	log_info(logger, "Socket (%d) - Registro completo!",socket);
	return 0;
}

int prepararNivelConexion(char* nombre, NivelConexion *nivelConexion) {
	Nivel *nivel = malloc(sizeof(Nivel));
	Planificador *planificador = malloc(sizeof(Planificador));
	log_debug(logger, "Se busca el nivel.");
	pthread_mutex_lock( &semaforo_niveles);
	nivel=(Nivel*)dictionary_get(niveles,nombre);
	pthread_mutex_unlock( &semaforo_niveles);


	if(nivel==NULL)
		return -1;
	log_debug(logger, "Nivel ok.");
	log_debug(logger, "Se busca el planificador.");
	pthread_mutex_lock( &semaforo_planificadores);
	planificador=(Planificador*)dictionary_get(planificadores,nombre);
	pthread_mutex_lock( &semaforo_planificadores);
	if (planificador==NULL)
		return -2;
	log_debug(logger, "Planificador ok.");
	log_debug(logger, "Se va a armar la struct de NivelConexion con los datos.");
	armarNivelConexion(nivelConexion,nivel,planificador);
	log_debug(logger, "Struct ok.");
	return 0;


return 0;
}
void armarNivelConexion(NivelConexion *nivelConexion,Nivel *nivel,Planificador *planificador){
	nivelConexion->ipNivel=nivel->ip;
	nivelConexion->puertoNivel=nivel->puerto;
	nivelConexion->ipPlanificador=planificador->ip;
	nivelConexion->puertoPlanificador= planificador->puerto;
}

int iniciarUnPlanificador(char* nombreNivel) {
	pthread_t thread;
	pthread_create(&thread, NULL, (void*) iniciarPlanificador, (void*) nombreNivel);
	return 0;
}

