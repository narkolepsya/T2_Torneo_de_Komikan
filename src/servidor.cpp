#include "servidor.h"
#include "juego.h"
#include <iostream>

struct DatosPartida {
    gestortorneo* gestor;
    jugador j1;
    jugador j2;
};

void inicializar_servidor(gestortorneo& gestor, int max_mesas, const Config& config){
    sem_init(&gestor.semaforo_mesas, 0, max_mesas);
    pthread_mutex_init(&gestor.mutex_emparejamiento, nullptr);
    pthread_cond_init(&gestor.condicion_cola, nullptr);
    gestor.configuracion_global = config;
    gestor.torneo_activo = true;
}

void Parar_servidor(gestortorneo& gestor){
    gestor.torneo_activo = false;
    pthread_cond_broadcast(&gestor.condicion_cola);

    sem_destroy(&gestor.semaforo_mesas);
    pthread_mutex_destroy(&gestor.mutex_emparejamiento);
    pthread_cond_destroy(&gestor.condicion_cola);
}

void registrar_jugador(gestortorneo& gestor, int id_jugador) {
    pthread_mutex_lock(&gestor.mutex_emparejamiento);

    jugador nuevo = {id_jugador};
    gestor.cola_espera.push(nuevo);

    if (gestor.cola_espera.size() >= 2){
        pthread_cond_signal(&gestor.condicion_cola);
    }

    pthread_mutex_unlock(&gestor.mutex_emparejamiento);
}

void* hilo_partida(void* arg){
    DatosPartida* datos = (DatosPartida*) arg;

    inciar_partida(datos->j1.id_jugador, datos->j2.id_jugador, datos->gestor->configuracion_global);

    sem_post(&datos->gestor->semaforo_mesas);

    delete datos;
    return nullptr;
}

void* despacharmesa(void* arg){
    gestortorneo* gestor = (gestortorneo*) arg;

    while (true){
        pthread_mutex_lock(&gestor->mutex_emparejamiento);

        while (gestor->cola_espera.size() < 2 && gestor->torneo_activo){
            pthread_cond_wait(&gestor->condicion_cola, &gestor->mutex_emparejamiento);
        }

        if (!gestor->torneo_activo && gestor->cola_espera.size() < 2){
            pthread_mutex_unlock(&gestor->mutex_emparejamiento);
            break;
        }

        jugador j1 = gestor->cola_espera.front(); gestor.cola_espera.pop();
        jugador j2 = gestor->cola_espera.front(); gestor.cola_espera.pop();

        pthread_mutex_unlock(&gestor->semaforo_mesas);

        sem_wait(&gestor->semaforo_mesas);

        DatosPartida* datos = new DatosPartida{gestor, j1, j2};
        pthread_t tid_partida;
        pthread_create(&tid_partida, nullptr, hilo_partida, datos);
        pthread_detach(tid_partida);
    }

    return nullptr;
}