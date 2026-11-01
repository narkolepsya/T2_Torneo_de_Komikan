#include "juego.h"
#include <iostream>
#include <random>
#include <chrono>
#include <thread>

using namespace std;

void* rutina_ficha(void* arg){
    ContextoFicha* cf = (ContextoFicha*)arg;
    
    mt19937 gen(cf->semilla);
    uniform_int_distribution<> dis(cf->retardo_min, cf->retardo_max);

    while (cf->monitor->partida_activa){
        int retardo_ms = dis(gen);

        this_thread::sleep_for(chrono::milliseconds(retardo_ms));

        if (!cf->monitor->partida_activa){
            break
        };

        int fila_origen = -1;
        int col_origen = -1; 
        int fila_destino = -1;
        int col_destino = -1;

        if (fila_origen = -1){
            continue;
        }

        SolicitudMovimiento Sol;
        sol.id_ficha = cf->id_ficha;
        sol.fila_origen = fila_origen;
        sol.columna_origen = col_origen;
        sol.fila_destino = fila_destino;
        sol.columna_destino = col_destino;

        pthread_mutex_lock(&cf->monitor->mutex_monitor);
        cf->monitor->cola_solicitudes.push(sol);

        cf->monitor->buzones[cf->id.ficha].solicitud_pendiente = true; 
        cf->monitor->buzones[cf->id.ficha].respuesta = PENDIENTE;

        phtread_cond_signal(&cf->monitor->condicion_cola);

        while (cf->monitor->buzones[cf->id_ficha].respuesta == PENDIENTE && cf->monitor->partida_activa){
            phtread_cond_wait(&cf->monitor->buzones[cf->id_ficha].condicion, &cf->monitor->mutex_monitor);
        }

        cf->monitor->buzones[cf->id_ficha].solicitud_pendiente = false;
        phtread_mutex_unlock(&cf->monitor->mutex_monitor);
    }

    delete cf;
    return nullptr;
}

void iniciar_partida(int id_j1, int id_j2, const Config& config){
    Tablero tablero;
    MonitorArbitro monitor;

    phtread_t hilos_fichas[13];

    ContextoFicha* cf_puma = new ContextoFicha{0, &monitor, config.retardoMinimo, config.retardoMaximo, config semilla, true};
    phtread_create(&hilos_fichas[0], nullptr, rutina_ficha, cf_puma);

    for (int i = 1; i <= 12, i++){
        ContextoFicha* cf_perro = new ContextoFicha{i, &monitor, config.retardo_max, config.retardoMaximo, config.semilla + i; false};
        phtread_create(&hilos_fichas[1], nullptr, rutina_ficha, cf_perro);

    }

    for(int i = 0; i<13; i++){
        phtread_join(hilos_fichas[i], nullptr);
    }
}