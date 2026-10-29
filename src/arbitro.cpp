#include "arbitro.h"

#include <exception>
#include <new>

using namespace std;

namespace {

void comprobarSincronizacion(int resultado) {
    if (resultado != 0) {
        terminate();
    }
}

}

bool inicializarArbitro(MonitorArbitro& monitor) {
    if (monitor.inicializado) {
        return false;
    }

    if (pthread_mutex_init(&monitor.mutexMonitor, nullptr) != 0) {
        return false;
    }

    if (pthread_cond_init(&monitor.condicionCola, nullptr) != 0) {
        comprobarSincronizacion(pthread_mutex_destroy(&monitor.mutexMonitor));
        return false;
    }

    for (int i = 0; i < 13; i++) {
        if (pthread_cond_init(&monitor.buzones[i].condicion, nullptr) != 0) {
            for (int j = 0; j < i; j++) {
                comprobarSincronizacion(pthread_cond_destroy(&monitor.buzones[j].condicion));
            }
            comprobarSincronizacion(pthread_cond_destroy(&monitor.condicionCola));
            comprobarSincronizacion(pthread_mutex_destroy(&monitor.mutexMonitor));
            return false;
        }
        monitor.buzones[i].solicitudPendiente = false;
        monitor.buzones[i].solicitudRecibida = false;
        monitor.buzones[i].respuesta = Pendiente;
    }

    while (!monitor.colaSolicitudes.empty()) {
        monitor.colaSolicitudes.pop();
    }

    monitor.partidaActiva = true;
    monitor.inicializado = true;
    return true;
}

void destruirArbitro(MonitorArbitro& monitor) {
    if (!monitor.inicializado) {
        return;
    }

    for (int i = 0; i < 13; i++) {
        comprobarSincronizacion(pthread_cond_destroy(&monitor.buzones[i].condicion));
    }
    comprobarSincronizacion(pthread_cond_destroy(&monitor.condicionCola));
    comprobarSincronizacion(pthread_mutex_destroy(&monitor.mutexMonitor));
    monitor.partidaActiva = false;
    monitor.inicializado = false;
}

EstadoRespuesta enviarSolicitudBloqueante(MonitorArbitro& monitor, const SolicitudMovimiento& solicitud) {
    if (solicitud.idFicha < 0 || solicitud.idFicha >= 13 || !monitor.inicializado) {
        return Rechazado;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));

    if (!monitor.partidaActiva) {
        comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
        return PartidaTerminada;
    }

    BuzonFicha& buzon = monitor.buzones[solicitud.idFicha];
    if (buzon.solicitudPendiente) {
        comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
        return Rechazado;
    }

    try {
        monitor.colaSolicitudes.push(solicitud);
    } catch (const bad_alloc&) {
        comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
        return Rechazado;
    }

    buzon.solicitudPendiente = true;
    buzon.solicitudRecibida = false;
    buzon.respuesta = Pendiente;
    comprobarSincronizacion(pthread_cond_signal(&monitor.condicionCola));

    while (buzon.respuesta == Pendiente && monitor.partidaActiva) {
        comprobarSincronizacion(pthread_cond_wait(&buzon.condicion, &monitor.mutexMonitor));
    }

    EstadoRespuesta resultado = buzon.respuesta;
    buzon.solicitudPendiente = false;
    buzon.solicitudRecibida = false;
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
    return resultado;
}

bool recibirSolicitud(MonitorArbitro& monitor, SolicitudMovimiento& solicitud) {
    if (!monitor.inicializado) {
        return false;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));

    while (monitor.colaSolicitudes.empty() && monitor.partidaActiva) {
        comprobarSincronizacion(pthread_cond_wait(&monitor.condicionCola, &monitor.mutexMonitor));
    }

    if (!monitor.partidaActiva) {
        comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
        return false;
    }

    solicitud = monitor.colaSolicitudes.front();
    monitor.colaSolicitudes.pop();
    monitor.buzones[solicitud.idFicha].solicitudRecibida = true;
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
    return true;
}

void responderSolicitud(MonitorArbitro& monitor, int idFicha, EstadoRespuesta respuesta) {
    if (idFicha < 0 || idFicha >= 13 || !monitor.inicializado ||
        (respuesta != Aprobado && respuesta != Rechazado)) {
        return;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));
    BuzonFicha& buzon = monitor.buzones[idFicha];

    if (monitor.partidaActiva && buzon.solicitudPendiente &&
        buzon.solicitudRecibida && buzon.respuesta == Pendiente) {
        buzon.respuesta = respuesta;
        comprobarSincronizacion(pthread_cond_signal(&buzon.condicion));
    }
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
}

void finalizarPartida(MonitorArbitro& monitor) {
    if (!monitor.inicializado) {
        return;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));
    monitor.partidaActiva = false;

    while (!monitor.colaSolicitudes.empty()) {
        monitor.colaSolicitudes.pop();
    }

    for (int i = 0; i < 13; i++) {
        if (monitor.buzones[i].respuesta == Pendiente) {
            monitor.buzones[i].respuesta = PartidaTerminada;
        }
        comprobarSincronizacion(pthread_cond_broadcast(&monitor.buzones[i].condicion));
    }

    comprobarSincronizacion(pthread_cond_broadcast(&monitor.condicionCola));
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
}
