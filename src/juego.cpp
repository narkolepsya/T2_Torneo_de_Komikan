#include "juego.h"
#include "ficha.h"
#include <cerrno>
#include <ctime>
#include <cstring>
#include <exception>
using namespace std;
namespace {

void comprobarPosix(int resultado) {
    if (resultado != 0) {
        terminate();
    }
}

timespec obtenerPlazo(int milisegundos) {
    timespec plazo;
    comprobarPosix(clock_gettime(CLOCK_MONOTONIC, &plazo));
    plazo.tv_sec += milisegundos / 1000;
    plazo.tv_nsec += (milisegundos % 1000) * 1000000L;

    if (plazo.tv_nsec >= 1000000000L) {
        plazo.tv_sec++;
        plazo.tv_nsec -= 1000000000L;
    }

    return plazo;
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

bool Juego::iniciar(const Config& nuevaConfiguracion, int idMesa, int idPartida, string& error, bool automaticos) {
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
    movimientosAutomaticos = automaticos;
    ejecucionAutorizada = false;
    versionTablero = 0;
    ultimoEvento = EventoPartida();
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
    pthread_condattr_t atributos;
    codigo = pthread_condattr_init(&atributos);

    if (codigo == 0) {
        codigo = pthread_condattr_setclock(&atributos, CLOCK_MONOTONIC);

        if (codigo == 0) {
            codigo = pthread_cond_init(&condicionCambios, &atributos);
        }

        comprobarPosix(pthread_condattr_destroy(&atributos));
    }

    if (codigo != 0) {
        liberarRecursos();
        estado.fase = FasePartida::Finalizada;
        estado.resultado.motivo = MotivoFinalizacion::ErrorPreparacion;
        error = "No se pudo inicializar la condicion monotona: " + string(strerror(codigo));
        return false;
    }

    cambiosInicializados = true;
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

    if (estado.fase == FasePartida::EnCurso) {
        publicarEventoConBloqueo(TipoEventoPartida::Inicio, -1, Movimiento(), -1, Pendiente);
        ejecucionAutorizada = true;
        comprobarPosix(pthread_cond_broadcast(&condicionEstado));

        if (!movimientosAutomaticos) {
            while (estado.fase == FasePartida::EnCurso &&
                   estado.hilosEsperandoCierre < cantidadParticipantes) {
                comprobarPosix(pthread_cond_wait(&condicionEstado, &monitor.mutexMonitor));
            }
        }
    }

    bool iniciado = estado.inicioAutorizado;
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

    finalizarConBloqueo(GanadorPartida::SinGanador, MotivoFinalizacion::CierreSolicitado);
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
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
    if (cambiosInicializados) {
        comprobarPosix(pthread_cond_destroy(&condicionCambios));
        cambiosInicializados = false;
    }

    if (condicionInicializada) {
        comprobarPosix(pthread_cond_destroy(&condicionEstado));
        condicionInicializada = false;
    }

    destruirArbitro(monitor);
}

void* Juego::ejecutarFicha(void* argumento) {
    ContextoFichaJuego& contexto = *static_cast<ContextoFichaJuego*>(argumento);
    Juego& juego = *contexto.juego;

    try {
        if (juego.esperarInicio(false)) {
            juego.cicloFicha(contexto);
        }
    } catch (...) {
        comprobarPosix(pthread_mutex_lock(&juego.monitor.mutexMonitor));
        juego.finalizarConBloqueo(GanadorPartida::SinGanador, MotivoFinalizacion::ErrorEjecucion);
        comprobarPosix(pthread_mutex_unlock(&juego.monitor.mutexMonitor));
    }

    juego.terminarParticipante(contexto.idFicha);
    return nullptr;
}

void* Juego::ejecutarArbitro(void* argumento) {
    Juego& juego = *static_cast<Juego*>(argumento);

    try {
        if (juego.esperarInicio(true)) {
            juego.cicloArbitro();
        }
    } catch (...) {
        comprobarPosix(pthread_mutex_lock(&juego.monitor.mutexMonitor));
        juego.finalizarConBloqueo(GanadorPartida::SinGanador, MotivoFinalizacion::ErrorEjecucion);
        comprobarPosix(pthread_mutex_unlock(&juego.monitor.mutexMonitor));
    }

    juego.terminarParticipante(-1);
    return nullptr;
}

bool Juego::esperarInicio(bool esArbitro) {
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
        comprobarPosix(pthread_cond_broadcast(&condicionEstado));

        while (estado.fase == FasePartida::EnCurso && !ejecucionAutorizada) {
            comprobarPosix(pthread_cond_wait(&condicionEstado, &monitor.mutexMonitor));
        }
    }

    bool activo = estado.fase == FasePartida::EnCurso;
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    return activo;
}

void Juego::terminarParticipante(int idFicha) {
    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    estado.hilosTerminados++;

    if (idFicha >= 0) {
        estado.fichasTerminadas[idFicha] = true;
    }

    comprobarPosix(pthread_cond_broadcast(&condicionEstado));
    comprobarPosix(pthread_cond_broadcast(&condicionCambios));
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
}

bool Juego::esperarRetardoConBloqueo(int idFicha, int milisegundos) {
    timespec plazo = obtenerPlazo(milisegundos);
    Posicion posicion;
    estado.hilosEsperandoCierre++;
    comprobarPosix(pthread_cond_broadcast(&condicionEstado));

    while (estado.fase == FasePartida::EnCurso && tablero.buscarFicha(idFicha, posicion)) {
        int codigo = pthread_cond_timedwait(&condicionCambios, &monitor.mutexMonitor, &plazo);

        if (codigo == ETIMEDOUT) {
            break;
        }

        comprobarPosix(codigo);
    }

    estado.hilosEsperandoCierre--;
    return estado.fase == FasePartida::EnCurso && tablero.buscarFicha(idFicha, posicion);
}

void Juego::cicloFicha(ContextoFichaJuego& contexto) {
    int id = contexto.idFicha;

    while (true) {
        int retardo = movimientosAutomaticos ?
            generarRetardo(contexto.generador, configuracion.retardoMinimo, configuracion.retardoMaximo) : 0;
        Tablero instantanea;
        uint64_t version;
        comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
        Posicion posicion;

        if (!movimientosAutomaticos) {
            estado.hilosEsperandoCierre++;
            comprobarPosix(pthread_cond_broadcast(&condicionEstado));

            while (estado.fase == FasePartida::EnCurso && tablero.buscarFicha(id, posicion)) {
                comprobarPosix(pthread_cond_wait(&condicionCambios, &monitor.mutexMonitor));
            }

            estado.hilosEsperandoCierre--;
            comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
            return;
        }

        bool activo = esperarRetardoConBloqueo(id, retardo);
        version = versionTablero;
        instantanea = tablero;
        comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));

        if (!activo) {
            return;
        }

        vector<Movimiento> candidatas = instantanea.obtenerMovimientos(id);
        comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
        estado.consultasPorFicha[id]++;
        estado.sinCandidatas[id] = candidatas.empty();

        if (candidatas.empty()) {
            estado.hilosEsperandoCierre++;
            comprobarPosix(pthread_cond_broadcast(&condicionEstado));

            while (estado.fase == FasePartida::EnCurso && version == versionTablero &&
                   tablero.buscarFicha(id, posicion)) {
                comprobarPosix(pthread_cond_wait(&condicionCambios, &monitor.mutexMonitor));
            }

            estado.hilosEsperandoCierre--;
            comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
            continue;
        }

        comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
        uniform_int_distribution<size_t> seleccion(0, candidatas.size() - 1);
        EstadoRespuesta respuesta = solicitarMovimiento(id, candidatas[seleccion(contexto.generador)]);

        if (respuesta == PartidaTerminada || respuesta == FichaCapturada) {
            return;
        }
    }
}

EstadoRespuesta Juego::solicitarMovimiento(int idFicha, const Movimiento& movimiento) {
    if (idFicha < 0 || idFicha >= cantidadFichas || !monitor.inicializado) {
        return Rechazado;
    }

    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    EstadoRespuesta respuesta;
    Posicion actual;

    if (estado.fase != FasePartida::EnCurso) {
        respuesta = PartidaTerminada;
    } else if (!tablero.buscarFicha(idFicha, actual)) {
        respuesta = FichaCapturada;
    } else if (monitor.buzones[idFicha].solicitudPendiente) {
        respuesta = Rechazado;
    } else {
        SolicitudMovimiento solicitud{idFicha, movimiento.origen.fila, movimiento.origen.columna,
                                     movimiento.destino.fila, movimiento.destino.columna,
                                     movimiento.tipo, movimiento.intermedio};
        estado.solicitudesPorFicha[idFicha]++;
        respuesta = enviarSolicitudConBloqueo(monitor, solicitud);
    }

    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    return respuesta;
}

void Juego::cicloArbitro() {
    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    evaluarVictoriaConBloqueo();

    while (estado.fase == FasePartida::EnCurso) {
        estado.hilosEsperandoCierre++;
        comprobarPosix(pthread_cond_broadcast(&condicionEstado));
        SolicitudMovimiento solicitud;
        bool recibida = recibirSolicitudConBloqueo(monitor, solicitud);
        estado.hilosEsperandoCierre--;

        if (!recibida || estado.fase != FasePartida::EnCurso) {
            break;
        }

        Movimiento movimiento{solicitud.tipo, {solicitud.filaOrigen, solicitud.columnaOrigen},
                              {solicitud.filaDestino, solicitud.columnaDestino}, solicitud.intermedio};
        bool aceptada = false;
        int capturado = -1;

        if (movimiento.tipo == TipoMovimiento::Simple) {
            aceptada = tablero.moverFicha(solicitud.idFicha, movimiento.origen, movimiento.destino);
        } else if (movimiento.tipo == TipoMovimiento::Captura && solicitud.idFicha == Tablero::puma) {
            aceptada = tablero.capturarPerro(movimiento.origen, movimiento.intermedio,
                                            movimiento.destino, capturado);
        }

        EstadoRespuesta respuesta = aceptada ? Aprobado : Rechazado;

        if (aceptada) {
            estado.accionesAceptadas++;
            versionTablero++;

            if (capturado >= Tablero::primer_perro) {
                retirarFichaConBloqueo(monitor, capturado);
            }
        } else {
            estado.accionesRechazadas++;
        }

        responderSolicitudConBloqueo(monitor, solicitud.idFicha, respuesta);
        publicarEventoConBloqueo(TipoEventoPartida::Accion, solicitud.idFicha, movimiento,
                                 capturado, respuesta);
        evaluarVictoriaConBloqueo();
        comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
        comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));
    }

    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
}

void Juego::evaluarVictoriaConBloqueo() {
    if (estado.fase != FasePartida::EnCurso) {
        return;
    }

    Posicion puma;

    if (!tablero.buscarFicha(Tablero::puma, puma)) {
        finalizarConBloqueo(GanadorPartida::SinGanador, MotivoFinalizacion::ErrorEjecucion);
    } else if (Tablero::ultimo_perro - tablero.contarPerrosRestantes() >= 6) {
        finalizarConBloqueo(GanadorPartida::Puma, MotivoFinalizacion::SeisCapturas);
    } else if (!tablero.tieneMovimientosPuma()) {
        finalizarConBloqueo(GanadorPartida::Perros, MotivoFinalizacion::PumaAcorralado);
    }
}

void Juego::finalizarConBloqueo(GanadorPartida ganador, MotivoFinalizacion motivo) {
    if (estado.fase == FasePartida::Finalizada) {
        return;
    }

    bool nuevo = estado.fase != FasePartida::Cerrando || monitor.partidaActiva;
    estado.fase = FasePartida::Cerrando;

    if (estado.resultado.motivo == MotivoFinalizacion::SinFinalizar) {
        estado.resultado = {ganador, motivo};
    }

    finalizarPartidaConBloqueo(monitor);
    versionTablero++;
    comprobarPosix(pthread_cond_broadcast(&condicionEstado));
    comprobarPosix(pthread_cond_broadcast(&condicionCambios));

    if (nuevo) {
        publicarEventoConBloqueo(TipoEventoPartida::Cierre, -1, Movimiento(), -1, PartidaTerminada);
    }
}

void Juego::publicarEventoConBloqueo(TipoEventoPartida tipo, int idFicha,
                                    const Movimiento& movimiento, int idCapturado,
                                    EstadoRespuesta respuesta) {
    ultimoEvento.secuencia = ++estado.secuenciaEvento;
    ultimoEvento.tipo = tipo;
    ultimoEvento.idFicha = idFicha;
    ultimoEvento.movimiento = movimiento;
    ultimoEvento.idCapturado = idCapturado;
    ultimoEvento.respuesta = respuesta;
    ultimoEvento.resultado = estado.resultado;
    ultimoEvento.tablero = tablero;
    comprobarPosix(pthread_cond_broadcast(&condicionCambios));
}

bool Juego::esperarSalidaFicha(int idFicha, int milisegundos) {
    if (!monitor.inicializado || idFicha < 0 || idFicha >= cantidadFichas || milisegundos < 0) {
        return false;
    }

    timespec plazo = obtenerPlazo(milisegundos);
    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));

    while (!estado.fichasTerminadas[idFicha]) {
        int codigo = pthread_cond_timedwait(&condicionCambios, &monitor.mutexMonitor, &plazo);

        if (codigo == ETIMEDOUT) {
            break;
        }

        comprobarPosix(codigo);
    }

    bool termino = estado.fichasTerminadas[idFicha];
    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    return termino;
}

bool Juego::esperarEvento(uint64_t anterior, EventoPartida& resultado, int milisegundos) {
    if (!monitor.inicializado || milisegundos < 0) {
        return false;
    }

    timespec plazo = obtenerPlazo(milisegundos);
    comprobarPosix(pthread_mutex_lock(&monitor.mutexMonitor));

    while (estado.secuenciaEvento <= anterior && estado.fase == FasePartida::EnCurso) {
        int codigo = pthread_cond_timedwait(&condicionCambios, &monitor.mutexMonitor, &plazo);

        if (codigo == ETIMEDOUT) {
            break;
        }

        comprobarPosix(codigo);
    }

    bool disponible = estado.secuenciaEvento > anterior;

    if (disponible) {
        resultado = ultimoEvento;
        resultado.eventosOmitidos = resultado.secuencia - anterior > 1;
    }

    comprobarPosix(pthread_mutex_unlock(&monitor.mutexMonitor));
    return disponible;
}
