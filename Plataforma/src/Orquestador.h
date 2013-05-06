

#define DIRECCION INADDR_ANY   //INADDR_ANY representa la direccion de cualquier
								//interfaz conectada con la computadora
#define PUERTO 5000
#define BUFF_SIZE 1024

//------ TIPOS DE MENSAJES!!------
#define ERROR_MENSAJE 0
#define PJ_PIDE_NIVEL 1 	//pj pide ip y puerto del nivel y planificador
#define REGISTRAR_NIVEL 2	//mensaje de un nivel
#define PJ_BLOQUEADO 3
#define RESOLVER_INTERBLOQUEO 4
//#define MSJ_XX 5
//#define MSJ_XX 6
//#define MSJ_XX 7
//#define MSJ_XX 8
//#define MSJ_XX 9

typedef struct{
	char* nombre;
	char* ip;
	int puerto;
	int socket;
}Nivel;

typedef struct{
	char* nombreNivel; //nivel q planifica... se entiende no?
	char* ip;
	int puerto;
	t_list bloqueados;
	t_list personajes; //por orden de llegada para saber cual matar en caso de bloqueo
}Planificador;

typedef struct{
	char* nombre;
}Personaje;


int iniciarOrquestador() ;
void enviarMsjError(int *socket);
void manejarConexion(int* socket);
int registrarNivel(Nivel *nivel);


