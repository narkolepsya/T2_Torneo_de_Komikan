#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <semaphore.h>
#include <pthread.h>
#include <queue>
#include "config.h"

usign namespace std;

struct jugador{
    int id_jugador;
};

struct gestortorneo{
    sem_t semaforo_mesas;
    pthread_mutex_t mutex_emparejamiento;
    pthread_cond_t condicion_cola;
    queue<jugador> cola_espera;
    Config configuracion_global;
    bool torneo_activo;
};

void inicializar_servidor(gestortorneo& gestor, int max_mesas, const Config& config);
void Parar_servidor(gestortorneo& gestor);
void registrar_jugador(gestortorneo& gestor, int id_jugador);
void* despacharmesa(void* arg);

#endif