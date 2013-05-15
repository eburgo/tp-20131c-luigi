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

	logger = log_create("/home/utnso/orquestador.log", "TEST", true,
			LOG_LEVEL_TRACE);
	log_info(logger, "Log creado con exito");

	log_debug(logger, "se va a iniciar el servidor");
	socketEscucha = iniciarServidor(PUERTO);
	log_debug(logger, "servidor escuchando en socket:%d y puerto:%d\n",socketEscucha,PUERTO);


// Aceptar una nueva conexion entrante. Se genera un nuevo socket con la nueva conexion.
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		printf("Escuchando conexiones entrantes.\n");
		log_debug(logger, "servidor escuchando.");
		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {
			log_error(logger, "Error al aceptar una conexión.");
			perror("Error al aceptar conexion entrante");
			return EXIT_FAILURE;
		}
		log_info(logger, "Se acepto una conexion correctamente.");
		log_debug(logger, "se crea un hilo para manejar la conexion.");
		pthread_create(thread, NULL, (void*) manejarConexion,
				socketNuevaConexion);
		log_debug(logger, "hilo iniciado.");
	}

	close(socketEscucha);
	log_info(logger, "Se serro el socket de escuchar de la plataforma.");
	log_destroy(logger);
	return EXIT_SUCCESS;
}

void manejarConexion(int* socket) {
	MPS_MSG mensaje;
	log_debug(logger, "Bloqueado esperando un msj");
	recibirMensaje(*socket, &mensaje);
	log_info(logger, "Se recibio un mensaje tipo: %d",
			mensaje.PayloadDescriptor);
	switch (mensaje.PayloadDescriptor) {

	case REGISTRAR_NIVEL:
		log_info(logger, "Se conecto un nivel");
		log_debug(logger, "Se va a deserializar el msj.");
		NivelDatos *nivelDatos = NivelDatos_desserializer(mensaje.Payload);
		log_debug(logger, "Se va a registrar el nivel.");
		registrarNivel(nivelDatos, *socket);
		log_debug(logger, "se va a iniciar un planificador");
		iniciarUnPlanificador(nivelDatos->nombre);

		log_info(logger, "Se creo un hilo para planificar para ese nivel");

		break;
	case PJ_PIDE_NIVEL:

		log_info(logger, "Se conecto un personaje.");
		NivelConexion *nivel = malloc(sizeof(NivelConexion));
		char* nombreNivel = (char*) mensaje.Payload;
		log_debug(logger, "Se van a preparar los datos del Nivel: %c",nombreNivel);
		int resultado=prepararNivelConexion(nombreNivel, nivel);

		if (resultado == -1){
			log_error(logger, "no esta el nivel");
			enviarMsjError(socket,"no esta el nivel");
			break;
		}
		if (resultado == -2) {
			log_error(logger, "no esta el planificador.");
			enviarMsjError(socket,"no esta el planificador");
			break;
		}
		log_debug(logger, "Se van a serializar los datos del Nivel: %c.",nombreNivel);
		t_stream *stream = nivelConexion_serializer(nivel);
		log_debug(logger, "Asignamos los datos serializados en el msj.");
		mensaje.PayloadDescriptor=PJ_PIDE_NIVEL;
		mensaje.PayLoadLength=stream->length;
		mensaje.Payload=stream->data;
		log_debug(logger, "Se va a enviar el msj.");
		enviarMensaje(*socket, &mensaje);
		log_debug(logger, "mensajeEnviado.");
		break;
	default:
		log_error(logger, "Tipo de mensaje desconocido.");
		enviarMsjError(socket, "Tipo de mensaje desconocido.");
		break;
	}
}

void enviarMsjError(int *socket, char* msjError) {
	MPS_MSG respuestaError;
	log_debug(logger, "Se prepara msj de error.");
	respuestaError.PayloadDescriptor = ERROR_MENSAJE;
	respuestaError.PayLoadLength = strlen("msjError") + 1;
	respuestaError.Payload = "msjError";
	log_debug(logger, "Se va a enviar el msj de error.");
	enviarMensaje(*socket, &respuestaError);
	log_debug(logger, "Msj enviado correctamente.");
}

int registrarNivel(NivelDatos *nivelDatos, int socket) {

	Nivel* nivel = malloc(sizeof(Nivel));
	log_debug(logger, "Se va encolar el nivel.");
	nivel->ip = nivelDatos->ip;
	nivel->nombre = nivelDatos->nombre;
	nivel->puerto = nivelDatos->puerto;
	nivel->socket = socket;
	pthread_mutex_lock( &semaforo_niveles);
	dictionary_put(niveles, nivel->nombre, nivel);
	pthread_mutex_unlock( &semaforo_niveles);
	log_debug(logger, "Nivel encolado.");
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

