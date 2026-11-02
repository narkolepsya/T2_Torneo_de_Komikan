#include "juego.h"
#include <cstring>
#include <exception>
using namespace std;
namespace {

void comprobarPosix(int resultado) {
    if (resultado != 0) {
        terminate();
    }
}

}

Juego::~Juego() {
    if (monitor.inicializado) {
        solicitarCierre();
        ResultadoPartida resultado;
        string error;

        if (!esperarFinalizacion(resultado, error)) {
            terminate();
        }

        liberarRecursos();
    }
}

bool Juego::iniciar(const Config& nuevaConfiguracion, int idMesa, int idPartida, string& error) {
    if (arbitroCreado) {
        error = "La partida anterior requiere cierre y join antes de reiniciar";
        return false;
    }

    for (bool creada : fichasCreadas) {
        if (creada) {
            error = "Hay trabajadores pendientes de join";
            return false;
        }
    }

    liberarRecursos();
    estado = EstadoJuego();
    tablero = Tablero();
    Tablero comprobacion;

    if (idMesa < 0 || idPartida < 0 || nuevaConfiguracion.retardoMinimo < 0 ||
        nuevaConfiguracion.retardoMaximo < nuevaConfiguracion.retardoMinimo ||
        !comprobacion.cargarDisposicion(nuevaConfiguracion.disposicionInicial)) {
        estado.fase = FasePartida::Finalizada;
        estado.resultado.motivo = MotivoFinalizacion::ErrorPreparacion;
        error = "Configuracion o identificadores de partida invalidos";
        return false;
    }

    configuracion = nuevaConfiguracion;

    try {
        for (int id = 0; id < cantidadFichas; id++) {
            contextos[id].idFicha = id;
            contextos[id].juego = this;
            seed_seq semillas{configuracion.semilla, static_cast<uint32_t>(idMesa),
                              static_cast<uint32_t>(idPartida), static_cast<uint32_t>(id)};
            contextos[id].generador.seed(semillas);
        }
    } catch (const exception&) {
        estado.fase = FasePartida::Finalizada;
        estado.resultado.motivo = MotivoFinalizacion::ErrorPreparacion;
        error = "No se pudieron preparar los generadores de las fichas";
        return false;
    }

    if (!inicializarArbitro(monitor)) {
        estado.fase = FasePartida::Finalizada;
        estado.resultado.motivo = MotivoFinalizacion::ErrorPreparacion;
        error = "No se pudo inicializar el monitor de la partida";
        return false;
    }

    int codigo = pthread_cond_init(&condicionEstado, nullptr);

    if (codigo != 0) {
        destruirArbitro(monitor);
        estado.fase = FasePartida::Finalizada;
        estado.resultado.motivo = MotivoFinalizacion::ErrorPreparacion;
        error = "No se pudo inicializar la condicion de Juego: " + string(strerror(codigo));
        return false;
    }

    condicionInicializada = true;
    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    estado.fase = FasePartida::Preparando;
    codigo = pthread_create(&hiloArbitro, nullptr, ejecutarArbitro, this);

    if (codigo == 0) {
        arbitroCreado = true;
        estado.hilosCreados++;
    }

    for (int id = 0; codigo == 0 && id < cantidadFichas; id++) {
        codigo = pthread_create(&hilosFichas[id], nullptr, ejecutarFicha, &contextos[id]);

        if (codigo == 0) {
            fichasCreadas[id] = true;
            estado.hilosCreados++;
        }
    }

    if (codigo != 0) {
        estado.fase = FasePartida::Cerrando;
        estado.resultado.motivo = MotivoFinalizacion::ErrorCreacionHilo;
        comprobarPosix(pthread_cond_broadcast(&condicionEstado));
    } else {
        while (estado.fase == FasePartida::Preparando &&
               estado.hilosPreparados < cantidadParticipantes) {
            comprobarPosix(pthread_cond_wait(&condicionEstado, &monitor.mutexMonitor));
        }

        if (estado.fase == FasePartida::Preparando) {
            estado.inicioAutorizado = true;
            estado.fase = FasePartida::EnCurso;
            comprobarPosix(pthread_cond_broadcast(&condicionEstado));

            while (estado.fase == FasePartida::EnCurso &&
                   estado.hilosIniciados < cantidadParticipantes) {
                comprobarPosix(pthread_cond_wait(&condicionEstado, &monitor.mutexMonitor));
            }
        }
    }

    bool iniciado = estado.fase == FasePartida::EnCurso;
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));

    if (!iniciado) {
        solicitarCierre();
        ResultadoPartida resultado;
        string errorLimpieza;
        bool unidos = esperarFinalizacion(resultado, errorLimpieza);

        if (unidos) {
            liberarRecursos();
        }

        error = codigo != 0 ? "No se pudo crear un hilo de partida: " + string(strerror(codigo)) :
                              "La partida se cerro durante la preparacion";

        if (!unidos) {
            error += "; " + errorLimpieza;
        }

        return false;
    }

    error.clear();
    return true;
}

void Juego::solicitarCierre() {
    if (!monitor.inicializado) {
        return;
    }

    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));

    if (estado.fase != FasePartida::Finalizada) {
        estado.fase = FasePartida::Cerrando;

        if (estado.resultado.motivo == MotivoFinalizacion::SinFinalizar) {
            estado.resultado.motivo = MotivoFinalizacion::CierreSolicitado;
        }
    }

    comprobarPosix(pthread_cond_broadcast(&condicionEstado));
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    finalizarPartida(monitor);
}

bool Juego::esperarFinalizacion(ResultadoPartida& resultado, string& error) {
    if (!monitor.inicializado) {
        error = "La partida no tiene recursos inicializados";
        return false;
    }

    if (!unirHilos(error)) {
        return false;
    }

    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    estado.fase = FasePartida::Finalizada;
    resultado = estado.resultado;
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    error.clear();
    return true;
}

bool Juego::unirHilos(string& error) {
    bool unidos = true;

    for (int id = 0; id < cantidadFichas; id++) {
        if (!fichasCreadas[id]) {
            continue;
        }

        int codigo = pthread_join(hilosFichas[id], nullptr);

        if (codigo != 0) {
            error = "No se pudo unir un trabajador: " + string(strerror(codigo));
            unidos = false;
            continue;
        }

        fichasCreadas[id] = false;
        comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
        estado.hilosUnidos++;
        comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    }

    if (arbitroCreado) {
        int codigo = pthread_join(hiloArbitro, nullptr);

        if (codigo != 0) {
            error = "No se pudo unir el arbitro: " + string(strerror(codigo));
            unidos = false;
        } else {
            arbitroCreado = false;
            comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
            estado.hilosUnidos++;
            comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
        }
    }
    return unidos;
}

bool Juego::obtenerInstantanea(Tablero& resultado) {
    if (!monitor.inicializado) {
        return false;
    }

    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    bool disponible = estado.inicioAutorizado;

    if (disponible) {
        resultado = tablero;
    }

    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    return disponible;
}

void Juego::obtenerEstado(EstadoJuego& resultado) {
    if (!monitor.inicializado) {
        resultado = estado;
        return;
    }

    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    resultado = estado;
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
}

void Juego::liberarRecursos() {
    if (condicionInicializada) {
        comprobarPosix(pthread_cond_destroy(&condicionEstado));
        condicionInicializada = false;
    }

    destruirArbitro(monitor);
}

void* Juego::ejecutarFicha(void* argumento) {
    ContextoFichaJuego& contexto = *static_cast<ContextoFichaJuego*>(argumento);
    contexto.juego->esperarInicioYCierre(false);
    return nullptr;
}

void* Juego::ejecutarArbitro(void* argumento) {
    Juego& juego = *static_cast<Juego*>(argumento);
    juego.esperarInicioYCierre(true);
    return nullptr;
}

void Juego::esperarInicioYCierre(bool esArbitro) {
    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));

    if (esArbitro && estado.fase == FasePartida::Preparando &&
        !tablero.cargarDisposicion(configuracion.disposicionInicial)) {
        estado.fase = FasePartida::Cerrando;
        estado.resultado.motivo = MotivoFinalizacion::ErrorPreparacion;
    }

    estado.hilosPreparados++;
    comprobarPosix(pthread_cond_broadcast(&condicionEstado));

    while (estado.fase == FasePartida::Preparando) {
        comprobarPosix(pthread_cond_wait(&condicionEstado, &monitor.mutexMonitor));
    }

    if (estado.inicioAutorizado && estado.fase == FasePartida::EnCurso) {
        estado.hilosIniciados++;
        estado.hilosEsperandoCierre++;
        comprobarPosix(pthread_cond_broadcast(&condicionEstado));

        while (estado.fase == FasePartida::EnCurso) {
            comprobarPosix(pthread_cond_wait(&condicionEstado, &monitor.mutexMonitor));
        }

        estado.hilosEsperandoCierre--;
    }

    estado.hilosTerminados++;
    comprobarPosix(pthread_cond_broadcast(&condicionEstado));
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
}
