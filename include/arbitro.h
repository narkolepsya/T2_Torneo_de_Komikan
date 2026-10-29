#ifndef ARBITRO_H
#define ARBITRO_H

#include <pthread.h>
#include <queue>

using namespace std;

enum EstadoRespuesta {
    Pendiente,
    Aprobado,
    Rechazado,
    PartidaTerminada
};

struct SolicitudMovimiento {
    int idFicha;
    int filaOrigen;
    int columnaOrigen;
    int filaDestino;
    int columnaDestino;
};

struct BuzonFicha {
    bool solicitudPendiente = false;
    bool solicitudRecibida = false;
    EstadoRespuesta respuesta = Pendiente;
    pthread_cond_t condicion;
};

struct MonitorArbitro {
    MonitorArbitro() = default;
    MonitorArbitro(const MonitorArbitro&) = delete;
    MonitorArbitro& operator=(const MonitorArbitro&) = delete;

    queue<SolicitudMovimiento> colaSolicitudes;
    pthread_mutex_t mutexMonitor;
    pthread_cond_t condicionCola;
    BuzonFicha buzones[13];
    bool partidaActiva = false;
    bool inicializado = false;
};

bool inicializarArbitro(MonitorArbitro& monitor);
void destruirArbitro(MonitorArbitro& monitor);
EstadoRespuesta enviarSolicitudBloqueante(MonitorArbitro& monitor, const SolicitudMovimiento& solicitud);
bool recibirSolicitud(MonitorArbitro& monitor, SolicitudMovimiento& solicitud);
void responderSolicitud(MonitorArbitro& monitor, int idFicha, EstadoRespuesta respuesta);
void finalizarPartida(MonitorArbitro& monitor);

#endif
