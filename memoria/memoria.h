#ifndef LIBMEMORIA_H_
#define LIBMEMORIA_H_
    #include "commons/collections/list.h"

    typedef char* t_memoria;

    typedef struct {
        char id;
        int inicio;
        int tamanio;
        char* dato;
        bool libre;
    } t_particion;


    t_memoria crear_memoria(int tamanio);
    int almacenar_particion(t_memoria segmento, char id, int tamanio, char* contenido);
    int eliminar_particion(t_memoria segmento, char id);
    void liberar_memoria(t_memoria segmento);
    t_list* particiones(t_memoria segmento);
    int tamanioPeorEspacio(t_memoria segmento);
    int buscarPeorEspacio(t_memoria segmento);
    int grabarContenido(t_memoria segmento, char* contenido);
    int buscarID(t_list *self, char id);
    int buscarDireccion(t_list *self, int dir);
    int buscarMayorDireccion(t_list *self);
    int buscarPeorParticion(t_list *self);
    bool _particion_menor(t_particion *list, t_particion *listaParticiones);
#endif /* LIBMEMORIA_H_ */
