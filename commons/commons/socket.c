#include "socket.h"

int realizarConexion(int* socketEscucha) {
	*socketEscucha = iniciarServidor(0);
	struct sockaddr_in sAddr;
	socklen_t len = sizeof(sAddr);
	getsockname(*socketEscucha, (struct sockaddr *) &sAddr, &len);
	return ntohs(sAddr.sin_port);
}

socket_t iniciarServidor(int puerto) {
	int sSocket;
	struct sockaddr_in sAddr;
	int yes = 1;

	sSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (sSocket == -1) {
		//"Error en socket"
		return -1;
	}

	sAddr.sin_family = AF_INET;
	sAddr.sin_port = htons(puerto);
	sAddr.sin_addr.s_addr = htonl(INADDR_ANY );

	memset(&(sAddr.sin_zero), '\0', 8);

	/* Para no esperar 1 minute a que el socket se libere */

	if (setsockopt(sSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
			== -1) {
		//"Error en setsockopt"
		return -1;
	}

	if (bind(sSocket, (struct sockaddr *) &sAddr, sizeof(sAddr)) == -1) {
		//"Error en bind"
		return -1;
	}

	if (listen(sSocket, 1) == -1) {
		//"Error en listen"
		return -1;
	}

	return sSocket;
}

socket_t conectarAlServidor(char *IPServidor, int PuertoServidor) {
	struct sockaddr_in Servidor;
	socket_t Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (Socket == -1)
		return -1;

	Servidor.sin_family = AF_INET;
	Servidor.sin_addr.s_addr = inet_addr((char*) IPServidor);
	Servidor.sin_port = htons(PuertoServidor);
	memset(Servidor.sin_zero, '\0', sizeof(Servidor.sin_zero));

	if (connect(Socket, (struct sockaddr *) &Servidor, sizeof(struct sockaddr))
			== -1)
		return -1;

	return Socket;
}

int recibirInfo(int Socket, buffer_t Buffer, int CantidadARecibir) {
	long Recibido = 0;
	long TotalRecibido = 0;

	while (TotalRecibido < CantidadARecibir) {

		Recibido = recv(Socket, Buffer + TotalRecibido,
				CantidadARecibir - TotalRecibido, MSG_WAITALL);

		switch (Recibido) {
		case 0:
			return TotalRecibido;
			break;

		case -1:
			return -1;
			break;

		default:
			TotalRecibido += Recibido;
			break;
		}
	}

	return TotalRecibido;
}

int enviarInfo(int Socket, buffer_t Buffer, int CantidadAEnviar) {
	int Enviados = 0;
	int TotalEnviados = 0;

	while (TotalEnviados < CantidadAEnviar) {
		Enviados = send(Socket, Buffer + TotalEnviados,
				CantidadAEnviar - TotalEnviados, 0);
		switch (Enviados) {
		case 0:
			return TotalEnviados;
			break;

		case -1:
			return -1;
			break;

		default:
			TotalEnviados += Enviados;
			break;
		}
	};

	return TotalEnviados;
}

int enviarMensaje(socket_t Socket, MPS_MSG *mensaje) {
	int retorno;
	MPS_MSG_HEADER cabecera;
	unsigned char *BufferEnviar;
	int largoHeader = sizeof(cabecera.PayloadDescriptor)
			+ sizeof(cabecera.PayLoadLength);
	int largoTotal = largoHeader + mensaje->PayLoadLength;

	BufferEnviar = malloc(sizeof(unsigned char) * largoTotal);
	cabecera.PayloadDescriptor = mensaje->PayloadDescriptor;
	cabecera.PayLoadLength = mensaje->PayLoadLength;
	//Copia la cabecera al buffer.
	memcpy(BufferEnviar, &cabecera, largoHeader);
	//copia el mensaje seguido de la cabezera dentro del buffer.
	memcpy(BufferEnviar + largoHeader, mensaje->Payload,
			mensaje->PayLoadLength);

	setsockopt(Socket, SOL_SOCKET, SO_SNDBUF, &largoTotal, sizeof(int));
	retorno = enviarInfo(Socket, BufferEnviar, largoTotal);

	free(BufferEnviar);
	if (retorno < largoTotal) {
		return -1;
	};

	return 1;
}

int recibirMensaje(int Socket, MPS_MSG *mensaje) {
	int retorno;
	MPS_MSG_HEADER cabecera;
	mensaje->PayLoadLength = 0;
	int largoHeader = sizeof cabecera.PayloadDescriptor
			+ sizeof cabecera.PayLoadLength;

	// Recibo Primero el Header

	retorno = recibirInfo(Socket, (unsigned char *) &cabecera, largoHeader);
	if (retorno == 0) {
		return 0;
	};

	if (retorno < largoHeader) {
		return -1;
	};

	// Desarmo la cabecera
	mensaje->PayloadDescriptor = cabecera.PayloadDescriptor;
	mensaje->PayLoadLength = cabecera.PayLoadLength;

	// Si es necesario recibo Primero el PayLoad.
	if (mensaje->PayLoadLength != 0) {
		mensaje->Payload = malloc(mensaje->PayLoadLength + 1);
		memset(mensaje->Payload, '\0', mensaje->PayLoadLength + 1);

		retorno = recibirInfo(Socket, mensaje->Payload,
				(mensaje->PayLoadLength));
	} else {
		mensaje->Payload = NULL;
	}

	return retorno;
}

t_stream* nivelConexion_serializer(NivelConexion* self) {
	char *data = malloc(
			(sizeof(int) * 2) + strlen(self->ipNivel)
					+ strlen(self->ipPlanificador) + 2);
	t_stream *stream = malloc(sizeof(t_stream));
	int offset = 0, tmp_size = 0;
	memcpy(data, self->ipPlanificador,
			tmp_size = strlen(self->ipPlanificador) + 1);
	offset = tmp_size;

	memcpy(data + offset, &self->puertoPlanificador, tmp_size = sizeof(int));
	offset += tmp_size;

	memcpy(data + offset, self->ipNivel, tmp_size = strlen(self->ipNivel) + 1);
	offset += tmp_size;

	memcpy(data + offset, &self->puertoNivel, tmp_size = sizeof(int));
	offset += tmp_size;

	stream->length = offset;
	stream->data = data;

	return stream;
}

NivelConexion* nivelConexion_desserializer(t_stream *stream) {
	NivelConexion* self = malloc(sizeof(NivelConexion));
	int offset = 0, tmp_size = 0;

	for (tmp_size = 1; (stream->data)[tmp_size - 1] != '\0'; tmp_size++);
	self->ipPlanificador = malloc(tmp_size);
	memcpy(self->ipPlanificador, stream->data, tmp_size);

	offset = tmp_size;

	memcpy(&self->puertoPlanificador, stream->data + offset, tmp_size =
			sizeof(int));

	offset += tmp_size;

	for (tmp_size = 1; (stream->data + offset)[tmp_size - 1] != '\0';
			tmp_size++);
	self->ipNivel = malloc(tmp_size);
	memcpy(self->ipNivel, stream->data + offset, tmp_size);

	offset += tmp_size;

	memcpy(&self->puertoNivel, stream->data + offset, tmp_size = sizeof(int));

	return self;
}

t_stream* NivelDatos_serializer(NivelDatos* self) {
	char *data = malloc(sizeof(int) + strlen(self->ip) + strlen(self->nombre) + 2);
	t_stream *stream = malloc(sizeof(t_stream));
	int offset = 0, tmp_size = 0;
	memcpy(data, self->nombre, tmp_size = strlen(self->nombre) + 1);
	offset = tmp_size;

	memcpy(data + offset, self->ip, tmp_size = strlen(self->ip) + 1);
	offset += tmp_size;

	memcpy(data + offset, &self->puerto, tmp_size = sizeof(int));
	offset += tmp_size;

	stream->length = offset;
	printf("  cacacacacaca el puerto es (%d) \n \n \n ",offset);
	stream->data = data;

	return stream;
}

NivelDatos* NivelDatos_desserializer(char *stream) {
	NivelDatos* self = malloc(sizeof(NivelDatos));
	int offset = 0, tmp_size = 0;

	for (tmp_size = 1; (stream)[tmp_size - 1] != '\0'; tmp_size++);
	self->nombre = malloc(tmp_size);
	memcpy(self->nombre, stream, tmp_size);
	offset = tmp_size;
	printf("offset: %d!!!!!!!!!!!!!1 \n\n\n",offset);
	for (tmp_size = 1; (stream+ offset)[tmp_size - 1] != '\0';tmp_size++);
	self->ip = malloc(tmp_size);
	memcpy(self->ip, stream + offset, tmp_size);

	offset += tmp_size;
	printf("offset: %d!!!!!!!!!!!!!1 \n\n\n",offset);
	memcpy(&self->puerto, stream + offset, tmp_size = sizeof(int));

	return self;
}

