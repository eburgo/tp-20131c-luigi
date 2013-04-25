/*
 * archivoLog.c
 *
 *  Created on: 22/04/2013
 *      Author: Esteban
 */

#include "conex.h"


char *horaActual() {
        char Fecha [30 * sizeof(char)];
        struct timeval t;
        time_t curtime;
        char *retorno = malloc(13 * sizeof(char));
        char hora[16 * sizeof(char)];

        memset(Fecha, '\0', (30 * sizeof(char)));

        gettimeofday(&t,NULL);
        curtime = t.tv_sec;

        strftime(Fecha, 30, "%T", localtime(&curtime));
        sprintf(hora, "%s.%ld", Fecha, t.tv_usec);
        hora[12] = '\0';
        strcpy(retorno, hora);

        return retorno;
}



int printL(int TipoLog, char* Mensaje) {


	char mensaje[500 * sizeof(char)];
	FILE* archLog;
	char* horaActualRtr;
	char nombreArchivo[25 * sizeof(char *)];


    memset(nombreArchivo, '\0', (sizeof(char)*25));
	memset(mensaje, '\0', (sizeof(char)*500));


	// [TIPOLOG]
	switch (TipoLog) {
	case (LOG_INFO):
		strcat(mensaje,"[INFO] ");
		break;

	case (LOG_ERROR):
		strcat(mensaje,"[ERROR] ");
		break;

	case (LOG_WARN):
		strcat(mensaje,"[WARN] ");
		break;

	case (LOG_DEBUG):
		strcat(mensaje,"[DEBUG] ");
		break;
	}

	// [FECHA]
	horaActualRtr = horaActual();
	strcat(mensaje,horaActualRtr);
	free(horaActualRtr);
	strcat(mensaje," ");

	// NOMBREPROCESO
	strcat(mensaje, "Proceso");

	// PIDPROCESO
	strcat(mensaje,"/");

	strcat(mensaje,"(pid:");

	// TIDPROCESO

	strcat(mensaje,"tid)");

	strcat(mensaje,": ");



	//Mensaje a grabar
	strcat(mensaje,Mensaje);

	//Fin de renglon
	strcat(mensaje,".\n");

	//Escribo en el archivo Log


	strcpy(nombreArchivo, "Log ");
	strcat(nombreArchivo, horaActual());
	strcat(nombreArchivo, ".txt");

	archLog = fopen(nombreArchivo, "a");
	fputs(mensaje, archLog);

	fclose(archLog);

	return 1;
}





