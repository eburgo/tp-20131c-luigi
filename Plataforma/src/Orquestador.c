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
		if(registrarNivel(nivelDatos, *socket) == 0){
			log_debug(loggerOrquestador, "Socket (%d) - Iniciando planificador del (%s)", *socket, nivelDatos->nombre);
			iniciarUnPlanificador(nivelDatos->nombre);
			log_info(loggerOrquestador, "Socket (%d) - Hilo del planificador del (%s) generado con exito", *socket, nivelDatos->nombre);
			log_debug(loggerOrquestador, "Socket (%d) - Niveles (%d), Planificadores (%d)", *socket, dictionary_size(niveles), dictionary_size(planificadores));
			enviarMensaje(*socket,&mensaje);
			esperarMensajesDeNivel(nivelDatos->nombre, *socket);
		}
		else {
			mensaje.PayloadDescriptor=ERROR_MENSAJE;
			enviarMsjError(socket,"fallo registro");
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
		log_debug(loggerOrquestador, "Socket (%d) - Serializando mensaje a enviar con el Nivel: (%s)", *socket, nombreNivel);
		t_stream *stream = nivelConexion_serializer(nivel);
		mensaje.PayloadDescriptor = PJ_PIDE_NIVEL;
		mensaje.PayLoadLength = stream->length;
		mensaje.Payload = stream->data;
		log_error(loggerOrquestador, "Socket (%d) - Enviando mensaje con info del nivel", *socket);
		enviarMensaje(*socket, &mensaje);
		log_error(loggerOrquestador, "Socket (%d) - Info enviada con exito", *socket);
		free(nivel);
		break;
	case FINALIZO_NIVELES:
		indice = buscarSimboloPersonaje(personajes, mensaje.Payload);
		list_remove_and_destroy_element(personajes, indice, free);
		if (list_size(personajes) == 0) {
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
	nivel=dictionary_get(niveles,nivelDatos->nombre);
	if(nivel!=NULL){
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
	log_debug(loggerOrquestador, "Nivel ok.");
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
	int nivelSigueVivo=1;
	while (nivelSigueVivo) {
		t_list* recLiberados;
		t_list* recAsignados;
		recLiberados = list_create();
		recAsignados = list_create();
		Planificador *planificadorNivel = malloc(sizeof(Planificador));
		MPS_MSG* mensajeRecursosAsignados;
		mensajeRecursosAsignados=malloc(sizeof(MPS_MSG));
		mensaje = malloc(sizeof(MPS_MSG));
		t_stream* streamA=malloc(sizeof(t_stream));
		log_debug(loggerOrquestador, "Socket (%d) - Esperando mensajes del (%s)", socket, nombreNivel);
		recibirMensaje(socket, mensaje);
		log_info(loggerOrquestador, "Se recibio un mensaje tipo (%d) del (%s)", mensaje->PayloadDescriptor, nombreNivel);
		switch (mensaje->PayloadDescriptor) {
		case RECURSOS_LIBERADOS:

			streamA->length=mensaje->PayLoadLength;
			streamA->data=mensaje->Payload;
			list_add_all(recLiberados, pjsEnDeadlock_desserializer(streamA));
			log_debug(loggerOrquestador,"Se deserializo con exito una lista de tamaño:(%d)", list_size(recLiberados));
			log_debug(loggerOrquestador, "Se busca el planificador del nivel (%s)", nombreNivel);
			planificadorNivel = (Planificador*) dictionary_get(planificadores,nombreNivel);
			while(list_is_empty(recLiberados) && list_is_empty(planificadorNivel->bloqueados) != 0){
				Personaje* personajeADesbloquear;
				int posicionPersonaje;
				MPS_MSG* mensajeDeDesbloqueo;
				mensajeDeDesbloqueo = malloc(sizeof(MPS_MSG));
				mensajeDeDesbloqueo->PayloadDescriptor =(char)5;
				char* unRecurso = list_remove(recLiberados,0);
				log_debug(loggerOrquestador,"obtengo el recurso:(%s) de la lista.", unRecurso);
				posicionPersonaje = list_find_personaje(planificadorNivel->bloqueados,unRecurso);
				personajeADesbloquear = list_get(planificadorNivel->bloqueados, posicionPersonaje);

					if(posicionPersonaje!= 0){
						log_debug(loggerOrquestador,"El personaje al que se le dará el recurso, para su desbloqueo, es: (%s)", personajeADesbloquear->simbolo);
						enviarMensaje(personajeADesbloquear->socket,mensajeDeDesbloqueo);
						list_add(recAsignados,unRecurso);
						queue_push(planificadorNivel->listos,personajeADesbloquear);
						list_remove(planificadorNivel->bloqueados,posicionPersonaje-1);
					}
			}
			t_stream* stream = malloc(sizeof(t_stream));
			stream = NivelRecursosLiberados_serializer(recAsignados);
			log_debug(loggerOrquestador,"Se acaba de serializar la lista de recursosAsignados para enviarla al procesoNivel");
			mensajeRecursosAsignados->PayloadDescriptor= 10;
			mensajeRecursosAsignados->PayLoadLength = stream->length;
			mensajeRecursosAsignados->Payload= stream->data;
			log_debug(loggerOrquestador,"Se envian los recursos asignados al Nivel: (%S)", nombreNivel);
			enviarMensaje(socket,mensajeRecursosAsignados);
			break;
		case CHEQUEO_INTERBLOQUEO:
			log_debug(loggerOrquestador, "Se debe chequear el deadlock!");
			break;
		default:
			//si se cierra el nivel llega un msj cualquier entonces cerramos este socket.
			nivelSigueVivo=0;
			close(socket);
			break;
		}
		free(mensaje);
	}
 }


int list_find_personaje(t_list* personajesBloqueados,char* unRecurso){
	Personaje* personaje;
	int posicion = 0;
	int encontre = 0;
		while (list_is_empty(personajesBloqueados)==encontre){
			personaje = list_get(personajesBloqueados, posicion);
			if (personaje->causaBloqueo == unRecurso) {
				encontre=1;
				}
			else posicion++;
		}

		if (encontre==0) {
			return 0;
			}
		else return posicion+1;
	}

void* buscarPjAMatar(char* nombreNivel,t_list *pjsEnDeadlock){
	Personaje* pjAMatar;
	int encontreAQuienMatar=0;
	//esta funcion es para iterar la lista de personajes del planificador
	void matarElPrimero(Personaje* pjEnLista){
		//esta funcion es para iterar la lista de pjsEnDeadlock
		int esElPersonaje(char *pjNombre){
				return string_equals_ignore_case(pjEnLista->simbolo, pjNombre);
		}

		Personaje *personaje;
		personaje=list_find(pjsEnDeadlock,(void*)esElPersonaje);
		if(!encontreAQuienMatar && (personaje != NULL)){
			printf("encontramos a quien matar\n");
			pjAMatar = pjEnLista;
			encontreAQuienMatar=1;
		}
	}

	Planificador *planificador = dictionary_get(planificadores,nombreNivel);
	list_iterate(planificador->personajes,(void*)matarElPrimero);
	printf("dentro del buscarPjAMatar(), el nombre que devolvemos va a ser:\n");
	printf("%c\n",*pjAMatar->simbolo);
	return pjAMatar;
}

char * ipDelSocket(int socket) {
   struct sockaddr_in adr_inet;/* AF_INET */
   int len_inet;  /* length */

   //obtenemos la ip que usa el socket
   len_inet = sizeof adr_inet;
   if ( getsockname(socket, (struct sockaddr *)&adr_inet,(socklen_t*) &len_inet) == -1 ) {
      return NULL; //error
   }

   return inet_ntoa(adr_inet.sin_addr);
}

void llamarKoopa() {
	char *prog[] = { "koopa","/home/utnso/Escritorio/koopa/koopa.txt", NULL };
	execv("/home/utnso/Escritorio/koopa/koopa",prog);
}

