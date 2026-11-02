#ifndef JUEGO_H
#define JUEGO_H
#include "arbitro.h"
#include "config.h"
#include <array>
#include <cstdint>
#include <random>
#include <string>
using namespace std;

enum class FasePartida {
    SinIniciar,
    Preparando,
    EnCurso,
    Cerrando,
    Finalizada
};

enum class GanadorPartida {
    SinGanador,
    Puma,
    Perros
};

enum class MotivoFinalizacion {
    SinFinalizar,
    CierreSolicitado,
    ErrorPreparacion,
    ErrorCreacionHilo,
    ErrorEjecucion,
    SeisCapturas,
    PumaAcorralado
};

struct ResultadoPartida {
    GanadorPartida ganador = GanadorPartida::SinGanador;
    MotivoFinalizacion motivo = MotivoFinalizacion::SinFinalizar;
};

struct EstadoJuego {
    FasePartida fase = FasePartida::SinIniciar;
    ResultadoPartida resultado;
    int hilosCreados = 0;
    int hilosPreparados = 0;
    int hilosIniciados = 0;
    int hilosEsperandoCierre = 0;
    int hilosTerminados = 0;
    int hilosUnidos = 0;
    bool inicioAutorizado = false;
    uint64_t secuenciaEvento = 0;
    uint64_t accionesAceptadas = 0;
    uint64_t accionesRechazadas = 0;
    array<uint64_t, 13> solicitudesPorFicha{};
    array<uint64_t, 13> consultasPorFicha{};
    array<bool, 13> fichasTerminadas{};
    array<bool, 13> sinCandidatas{};
};

enum class TipoEventoPartida {
    Inicio,
    Accion,
    Cierre
};

struct EventoPartida {
    uint64_t secuencia = 0;
    TipoEventoPartida tipo = TipoEventoPartida::Inicio;
    int idFicha = -1;
    Movimiento movimiento;
    int idCapturado = -1;
    EstadoRespuesta respuesta = Pendiente;
    ResultadoPartida resultado;
    Tablero tablero;
    bool eventosOmitidos = false;
};

class Juego;

struct ContextoFichaJuego {
    int idFicha = 0;
    mt19937 generador;
    Juego* juego = nullptr;
};

class Juego {
public:
    Juego() = default;
    ~Juego();
    Juego(const Juego&) = delete;
    Juego& operator=(const Juego&) = delete;

    bool iniciar(const Config& configuracion, int idMesa, int idPartida, string& error, bool movimientosAutomaticos = true);
    void solicitarCierre();
    bool esperarFinalizacion(ResultadoPartida& resultado, string& error);
    bool obtenerInstantanea(Tablero& resultado);
    void obtenerEstado(EstadoJuego& resultado);
    EstadoRespuesta solicitarMovimiento(int idFicha, const Movimiento& movimiento);
    bool esperarSalidaFicha(int idFicha, int milisegundos);
    bool esperarEvento(uint64_t anterior, EventoPartida& resultado, int milisegundos);

private:
    static constexpr int cantidadFichas = Tablero::ultimo_perro + 1;
    static constexpr int cantidadParticipantes = cantidadFichas + 1;

    Tablero tablero;
    Config configuracion;
    MonitorArbitro monitor;
    EstadoJuego estado;
    pthread_cond_t condicionEstado;
    pthread_cond_t condicionCambios;
    bool cambiosInicializados = false;
    bool movimientosAutomaticos = true;
    bool ejecucionAutorizada = false;
    uint64_t versionTablero = 0;
    EventoPartida ultimoEvento;
    bool condicionInicializada = false;
    array<ContextoFichaJuego, cantidadFichas> contextos;
    array<pthread_t, cantidadFichas> hilosFichas{};
    array<bool, cantidadFichas> fichasCreadas{};
    pthread_t hiloArbitro{};
    bool arbitroCreado = false;

    static void* ejecutarFicha(void* argumento);
    static void* ejecutarArbitro(void* argumento);
    bool esperarInicio(bool esArbitro);
    void cicloFicha(ContextoFichaJuego& contexto);
    void cicloArbitro();
    bool esperarRetardoConBloqueo(int idFicha, int milisegundos);
    void terminarParticipante(int idFicha);
    void evaluarVictoriaConBloqueo();
    void finalizarConBloqueo(GanadorPartida ganador, MotivoFinalizacion motivo);
    void publicarEventoConBloqueo(TipoEventoPartida tipo, int idFicha,
                                  const Movimiento& movimiento, int idCapturado,
                                  EstadoRespuesta respuesta);
    bool unirHilos(string& error);
    void liberarRecursos();
};

#endif
