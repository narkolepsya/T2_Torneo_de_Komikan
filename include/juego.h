#ifndef JUEGO_H
#define JUEGO_H

#include "config.h"
#include "arbitro.h"
#include "tablero.h"
#include <pthread.h>

usign namespace std;

struct jugador;

struct ContextoFicha {
    int id_ficha;
    MonitorArbitro* monitor;
    int retardo_min;
    int retardo_max;
    int semilla;
    bool es_puma;
};

void iniciar_partida(int id_j1, int id_j2, const config& config);
void* rutina_ficha(void* arg);

#endif