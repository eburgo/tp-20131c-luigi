/*
 * conex.c
 *
 *  Created on: 22/04/2013
 *      Author: Esteban
 */


#include "conex.h"



socket_t iniciarServidor(int puerto)
{
	int sSocket;
	struct sockaddr_in sAddr;
	int yes = 1;

	sSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (sSocket == -1)
	{
		printL(LOG_ERROR, "Error en socket");
		return -1;
	}

	sAddr.sin_family = AF_INET;
	sAddr.sin_port = htons(puerto);
	sAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(sAddr.sin_zero), '\0', 8);

	/* Para no esperar 1 minute a que el socket se libere */

	if (setsockopt(sSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		printL(LOG_ERROR, "Error en setsockopt");
		return -1;
	}

	if (bind(sSocket, (struct sockaddr *) &sAddr, sizeof(sAddr)) == -1)
	{
		printL(LOG_ERROR, "Error en bind");
		return -1;
	}

	if (listen(sSocket, 1) == -1)
	{
		printL(LOG_ERROR, "Error en listen");
		return -1;
	}

	return sSocket;
}


socket_t aceptarConexion(socket_t cSocket)
{

	struct sockaddr_in cAddr;
	socket_t cSocket_aceptado;
	unsigned int address_size = sizeof(cAddr);

	cSocket_aceptado = accept(cSocket, (struct sockaddr *) &cAddr, &address_size);

	if (cSocket_aceptado == -1)
	{
		printL(LOG_ERROR, "Error de accept");
		return -1;
	}

	return cSocket_aceptado;
}


socket_t conectarAlServidor(char *IPServidor, int PuertoServidor)
{
	struct sockaddr_in Servidor;
	socket_t Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (Socket == -1) return -1;

	Servidor.sin_family = AF_INET;
	Servidor.sin_addr.s_addr = inet_addr((char*) IPServidor);
	Servidor.sin_port = htons(PuertoServidor);
	memset(Servidor.sin_zero, '\0', sizeof(Servidor.sin_zero));

	if (connect(Socket, (struct sockaddr *) &Servidor, sizeof(struct sockaddr))== -1) return -1;

	return Socket;
}


int recibirInfo(int Socket, buffer_t Buffer, int CantidadARecibir)
{
        long Recibido = 0;
        long TotalRecibido = 0;

        while (TotalRecibido < CantidadARecibir)
        {

                Recibido = recv(Socket, Buffer + TotalRecibido, CantidadARecibir - TotalRecibido, MSG_WAITALL);


                switch (Recibido)
                {
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


int enviarInfo(int Socket, buffer_t Buffer, int CantidadAEnviar)
{
        int Enviados = 0;
        int TotalEnviados = 0;

        while (TotalEnviados < CantidadAEnviar)
        {
                Enviados = send(Socket, Buffer + TotalEnviados,CantidadAEnviar - TotalEnviados, 0);
                switch (Enviados)
                {
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


int enviarMensaje(socket_t Socket, MPS_MSG *mensaje)
{
        int retorno;
        MPS_MSG_HEADER cabecera;
        unsigned char *BufferEnviar;
        int largoHeader = sizeof(cabecera.PayloadDescriptor) + sizeof(cabecera.PayLoadLength);
        int largoTotal = largoHeader + mensaje->PayLoadLength;


        BufferEnviar = malloc(sizeof(unsigned char) * largoTotal);
        cabecera.PayloadDescriptor = mensaje->PayloadDescriptor;
        cabecera.PayLoadLength = mensaje->PayLoadLength;
        memcpy(BufferEnviar,&cabecera, largoHeader);
        memcpy(BufferEnviar+largoHeader,mensaje->Payload, mensaje->PayLoadLength);

        setsockopt(Socket, SOL_SOCKET, SO_SNDBUF, &largoTotal, sizeof(int));
        retorno = enviarInfo(Socket, BufferEnviar, largoTotal);

        free(BufferEnviar);
        if (retorno < largoTotal)
        {
                return -1;
        };

        return 1;
}


int recibirMensaje(int Socket, MPS_MSG *mensaje)
{
        int retorno;
        MPS_MSG_HEADER cabecera;
        mensaje->PayLoadLength = 0;
        int largoHeader = sizeof cabecera.PayloadDescriptor + sizeof cabecera.PayLoadLength;

        // Recibo Primero el Header

        retorno = recibirInfo(Socket, (unsigned char *) &cabecera, largoHeader);
        if (retorno == 0)
        {
            return 0;
        };

        if (retorno < largoHeader)
        {
                return -1;
        };

        // Desarmo la cabecera
        mensaje->PayloadDescriptor = cabecera.PayloadDescriptor;
        mensaje->PayLoadLength = cabecera.PayLoadLength;

        // Si es necesario recibo Primero el PayLoad.
        if (mensaje->PayLoadLength != 0)
        {
                mensaje->Payload = malloc(mensaje->PayLoadLength + 1);
                memset(mensaje->Payload, '\0', mensaje->PayLoadLength + 1);

                retorno = recibirInfo(Socket, mensaje->Payload,(mensaje->PayLoadLength));
        }
        else
        {
                mensaje->Payload = NULL;
        }

        return retorno;
}

