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
#include <commons/string.h>
#include "Orquestador.h"
#include "Planificador.h"
#include "inotify.h"

pthread_mutex_t semaforo_niveles = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t semaforo_planificadores = PTHREAD_MUTEX_INITIALIZER;
//Listas compartidas.
t_dictionary *planificadores;
t_dictionary *niveles;
t_log* loggerOrquestador;
t_list* personajes;
int quantumDefault = 2;
int tiempoAccion = 2;
//Tipos de mensajes a enviar
#define DESBLOQUEAR 5
#define RECURSOS_ASIGNADOS 10
#define RECURSOS_NO_ASIGNADOS 11
// PROTOTIPOS
void armarNivelConexion(NivelConexion *nivelConexion, Nivel *nivel, Planificador *planificador);

int iniciarOrquestador() {
	int socketEscucha;
	int *socketNuevaConexion;
	pthread_t *thread;
	pthread_t threadInotify;
	//t_queue *colaHilos = queue_create();
	niveles = dictionary_create();
	planificadores = dictionary_create();
	personajes = list_create();
	loggerOrquestador = log_create("/home/utnso/orquestador.log", "ORQUESTADOR", true, LOG_LEVEL_TRACE);

	log_debug(loggerOrquestador, "Iniciando servidor...");
	socketEscucha = iniciarServidor(PUERTO);
	log_debug(loggerOrquestador, "Servidor escuchando en el puerto (%d)", PUERTO);
	log_debug(loggerOrquestador, "Se levanta la configuracion de los planificadores.");
	levantarConfiguracion("/home/utnso/git/tp-20131c-luigi/Plataforma/Planificador.config", &quantumDefault, &tiempoAccion);
	log_debug(loggerOrquestador, "Quantum seteado a(%d), tiempoAccion seteado a(%d)", quantumDefault, tiempoAccion);
	pthread_create(&threadInotify, NULL, (void*) inotify, NULL );
	while (1) {
		thread = malloc(sizeof(pthread_t));
		socketNuevaConexion = malloc(sizeof(int));
		log_debug(loggerOrquestador, "El servidor esta esperando conexiones.");
		if ((*socketNuevaConexion = accept(socketEscucha, NULL, 0)) < 0) {
			log_error(loggerOrquestador, "Error al aceptar una conexión.");
			return EXIT_FAILURE;
		}
		log_debug(loggerOrquestador, "Conexion aceptada. se crea un hilo para manejar la conexion con el socket (%d)", *socketNuevaConexion);
		pthread_create(thread, NULL, (void*) manejarConexion, socketNuevaConexion);
	}

	close(socketEscucha);
	log_info(loggerOrquestador, "Se cerro el socket de escuchar de la plataforma.");
	log_destroy(loggerOrquestador);
	return EXIT_SUCCESS;
}

void manejarConexion(int* socket) {
	MPS_MSG mensaje;
	int indice;
	log_debug(loggerOrquestador, "Socket (%d) - Esperando mensaje inicializador ( Personaje o Nivel )", *socket);
	recibirMensaje(*socket, &mensaje);
	log_info(loggerOrquestador, "Se recibio un mensaje tipo (%d)", mensaje.PayloadDescriptor);
	switch (mensaje.PayloadDescriptor) {

	case REGISTRAR_NIVEL:
		log_info(loggerOrquestador, "Socket (%d) - Se conecto un nivel", *socket);
		NivelDatos *nivelDatos = NivelDatos_desserializer(mensaje.Payload);
		log_debug(loggerOrquestador, "Socket (%d) - Se va a registrar el (%s)", *socket, nivelDatos->nombre);
		if (registrarNivel(nivelDatos, *socket) == 0) {
			log_debug(loggerOrquestador, "Socket (%d) - Iniciando planificador del (%s)", *socket, nivelDatos->nombre);
			iniciarUnPlanificador(nivelDatos->nombre);
			log_info(loggerOrquestador, "Socket (%d) - Hilo del planificador del (%s) generado con exito", *socket, nivelDatos->nombre);
			log_debug(loggerOrquestador, "Socket (%d) - Niveles (%d), Planificadores (%d)", *socket, dictionary_size(niveles), dictionary_size(planificadores));
			enviarMensaje(*socket, &mensaje);
			esperarMensajesDeNivel(nivelDatos->nombre, *socket);
		} else {
			mensaje.PayloadDescriptor = ERROR_MENSAJE;
			enviarMsjError(socket, "Fallo en el registro");
		}
		break;
	case PJ_PIDE_NIVEL:
		log_info(loggerOrquestador, "Socket (%d) - Se conecto un Personaje", *socket);
		NivelConexion *nivel = malloc(sizeof(NivelConexion));
		char* nombreNivel = (char*) mensaje.Payload;
		log_debug(loggerOrquestador, "Socket (%d) - Se van a preparar los datos del Nivel (%s)", *socket, nombreNivel);
		int resultado = prepararNivelConexion(nombreNivel, nivel);

		if (resultado == -1) {
			log_error(loggerOrquestador, "Socket (%d) - Nivel no encontrado", *socket);
			enviarMsjError(socket, "Nivel no encontrado");
			break;
		}
		if (resultado == -2) {
			log_error(loggerOrquestador, "Socket (%d) - Planificador no encontrado", *socket);
			enviarMsjError(socket, "No esta el planificador");
			break;
		}
		t_stream *stream = nivelConexion_serializer(nivel);
		armarMensaje(&mensaje,PJ_PIDE_NIVEL,stream->length,stream->data);
		log_error(loggerOrquestador, "Socket (%d) - Enviando mensaje con info del nivel solicitado", *socket);
		enviarMensaje(*socket, &mensaje);
		log_error(loggerOrquestador, "Socket (%d) - Info enviada con exito", *socket);
		free(nivel);
		break;
	case FINALIZO_NIVELES:
		log_debug(loggerOrquestador, "El personaje (%s) termino su plan de niveles", mensaje.Payload);
		indice = buscarSimboloPersonaje(personajes, mensaje.Payload);
		list_remove_and_destroy_element(personajes, indice, free);
		if (list_size(personajes) == 0) {
			log_debug(loggerOrquestador, "Todos los personajes terminaron de forma correcta, se llama a koopa.");
			llamarKoopa();
		}
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
	Nivel* nivel;
	nivel = dictionary_get(niveles, nivelDatos->nombre);
	if (nivel != NULL ) {
		log_info(loggerOrquestador, "Socket (%d) - El nivel (%s), ya existe.", socket, nivel->nombre);
		return 1;
	}
	nivel = malloc(sizeof(Nivel));
	nivel->ip = nivelDatos->ip;
	nivel->nombre = nivelDatos->nombre;
	nivel->puerto = nivelDatos->puerto;
	nivel->socket = socket;
	pthread_mutex_lock(&semaforo_niveles);
	dictionary_put(niveles, nivel->nombre, nivel);
	pthread_mutex_unlock(&semaforo_niveles);
	log_info(loggerOrquestador, "Socket (%d) - Registro completo del nivel (%s,puerto:%d,ip:%s)", socket, nivel->nombre, nivel->puerto, nivel->ip);
	return 0;
}

int prepararNivelConexion(char* nombre, NivelConexion *nivelConexion) {

	Nivel *nivel = malloc(sizeof(Nivel));
	Planificador *planificador = malloc(sizeof(Planificador));
	log_debug(loggerOrquestador, "Se busca el nivel (%s)", nombre);
	pthread_mutex_lock(&semaforo_niveles);
	nivel = (Nivel*) dictionary_get(niveles, nombre);
	pthread_mutex_unlock(&semaforo_niveles);
	if (nivel == NULL )
		return -1;
	log_debug(loggerOrquestador, "Se busca el planificador (%s)", nombre);
	pthread_mutex_lock(&semaforo_planificadores);
	planificador = (Planificador*) dictionary_get(planificadores, nombre);
	pthread_mutex_unlock(&semaforo_planificadores);
	if (planificador == NULL )
		return -2;
	log_debug(loggerOrquestador, "Se va a armar la struct de NivelConexion con los datos del nivel (%s)", nombre);
	armarNivelConexion(nivelConexion, nivel, planificador);
	return 0;
}
void armarNivelConexion(NivelConexion *nivelConexion, Nivel *nivel, Planificador *planificador) {
	nivelConexion->ipNivel = nivel->ip;
	nivelConexion->puertoNivel = nivel->puerto;
	nivelConexion->ipPlanificador = planificador->ip;
	nivelConexion->puertoPlanificador = planificador->puerto;
}

int iniciarUnPlanificador(char* nombreNivel) {
	int *socketEscucha;
	socketEscucha = malloc(sizeof(int));
	pthread_t* thread = malloc(sizeof(pthread_t));
	Planificador *planificador = malloc(sizeof(Planificador));
	fd_set* set = malloc(sizeof(fd_set));
	FD_ZERO(set);
	planificador->nombreNivel = nombreNivel;
	planificador->puerto = realizarConexion(socketEscucha);
	planificador->socketEscucha = *socketEscucha;
	planificador->ip = ipDelSocket(*socketEscucha);
	planificador->bloqueados = list_create();
	planificador->personajes = list_create();
	planificador->listos = queue_create();
	planificador->set = set;
	pthread_mutex_lock(&semaforo_planificadores);
	dictionary_put(planificadores, planificador->nombreNivel, planificador);
	pthread_mutex_unlock(&semaforo_planificadores);
	pthread_create(thread, NULL, (void*) iniciarPlanificador, (void*) planificador);
	return 0;
}

void levantarConfiguracion(char* path, int *quantum, int *tiempoAccion) {
	t_config *config = config_create(path);
	*tiempoAccion = atoi(config_get_string_value(config, "tiempoAccion"));
	*quantum = atoi(config_get_string_value(config, "quantum"));
	config_destroy(config);
}

void esperarMensajesDeNivel(char *nombreNivel, int socket) {
	MPS_MSG* mensaje;
	int nivelSigueVivo = 1;
	while (nivelSigueVivo) {
		t_list* recLiberados;
		recLiberados = list_create();
		Planificador *planificadorNivel = malloc(sizeof(Planificador));
		mensaje = malloc(sizeof(MPS_MSG));
		t_stream* streamA = malloc(sizeof(t_stream));
		log_debug(loggerOrquestador, "Socket (%d) - Esperando mensajes del (%s)", socket, nombreNivel);
		recibirMensaje(socket, mensaje);
		log_info(loggerOrquestador, "Se recibio un mensaje tipo (%d) del (%s)", mensaje->PayloadDescriptor, nombreNivel);
		switch (mensaje->PayloadDescriptor) {

		case RECURSOS_LIBERADOS:
			streamA->length = mensaje->PayLoadLength;
			streamA->data = mensaje->Payload;
			int seDesbloqueoPersonaje = 0;
			list_add_all(recLiberados, pjsEnDeadlock_desserializer(streamA));
			log_debug(loggerOrquestador, "El nivel (%s) nos informa de recursos liberados, se procede a chequear si se desbloquea algun personaje", nombreNivel);
			pthread_mutex_lock(&semaforo_niveles);
			planificadorNivel = (Planificador*) dictionary_get(planificadores, nombreNivel);
			pthread_mutex_unlock(&semaforo_niveles);
			t_queue* colaPersonajesLiberados = queue_create();
			t_queue* colaPersonajesADesbloquear = queue_create();
			while (!(list_is_empty(recLiberados) || list_is_empty(planificadorNivel->bloqueados))) {
				Personaje* personajeADesbloquear;
				int posicionPersonaje;
				char* unRecurso = list_remove(recLiberados, 0);
				posicionPersonaje = buscarPersonajeQueEsteBloqueadoPor(planificadorNivel->bloqueados, unRecurso);
				if (posicionPersonaje >= 0) {

					personajeADesbloquear = list_get(planificadorNivel->bloqueados, posicionPersonaje);

					PersonajeDesbloqueado* personajeDesbloqueado = malloc(sizeof(PersonajeDesbloqueado));
					personajeDesbloqueado->simboloPersonaje = personajeADesbloquear->simbolo;
					personajeDesbloqueado->socket = personajeADesbloquear->socket;
					queue_push(colaPersonajesADesbloquear, personajeDesbloqueado);

					PersonajeLiberado* personajeLiberado = malloc(sizeof(PersonajeLiberado));
					personajeLiberado->recursoAAsignar = *unRecurso;
					personajeLiberado->simboloPersonaje = *personajeADesbloquear->simbolo;
					queue_push(colaPersonajesLiberados, personajeLiberado);

					log_debug(loggerOrquestador, "Paso al personaje: (%s) de la listaBloqueados a la lista Listos", personajeADesbloquear->simbolo);
					personajeADesbloquear->quantum = quantumDefault;
					queue_push(planificadorNivel->listos, personajeADesbloquear);
					list_remove(planificadorNivel->bloqueados, posicionPersonaje);
					seDesbloqueoPersonaje = 1;
				}
			}
			if (seDesbloqueoPersonaje == 0) {
				log_debug(loggerOrquestador, "No se encontraron personajes para desbloquear con los recursos liberados por el nivel (%s)", nombreNivel);
				MPS_MSG mensajeNoPersonajesParaLiberar;
				armarMensaje(&mensajeNoPersonajesParaLiberar,SIN_PERSONAJES_PARA_LIBERAR,sizeof(char),"0");
				enviarMensaje(socket, &mensajeNoPersonajesParaLiberar);
			} else if (seDesbloqueoPersonaje == 1) {
				MPS_MSG mensajeLiberacionPersonaje;
				t_stream* stream = colaPersonajesLiberados_serializer(colaPersonajesLiberados);
				armarMensaje(&mensajeLiberacionPersonaje,PERSONAJE_LIBERADO,stream->length,stream->data);
				enviarMensaje(socket, &mensajeLiberacionPersonaje);

				MPS_MSG mensajeDeDesbloqueo;
				armarMensaje(&mensajeDeDesbloqueo,DESBLOQUEAR,sizeof(char),"0");
				PersonajeDesbloqueado* personajeDesbloqueado;

				while (!queue_is_empty(colaPersonajesADesbloquear)) {

					personajeDesbloqueado = queue_pop(colaPersonajesADesbloquear);
					log_debug(loggerOrquestador, "Notifico al personaje: (%s) de su desbloqueo", personajeDesbloqueado->simboloPersonaje);
					enviarMensaje(personajeDesbloqueado->socket, &mensajeDeDesbloqueo);
					sem_post(planificadorNivel->sem);
				}
			}
			break;
		case CHEQUEO_INTERBLOQUEO:
			log_debug(loggerOrquestador, "Hay Deadlock en el nivel (%s), se procede a resolverlo", nombreNivel);
			t_stream* stream = malloc(sizeof(t_stream));
			stream->data = mensaje->Payload;
			stream->length = mensaje->PayLoadLength;
			t_list *pjsEnDeadlock = pjsEnDeadlock_desserializer(stream);
			Personaje *pjAMatar = (Personaje*) buscarPjAMatar(nombreNivel, pjsEnDeadlock);
			armarMensaje(mensaje,CHEQUEO_INTERBLOQUEO,strlen(pjAMatar->simbolo)+1,pjAMatar->simbolo);
			log_debug(loggerOrquestador, "Notificamos al Nivel (%s) que mataremos al personaje (%s)", nombreNivel, pjAMatar->simbolo);
			enviarMensaje(socket, mensaje);
			log_debug(loggerOrquestador, "Se notifico con exito al Nivel (%s) la muerte del personaje (%s)", nombreNivel, pjAMatar->simbolo);
			sacarPersonaje(dictionary_get(planificadores, nombreNivel), pjAMatar, TRUE);
			free(stream);
			break;
		default:
			log_debug(loggerOrquestador, "El nivel (%s) se borrara de la lista de niveles", nombreNivel);
			nivelSigueVivo = 0;
			dictionary_remove(niveles, nombreNivel);
			close(socket);
			break;
		}
		free(mensaje);
	}
}

int buscarPersonajeQueEsteBloqueadoPor(t_list* personajesBloqueados, char* unRecurso) {
	Personaje* personaje;
	int posicion;
	int encontre = 0;
	for (posicion = 0; posicion < list_size(personajesBloqueados) && !encontre; posicion++) {
		personaje = list_get(personajesBloqueados, posicion);
		if (string_equals_ignore_case(personaje->causaBloqueo, unRecurso)) {
			return posicion;
		}
	}
	return -1;
}

void* buscarPjAMatar(char* nombreNivel, t_list *pjsEnDeadlock) {
	Personaje* pjAMatar;
	int encontreAQuienMatar = 0;
	//esta funcion es para iterar la lista de personajes del planificador
	void matarElPrimero(Personaje* pjEnLista) {
		//esta funcion es para iterar la lista de pjsEnDeadlock
		int esElPersonaje(char *pjNombre) {
			return string_equals_ignore_case(pjEnLista->simbolo, pjNombre);
		}

		Personaje *personaje;
		personaje = list_find(pjsEnDeadlock, (void*) esElPersonaje);
		if (!encontreAQuienMatar && (personaje != NULL )) {
			pjAMatar = pjEnLista;
			encontreAQuienMatar = 1;
		}
	}

	Planificador *planificador = dictionary_get(planificadores, nombreNivel);
	list_iterate(planificador->personajes, (void*) matarElPrimero);
	return pjAMatar;
}

char * ipDelSocket(int socket) {
	struct sockaddr_in adr_inet;/* AF_INET */
	int len_inet; /* length */

	//obtenemos la ip que usa el socket
	len_inet = sizeof adr_inet;
	if (getsockname(socket, (struct sockaddr *) &adr_inet, (socklen_t*) &len_inet) == -1) {
		return NULL ; //error
	}

	return inet_ntoa(adr_inet.sin_addr);
}

void llamarKoopa() {
	char *prog[] = { "koopa", "/home/utnso/Escritorio/koopa/koopa.txt", NULL };
	execv("/home/utnso/Escritorio/koopa/koopa", prog);
}

t_stream* colaPersonajesLiberados_serializer(t_queue* colaPersonajesLiberados) {

	char* data = malloc(queue_size(colaPersonajesLiberados) * 2);
	t_stream *stream = malloc(sizeof(t_stream));
	int offset = 0, tmp_size = 0;
	PersonajeLiberado* personaje;
	personaje = queue_pop(colaPersonajesLiberados);
	memcpy(data, personaje, tmp_size = sizeof(PersonajeLiberado));
	offset = tmp_size;
	while (!queue_is_empty(colaPersonajesLiberados)) {
		personaje = queue_pop(colaPersonajesLiberados);
		memcpy(data + offset, personaje, tmp_size = sizeof(PersonajeLiberado));
		offset += tmp_size;
	}
	stream->length = offset;
	stream->data = data;

	return stream;
}
