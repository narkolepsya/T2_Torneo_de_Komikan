#ifndef ARBITRO_H
#define ARBITRO_H

#include "tablero.h"

#include <pthread.h>
#include <queue>

using namespace std;

enum EstadoRespuesta {
    Pendiente,
    Aprobado,
    Rechazado,
    PartidaTerminada,
    FichaCapturada
};

struct SolicitudMovimiento {
    int idFicha;
    int filaOrigen;
    int columnaOrigen;
    int filaDestino;
    int columnaDestino;
    TipoMovimiento tipo = TipoMovimiento::Simple;
    Posicion intermedio = {-1, -1};
};

struct BuzonFicha {
    bool solicitudPendiente = false;
    bool solicitudRecibida = false;
    bool retirada = false;
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

EstadoRespuesta enviarSolicitudConBloqueo(MonitorArbitro& monitor, const SolicitudMovimiento& solicitud);
bool recibirSolicitudConBloqueo(MonitorArbitro& monitor, SolicitudMovimiento& solicitud);
void responderSolicitudConBloqueo(MonitorArbitro& monitor, int idFicha, EstadoRespuesta respuesta);
void retirarFichaConBloqueo(MonitorArbitro& monitor, int idFicha);
void finalizarPartidaConBloqueo(MonitorArbitro& monitor);

#endif
