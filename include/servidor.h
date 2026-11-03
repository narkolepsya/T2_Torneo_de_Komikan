#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "juego.h"

#include <array>
#include <atomic>
#include <memory>
#include <signal.h>
#include <vector>

using namespace std;

struct EstadoTorneo {
    int jugadoresAdmitidos = 0;
    int parejasFormadas = 0;
    int parejasEsperaron = 0;
    int partidasIniciadas = 0;
    int partidasTerminadas = 0;
    int mesasActivas = 0;
    int maxMesasActivas = 0;
    int maxParejasEsperando = 0;
    int llegadasCreadas = 0;
    int mesasCreadas = 0;
    bool cerrado = false;
    bool interrumpido = false;
    bool fallo = false;
};

class Servidor {
public:
    static constexpr int maximoJugadores = 128;

    Servidor() = default;
    ~Servidor();
    Servidor(const Servidor&) = delete;
    Servidor& operator=(const Servidor&) = delete;

    bool ejecutar(int jugadores, int mesas, const Config& configuracion, string& error);
    void solicitarCierre(MotivoFinalizacion motivo = MotivoFinalizacion::Interrupcion);
    bool notificarAbandono(int idJugador, CausaAbandono causa = CausaAbandono::Desconexion);
    void obtenerEstado(EstadoTorneo& resultado);

private:
    struct Llegada {
        Servidor* servidor = nullptr;
        int id = 0;
        pthread_t hilo{};
        bool creado = false;
    };

    struct Pareja {
        int idPartida = 0;
        int puma = 0;
        int perros = 0;
        bool espero = false;
    };

    struct Mesa {
        Servidor* servidor = nullptr;
        int id = 0;
        pthread_t hilo{};
        bool creado = false;
        bool ocupada = false;
        bool juegoDisponible = false;
        Pareja pareja;
        Juego juego;
    };

    static constexpr size_t capacidadEventos = 64;
    atomic<bool> inicializado{false};
    pthread_mutex_t mutex;
    pthread_cond_t cambios;
    pthread_cond_t hayEventos;
    pthread_cond_t hayEspacio;
    int condicionesInicializadas = 0;
    bool mutexInicializado = false;
    Config configuracion;
    int cantidadJugadores = 0;
    int cantidadMesas = 0;
    vector<Llegada> llegadas;
    vector<unique_ptr<Mesa>> mesas;
    array<int, maximoJugadores> colaJugadores{};
    size_t primerJugador = 0;
    size_t jugadoresEnCola = 0;
    array<Pareja, maximoJugadores / 2> colaParejas{};
    size_t primeraPareja = 0;
    size_t parejasEnCola = 0;
    array<bool, maximoJugadores + 1> abandonaron{};
    array<bool, maximoJugadores + 1> terminaron{};
    array<CausaAbandono, maximoJugadores + 1> causas{};
    array<EventoRegistro, capacidadEventos> eventos;
    size_t primerEvento = 0;
    size_t eventosEnCola = 0;
    bool inicioLlegadas = false;
    bool cerrando = false;
    bool finPublicador = false;
    bool falloRegistro = false;
    MotivoFinalizacion motivoCierre = MotivoFinalizacion::SinFinalizar;
    EstadoTorneo estado;
    string ultimoError;
    EjecucionRegistro ejecucion;
    RegistroEventos registro;
    pthread_t hiloPublicador{};
    pthread_t hiloSenales{};
    bool publicadorCreado = false;
    bool senalesCreadas = false;
    sigset_t conjuntoSenales;

    bool preparar(int jugadores, int mesas, const Config& datos, string& error);
    void liberarRecursos();
    void cerrarConBloqueo(MotivoFinalizacion motivo);
    void esperarCapacidadConBloqueo(size_t cantidad);
    void encolarConBloqueo(TipoEventoRegistro tipo, const Pareja& pareja, int idMesa = 0,
                          MotivoRegistro motivo = MotivoRegistro::Ninguno,
                          ResultadoRegistro resultado = ResultadoRegistro::SinGanador);
    void despachar();
    void admitir(int id);
    void atenderMesa(Mesa& mesa);
    void publicarEventos();
    void controlarSenales();
    static void* ejecutarLlegada(void* argumento);
    static void* ejecutarMesa(void* argumento);
    static void* ejecutarPublicador(void* argumento);
    static void* ejecutarSenales(void* argumento);
    static void notificarErrorRegistro(void* argumento);
};

#endif
