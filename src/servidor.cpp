#include "servidor.h"
#include "consola.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>

using namespace std;

namespace {

void comprobarPosix(int codigo) {
    if (codigo != 0) {
        terminate();
    }
}

class BloqueoServidor {
public:
    explicit BloqueoServidor(pthread_mutex_t& mutex) : mutex(mutex) { tomar(); }
    ~BloqueoServidor() { soltar(); }
    BloqueoServidor(const BloqueoServidor&) = delete;
    BloqueoServidor& operator=(const BloqueoServidor&) = delete;
    void tomar() {
        if (!tomado) {
            comprobarPosix(pthread_mutex_lock(&mutex));
            tomado = true;
        }
    }
    void soltar() {
        if (tomado) {
            comprobarPosix(pthread_mutex_unlock(&mutex));
            tomado = false;
        }
    }
private:
    pthread_mutex_t& mutex;
    bool tomado = false;
};

MotivoRegistro convertirMotivo(MotivoFinalizacion motivo) {
    switch (motivo) {
        case MotivoFinalizacion::SinFinalizar: return MotivoRegistro::Ninguno;
        case MotivoFinalizacion::CierreSolicitado: return MotivoRegistro::CierreSolicitado;
        case MotivoFinalizacion::Interrupcion: return MotivoRegistro::Interrupcion;
        case MotivoFinalizacion::ErrorPreparacion: return MotivoRegistro::ErrorPreparacion;
        case MotivoFinalizacion::ErrorCreacionHilo: return MotivoRegistro::ErrorCreacionHilo;
        case MotivoFinalizacion::ErrorEjecucion: return MotivoRegistro::ErrorEjecucion;
        case MotivoFinalizacion::ErrorRegistro: return MotivoRegistro::ErrorRegistro;
        case MotivoFinalizacion::SeisCapturas: return MotivoRegistro::SeisCapturas;
        case MotivoFinalizacion::PumaAcorralado: return MotivoRegistro::PumaAcorralado;
        case MotivoFinalizacion::AbandonoPuma: return MotivoRegistro::AbandonoPuma;
        case MotivoFinalizacion::AbandonoPerros: return MotivoRegistro::AbandonoPerros;
        case MotivoFinalizacion::FalloJugadorPuma: return MotivoRegistro::FalloJugadorPuma;
        case MotivoFinalizacion::FalloJugadorPerros: return MotivoRegistro::FalloJugadorPerros;
    }
    return MotivoRegistro::ErrorEjecucion;
}

}

Servidor::~Servidor() {
    liberarRecursos();
}

bool Servidor::preparar(int jugadores, int limiteMesas, const Config& datos, string& error) {
    Tablero comprobacion;
    if (jugadores < 2 || jugadores > maximoJugadores || jugadores % 2 != 0 || limiteMesas < 1 ||
        datos.retardoMinimo < 0 || datos.retardoMaximo < datos.retardoMinimo ||
        !comprobacion.cargarDisposicion(datos.disposicionInicial)) {
        error = "Parametros invalidos: N par entre 2 y 128, M positivo y configuracion valida";
        return false;
    }
    cantidadJugadores = jugadores;
    cantidadMesas = limiteMesas;
    configuracion = datos;
    try {
        llegadas.resize(jugadores);
        for (int i = 0; i < jugadores; i++) {
            llegadas[i].servidor = this;
            llegadas[i].id = i + 1;
        }
        for (int id = 1; id <= min(limiteMesas, jugadores / 2); id++) {
            unique_ptr<Mesa> mesa(new Mesa);
            mesa->id = id;
            mesa->servidor = this;
            mesas.push_back(move(mesa));
        }
    } catch (const exception&) {
        error = "No se pudo reservar memoria para el torneo";
        return false;
    }
    int codigo = pthread_mutex_init(&mutex, nullptr);
    if (codigo != 0) {
        error = "No se pudo inicializar el mutex del servidor: " + string(strerror(codigo));
        return false;
    }
    mutexInicializado = true;
    for (pthread_cond_t* condicion : {&cambios, &hayEventos, &hayEspacio}) {
        codigo = pthread_cond_init(condicion, nullptr);
        if (codigo != 0) {
            error = "No se pudo inicializar una condicion del servidor: " + string(strerror(codigo));
            return false;
        }
        condicionesInicializadas++;
    }
    inicializado.store(true);
    return true;
}

void Servidor::liberarRecursos() {
    inicializado.store(false);
    pthread_cond_t* condiciones[] = {&cambios, &hayEventos, &hayEspacio};
    while (condicionesInicializadas > 0) {
        comprobarPosix(pthread_cond_destroy(condiciones[--condicionesInicializadas]));
    }
    if (mutexInicializado) {
        comprobarPosix(pthread_mutex_destroy(&mutex));
        mutexInicializado = false;
    }
}

void Servidor::cerrarConBloqueo(MotivoFinalizacion motivo) {
    if (!cerrando) {
        cerrando = true;
        motivoCierre = motivo;
        estado.interrumpido = motivo == MotivoFinalizacion::Interrupcion;
    }
    if (motivo != MotivoFinalizacion::Interrupcion && motivo != MotivoFinalizacion::CierreSolicitado) {
        estado.fallo = true;
    }
    for (auto& mesa : mesas) {
        if (mesa->ocupada && mesa->juegoDisponible) {
            mesa->juego.solicitarCierre(motivoCierre);
        }
    }
    comprobarPosix(pthread_cond_broadcast(&cambios));
    comprobarPosix(pthread_cond_broadcast(&hayEspacio));
}

void Servidor::solicitarCierre(MotivoFinalizacion motivo) {
    if (!inicializado.load()) {
        return;
    }
    BloqueoServidor bloqueo(mutex);
    cerrarConBloqueo(motivo);
}

void Servidor::notificarErrorRegistro(void* argumento) {
    Servidor& servidor = *static_cast<Servidor*>(argumento);
    BloqueoServidor bloqueo(servidor.mutex);
    servidor.falloRegistro = true;
    servidor.cerrarConBloqueo(MotivoFinalizacion::ErrorRegistro);
}

bool Servidor::notificarAbandono(int id, CausaAbandono causa) {
    if (!inicializado.load()) {
        return false;
    }
    BloqueoServidor bloqueo(mutex);
    if (cerrando || id < 1 || id > cantidadJugadores || abandonaron[id] || terminaron[id] ||
        (causa != CausaAbandono::Desconexion && causa != CausaAbandono::FalloInterno)) {
        return false;
    }
    for (auto& mesa : mesas) {
        if (mesa->ocupada && mesa->juegoDisponible &&
            (mesa->pareja.puma == id || mesa->pareja.perros == id)) {
            if (!mesa->juego.notificarAbandono(id, causa)) {
                return false;
            }
        }
    }
    abandonaron[id] = true;
    causas[id] = causa;
    return true;
}

void Servidor::obtenerEstado(EstadoTorneo& resultado) {
    if (!inicializado.load()) {
        resultado = EstadoTorneo();
        return;
    }
    BloqueoServidor bloqueo(mutex);
    resultado = estado;
}

void Servidor::esperarCapacidadConBloqueo(size_t cantidad) {
    while (capacidadEventos - eventosEnCola < cantidad && !falloRegistro) {
        comprobarPosix(pthread_cond_wait(&hayEspacio, &mutex));
    }
}

void Servidor::encolarConBloqueo(TipoEventoRegistro tipo, const Pareja& pareja, int mesa,
                                MotivoRegistro motivo, ResultadoRegistro resultado) {
    if (falloRegistro) {
        return;
    }
    if (eventosEnCola == capacidadEventos) {
        terminate();
    }
    EventoRegistro& evento = eventos[(primerEvento + eventosEnCola++) % capacidadEventos];
    evento = EventoRegistro();
    evento.tipo = tipo;
    evento.tiempo = tomarTiempoRegistro();
    evento.idPartida = pareja.idPartida;
    evento.idMesa = mesa;
    evento.jugador1 = pareja.puma;
    evento.jugador2 = pareja.perros;
    evento.motivo = motivo;
    evento.resultado = resultado;
    evento.mesasActivas = estado.mesasActivas;
    evento.parejasEsperando = static_cast<int>(parejasEnCola);
    evento.movimiento = {TipoMovimiento::Simple, {-1, -1}, {-1, -1}, {-1, -1}};
    comprobarPosix(pthread_cond_signal(&hayEventos));
}

void Servidor::admitir(int id) {
    BloqueoServidor bloqueo(mutex);
    while (!inicioLlegadas && !cerrando) {
        comprobarPosix(pthread_cond_wait(&cambios, &mutex));
    }
    esperarCapacidadConBloqueo(1);
    if (cerrando) {
        return;
    }
    colaJugadores[(primerJugador + jugadoresEnCola++) % colaJugadores.size()] = id;
    estado.jugadoresAdmitidos++;
    encolarConBloqueo(TipoEventoRegistro::LlegadaJugador, {0, id, 0, false});
    comprobarPosix(pthread_cond_broadcast(&cambios));
}

void Servidor::despachar() {
    BloqueoServidor bloqueo(mutex);
    while (!cerrando && estado.partidasTerminadas < cantidadJugadores / 2) {
        esperarCapacidadConBloqueo(2);
        if (cerrando) {
            break;
        }
        if (jugadoresEnCola >= 2) {
            int puma = colaJugadores[primerJugador];
            primerJugador = (primerJugador + 1) % colaJugadores.size();
            int perros = colaJugadores[primerJugador];
            primerJugador = (primerJugador + 1) % colaJugadores.size();
            jugadoresEnCola -= 2;
            Pareja pareja{++estado.parejasFormadas, puma, perros, false};
            colaParejas[(primeraPareja + parejasEnCola++) % colaParejas.size()] = pareja;
            estado.maxParejasEsperando = max(estado.maxParejasEsperando, static_cast<int>(parejasEnCola));
            encolarConBloqueo(TipoEventoRegistro::Emparejamiento, pareja);
        }
        Mesa* libre = nullptr;
        for (auto& mesa : mesas) {
            if (!mesa->ocupada) {
                libre = mesa.get();
                break;
            }
        }
        if (parejasEnCola > 0 && libre != nullptr) {
            libre->pareja = colaParejas[primeraPareja];
            primeraPareja = (primeraPareja + 1) % colaParejas.size();
            parejasEnCola--;
            libre->ocupada = true;
            estado.mesasActivas++;
            estado.maxMesasActivas = max(estado.maxMesasActivas, estado.mesasActivas);
            encolarConBloqueo(TipoEventoRegistro::AsignacionMesa, libre->pareja, libre->id);
            comprobarPosix(pthread_cond_broadcast(&cambios));
            continue;
        }
        if (parejasEnCola > 0 && libre == nullptr) {
            Pareja& pareja = colaParejas[(primeraPareja + parejasEnCola - 1) % colaParejas.size()];
            if (!pareja.espero) {
                pareja.espero = true;
                estado.parejasEsperaron++;
                encolarConBloqueo(TipoEventoRegistro::EsperaMesa, pareja);
            }
        }
        if (jugadoresEnCola >= 2) {
            continue;
        }
        if (estado.partidasTerminadas < cantidadJugadores / 2) {
            comprobarPosix(pthread_cond_wait(&cambios, &mutex));
        }
    }
}

void Servidor::atenderMesa(Mesa& mesa) {
    BloqueoServidor bloqueo(mutex);
    while (true) {
        while (!mesa.ocupada && !cerrando) {
            comprobarPosix(pthread_cond_wait(&cambios, &mutex));
        }
        if (!mesa.ocupada) {
            return;
        }
        OpcionesRegistro opciones;
        opciones.identidad.ejecucion = ejecucion;
        opciones.identidad.jugadorPuma = mesa.pareja.puma;
        opciones.identidad.jugadorPerros = mesa.pareja.perros;
        Pareja pareja = mesa.pareja;
        bloqueo.soltar();
        string error;
        bool iniciada = mesa.juego.iniciar(configuracion, mesa.id, pareja.idPartida, error, true, opciones);
        bloqueo.tomar();
        mesa.juegoDisponible = iniciada;
        if (iniciada) {
            estado.partidasIniciadas++;
            if (cerrando) {
                mesa.juego.solicitarCierre(motivoCierre);
            } else {
                for (int id : {pareja.puma, pareja.perros}) {
                    if (abandonaron[id]) {
                        mesa.juego.notificarAbandono(id, causas[id]);
                    }
                }
            }
        }
        bloqueo.soltar();
        ResultadoPartida resultado;
        if (!iniciada) {
            EstadoJuego estadoJuego;
            mesa.juego.obtenerEstado(estadoJuego);
            resultado = estadoJuego.resultado;
        }
        bool guardada = iniciada && mesa.juego.esperarFinalizacion(resultado, error);
        if (!guardada) {
            imprimirConsola("Error en partida " + to_string(pareja.idPartida) + ": " + error, true);
        }
        bloqueo.tomar();
        mesa.juegoDisponible = false;
        terminaron[pareja.puma] = true;
        terminaron[pareja.perros] = true;
        esperarCapacidadConBloqueo(3);
        if (!guardada) {
            encolarConBloqueo(TipoEventoRegistro::Error, pareja, mesa.id,
                iniciada ? MotivoRegistro::ErrorRegistro : MotivoRegistro::ErrorPreparacion);
            cerrarConBloqueo(iniciada ? MotivoFinalizacion::ErrorRegistro : MotivoFinalizacion::ErrorPreparacion);
        }
        estado.partidasTerminadas++;
        encolarConBloqueo(TipoEventoRegistro::ResultadoPartida, pareja, mesa.id,
            convertirMotivo(resultado.motivo), resultado.ganador == GanadorPartida::Puma ? ResultadoRegistro::Puma :
                (resultado.ganador == GanadorPartida::Perros ? ResultadoRegistro::Perros : ResultadoRegistro::SinGanador));
        mesa.ocupada = false;
        estado.mesasActivas--;
        encolarConBloqueo(TipoEventoRegistro::LiberacionMesa, pareja, mesa.id);
        comprobarPosix(pthread_cond_broadcast(&cambios));
    }
}

void Servidor::publicarEventos() {
    while (true) {
        BloqueoServidor bloqueo(mutex);
        while (eventosEnCola == 0 && !finPublicador) {
            comprobarPosix(pthread_cond_wait(&hayEventos, &mutex));
        }
        if (eventosEnCola == 0) {
            return;
        }
        EventoRegistro evento = eventos[primerEvento];
        primerEvento = (primerEvento + 1) % capacidadEventos;
        eventosEnCola--;
        comprobarPosix(pthread_cond_broadcast(&hayEspacio));
        bool fallido = falloRegistro;
        bloqueo.soltar();
        if (fallido) {
            continue;
        }
        string error;
        if (!registro.reservar(1, error)) {
            notificarErrorRegistro(this);
            continue;
        }
        if (!registro.publicar(&evento, 1, 1, error)) {
            registro.cancelarReserva(1);
            notificarErrorRegistro(this);
            continue;
        }

    }
}

void Servidor::controlarSenales() {
    while (true) {
        int senal = 0;
        comprobarPosix(sigwait(&conjuntoSenales, &senal));
        if (senal == SIGUSR1) {
            return;
        }
        solicitarCierre(MotivoFinalizacion::Interrupcion);
    }
}

void* Servidor::ejecutarLlegada(void* argumento) {
    Llegada& llegada = *static_cast<Llegada*>(argumento);
    try {
        llegada.servidor->admitir(llegada.id);
    } catch (...) {
        llegada.servidor->solicitarCierre(MotivoFinalizacion::ErrorEjecucion);
    }
    return nullptr;
}

void* Servidor::ejecutarMesa(void* argumento) {
    Mesa& mesa = *static_cast<Mesa*>(argumento);
    try {
        mesa.servidor->atenderMesa(mesa);
    } catch (...) {
        mesa.servidor->solicitarCierre(MotivoFinalizacion::ErrorEjecucion);
        mesa.juego.solicitarCierre(MotivoFinalizacion::ErrorEjecucion);
        ResultadoPartida resultado;
        string error;
        mesa.juego.esperarFinalizacion(resultado, error);
        EstadoJuego estadoJuego;
        mesa.juego.obtenerEstado(estadoJuego);
        resultado = estadoJuego.resultado;
        Servidor& servidor = *mesa.servidor;
        BloqueoServidor bloqueo(servidor.mutex);
        mesa.juegoDisponible = false;
        servidor.esperarCapacidadConBloqueo(3);
        if (mesa.ocupada) {
            servidor.encolarConBloqueo(TipoEventoRegistro::Error, mesa.pareja, mesa.id, MotivoRegistro::ErrorEjecucion);
            servidor.encolarConBloqueo(TipoEventoRegistro::ResultadoPartida, mesa.pareja, mesa.id,
                resultado.motivo == MotivoFinalizacion::SinFinalizar ? MotivoRegistro::ErrorEjecucion : convertirMotivo(resultado.motivo),
                resultado.ganador == GanadorPartida::Puma ? ResultadoRegistro::Puma :
                    resultado.ganador == GanadorPartida::Perros ? ResultadoRegistro::Perros : ResultadoRegistro::SinGanador);
            servidor.terminaron[mesa.pareja.puma] = true;
            servidor.terminaron[mesa.pareja.perros] = true;
            mesa.ocupada = false;
            servidor.estado.mesasActivas--;
            servidor.estado.partidasTerminadas++;
            servidor.encolarConBloqueo(TipoEventoRegistro::LiberacionMesa, mesa.pareja, mesa.id);
            comprobarPosix(pthread_cond_broadcast(&servidor.cambios));
        }
    }
    return nullptr;
}

void* Servidor::ejecutarPublicador(void* argumento) {
    Servidor& servidor = *static_cast<Servidor*>(argumento);
    try {
        servidor.publicarEventos();
    } catch (...) {
        notificarErrorRegistro(&servidor);
    }
    return nullptr;
}

void* Servidor::ejecutarSenales(void* argumento) {
    static_cast<Servidor*>(argumento)->controlarSenales();
    return nullptr;
}

bool Servidor::ejecutar(int jugadores, int limiteMesas, const Config& datos, string& error) {
    if (mutexInicializado || !llegadas.empty()) {
        error = "Utilice un Servidor nuevo por ejecucion de torneo";
        return false;
    }
    sigset_t anterior;
    comprobarPosix(sigemptyset(&conjuntoSenales));
    comprobarPosix(sigaddset(&conjuntoSenales, SIGINT));
    comprobarPosix(sigaddset(&conjuntoSenales, SIGUSR1));
    int codigo = pthread_sigmask(SIG_BLOCK, &conjuntoSenales, &anterior);
    if (codigo != 0) {
        error = "No se pudieron bloquear las senales del torneo";
        return false;
    }
    if (!preparar(jugadores, limiteMesas, datos, error)) {
        liberarRecursos();
        comprobarPosix(pthread_sigmask(SIG_SETMASK, &anterior, nullptr));
        return false;
    }
    OpcionesRegistro opciones;
    if (!crearEjecucionRegistro("registros", ejecucion, error)) {
        liberarRecursos();
        comprobarPosix(pthread_sigmask(SIG_SETMASK, &anterior, nullptr));
        return false;
    }
    opciones.identidad.ejecucion = ejecucion;
    opciones.identidad.esTorneo = true;
    opciones.identidad.cantidadJugadores = jugadores;
    opciones.identidad.cantidadMesas = limiteMesas;
    if (!registro.abrir(opciones, configuracion, error, notificarErrorRegistro, this)) {
        liberarRecursos();
        comprobarPosix(pthread_sigmask(SIG_SETMASK, &anterior, nullptr));
        return false;
    }
    codigo = pthread_create(&hiloPublicador, nullptr, ejecutarPublicador, this);
    if (codigo != 0) {
        EventoRegistro cierre;
        cierre.tipo = TipoEventoRegistro::Cierre;
        cierre.motivo = MotivoRegistro::ErrorCreacionHilo;
        cierre.tiempo = tomarTiempoRegistro();
        string errorRegistro;
        if (registro.reservar(1, errorRegistro)) {
            registro.publicar(&cierre, 1, 1, errorRegistro);
        }
        registro.cerrar(errorRegistro);
        estado.fallo = true;
        error = "No se pudo crear el publicador del torneo: " + string(strerror(codigo));
        liberarRecursos();
        comprobarPosix(pthread_sigmask(SIG_SETMASK, &anterior, nullptr));
        return false;
    }
    publicadorCreado = true;
    {
        BloqueoServidor bloqueo(mutex);
        encolarConBloqueo(TipoEventoRegistro::Inicio, {});
        codigo = pthread_create(&hiloSenales, nullptr, ejecutarSenales, this);
        senalesCreadas = codigo == 0;
        for (auto& mesa : mesas) {
            if (codigo != 0) {
                break;
            }
            codigo = pthread_create(&mesa->hilo, nullptr, ejecutarMesa, mesa.get());
            mesa->creado = codigo == 0;
            estado.mesasCreadas += mesa->creado;
        }
        for (Llegada& llegada : llegadas) {
            if (codigo != 0) {
                break;
            }
            codigo = pthread_create(&llegada.hilo, nullptr, ejecutarLlegada, &llegada);
            llegada.creado = codigo == 0;
            estado.llegadasCreadas += llegada.creado;
        }
        if (codigo != 0) {
            ultimoError = "No se pudo crear un hilo del torneo: " + string(strerror(codigo));
            cerrarConBloqueo(MotivoFinalizacion::ErrorCreacionHilo);
        } else {
            inicioLlegadas = true;
            comprobarPosix(pthread_cond_broadcast(&cambios));
        }
    }
    try {
        despachar();
    } catch (...) {
        solicitarCierre(MotivoFinalizacion::ErrorEjecucion);
    }
    solicitarCierre(MotivoFinalizacion::CierreSolicitado);
    for (Llegada& llegada : llegadas) {
        if (llegada.creado) {
            comprobarPosix(pthread_join(llegada.hilo, nullptr));
        }
    }
    for (auto& mesa : mesas) {
        if (mesa->creado) {
            comprobarPosix(pthread_join(mesa->hilo, nullptr));
        }
    }
    if (senalesCreadas) {
        comprobarPosix(pthread_kill(hiloSenales, SIGUSR1));
        comprobarPosix(pthread_join(hiloSenales, nullptr));
    }
    {
        BloqueoServidor bloqueo(mutex);
        esperarCapacidadConBloqueo(2);
        if (estado.fallo) {
            encolarConBloqueo(TipoEventoRegistro::Error, {}, 0, convertirMotivo(motivoCierre));
        }
        encolarConBloqueo(TipoEventoRegistro::Cierre, {}, 0, convertirMotivo(motivoCierre));
        finPublicador = true;
        comprobarPosix(pthread_cond_broadcast(&hayEventos));
    }
    comprobarPosix(pthread_join(hiloPublicador, nullptr));
    bool guardado = registro.cerrar(error);
    BloqueoServidor bloqueo(mutex);
    estado.cerrado = true;
    estado.fallo = estado.fallo || !guardado;
    if (estado.fallo && error.empty()) {
        error = ultimoError.empty() ? "El torneo finalizo por un error de infraestructura" : ultimoError;
    }
    bool correcto = !estado.fallo;
    bloqueo.soltar();
    comprobarPosix(pthread_sigmask(SIG_SETMASK, &anterior, nullptr));
    return correcto;
}
