#ifndef JUEGO_H
#define JUEGO_H
#include "arbitro.h"
#include "config.h"
#include <array>
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
    ErrorCreacionHilo
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

    bool iniciar(const Config& configuracion, int idMesa, int idPartida, string& error);
    void solicitarCierre();
    bool esperarFinalizacion(ResultadoPartida& resultado, string& error);
    bool obtenerInstantanea(Tablero& resultado);
    void obtenerEstado(EstadoJuego& resultado);

private:
    static constexpr int cantidadFichas = Tablero::ultimo_perro + 1;
    static constexpr int cantidadParticipantes = cantidadFichas + 1;

    Tablero tablero;
    Config configuracion;
    MonitorArbitro monitor;
    EstadoJuego estado;
    pthread_cond_t condicionEstado;
    bool condicionInicializada = false;
    array<ContextoFichaJuego, cantidadFichas> contextos;
    array<pthread_t, cantidadFichas> hilosFichas{};
    array<bool, cantidadFichas> fichasCreadas{};
    pthread_t hiloArbitro{};
    bool arbitroCreado = false;

    static void* ejecutarFicha(void* argumento);
    static void* ejecutarArbitro(void* argumento);
    void esperarInicioYCierre(bool esArbitro);
    bool unirHilos(string& error);
    void liberarRecursos();
};

#endif
