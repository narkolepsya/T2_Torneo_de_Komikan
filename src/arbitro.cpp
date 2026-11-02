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
        monitor.buzones[i].retirada = false;
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
    if (!monitor.inicializado) {
        return Rechazado;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));
    EstadoRespuesta respuesta = enviarSolicitudConBloqueo(monitor, solicitud);
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
    return respuesta;
}

EstadoRespuesta enviarSolicitudConBloqueo(MonitorArbitro& monitor, const SolicitudMovimiento& solicitud) {
    if (solicitud.idFicha < 0 || solicitud.idFicha >= 13) {
        return Rechazado;
    }

    if (!monitor.partidaActiva) {
        return PartidaTerminada;
    }

    BuzonFicha& buzon = monitor.buzones[solicitud.idFicha];

    if (buzon.retirada) {
        return FichaCapturada;
    }

    if (buzon.solicitudPendiente) {
        return Rechazado;
    }

    try {
        monitor.colaSolicitudes.push(solicitud);
    } catch (const bad_alloc&) {
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
    return resultado;
}

bool recibirSolicitud(MonitorArbitro& monitor, SolicitudMovimiento& solicitud) {
    if (!monitor.inicializado) {
        return false;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));
    bool recibida = recibirSolicitudConBloqueo(monitor, solicitud);
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
    return recibida;
}

bool recibirSolicitudConBloqueo(MonitorArbitro& monitor, SolicitudMovimiento& solicitud) {
    while (monitor.partidaActiva) {
        while (monitor.colaSolicitudes.empty() && monitor.partidaActiva) {
            comprobarSincronizacion(pthread_cond_wait(&monitor.condicionCola, &monitor.mutexMonitor));
        }

        if (!monitor.partidaActiva) {
            return false;
        }

        SolicitudMovimiento siguiente = monitor.colaSolicitudes.front();
        monitor.colaSolicitudes.pop();
        BuzonFicha& buzon = monitor.buzones[siguiente.idFicha];

        if (buzon.retirada || !buzon.solicitudPendiente || buzon.respuesta != Pendiente) {
            continue;
        }

        solicitud = siguiente;
        buzon.solicitudRecibida = true;
        return true;
    }

    return false;
}

void responderSolicitud(MonitorArbitro& monitor, int idFicha, EstadoRespuesta respuesta) {
    if (!monitor.inicializado) {
        return;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));
    responderSolicitudConBloqueo(monitor, idFicha, respuesta);
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
}

void responderSolicitudConBloqueo(MonitorArbitro& monitor, int idFicha, EstadoRespuesta respuesta) {
    if (idFicha < 0 || idFicha >= 13 || (respuesta != Aprobado && respuesta != Rechazado)) {
        return;
    }

    BuzonFicha& buzon = monitor.buzones[idFicha];

    if (monitor.partidaActiva && !buzon.retirada && buzon.solicitudPendiente &&
        buzon.solicitudRecibida && buzon.respuesta == Pendiente) {
        buzon.respuesta = respuesta;
        comprobarSincronizacion(pthread_cond_signal(&buzon.condicion));
    }
}

void retirarFichaConBloqueo(MonitorArbitro& monitor, int idFicha) {
    if (idFicha < 1 || idFicha >= 13) {
        return;
    }

    BuzonFicha& buzon = monitor.buzones[idFicha];
    buzon.retirada = true;

    if (buzon.respuesta == Pendiente) {
        buzon.respuesta = FichaCapturada;
    }

    comprobarSincronizacion(pthread_cond_broadcast(&buzon.condicion));
}

void finalizarPartida(MonitorArbitro& monitor) {
    if (!monitor.inicializado) {
        return;
    }

    comprobarSincronizacion(pthread_mutex_lock(&monitor.mutexMonitor));
    finalizarPartidaConBloqueo(monitor);
    comprobarSincronizacion(pthread_mutex_unlock(&monitor.mutexMonitor));
}

void finalizarPartidaConBloqueo(MonitorArbitro& monitor) {
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
}
