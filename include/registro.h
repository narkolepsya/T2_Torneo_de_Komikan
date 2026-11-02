#ifndef REGISTRO_H
#define REGISTRO_H

#include "config.h"

#include <cstdint>
#include <pthread.h>
#include <string>
#include <vector>

using namespace std;

struct TiempoRegistro {
    int64_t realMs = 0;
    int64_t monotonoNs = 0;
};

TiempoRegistro tomarTiempoRegistro();

struct EjecucionRegistro {
    string directorio;
    string idTorneo;
    TiempoRegistro inicio;
};

bool crearEjecucionRegistro(const string& base, EjecucionRegistro& resultado, string& error);

struct IdentidadRegistro {
    EjecucionRegistro ejecucion;
    int idPartida = 0;
    int idMesa = 0;
    int jugadorPuma = 1;
    int jugadorPerros = 2;
    bool esTorneo = false;
};

struct OpcionesRegistro {
    bool habilitado = true;
    IdentidadRegistro identidad;
    size_t capacidad = 64;
};

enum class TipoEventoRegistro {
    Inicio,
    Movimiento,
    Captura,
    Rechazo,
    Victoria,
    Cierre,
    Error,
    LlegadaJugador,
    Emparejamiento,
    AsignacionMesa,
    LiberacionMesa,
    ResultadoPartida
};

enum class ResultadoRegistro {
    SinGanador,
    Puma,
    Perros
};

enum class MotivoRegistro {
    Ninguno,
    Confirmado,
    OrigenObsoleto,
    PosicionInexistente,
    DestinoOcupado,
    NoAdyacente,
    CapturaSoloPuma,
    IntermedioSinPerro,
    TrayectoriaInvalida,
    TipoInvalido,
    SeisCapturas,
    PumaAcorralado,
    CierreSolicitado,
    Interrupcion,
    ErrorPreparacion,
    ErrorCreacionHilo,
    ErrorEjecucion,
    ErrorRegistro
};

struct EventoRegistro {
    TipoEventoRegistro tipo = TipoEventoRegistro::Inicio;
    MotivoRegistro motivo = MotivoRegistro::Ninguno;
    ResultadoRegistro resultado = ResultadoRegistro::SinGanador;
    TiempoRegistro tiempo;
    int idFicha = -1;
    Movimiento movimiento;
    int idCapturado = -1;
    int idPartida = -1;
    int idMesa = -1;
    int jugador1 = -1;
    int jugador2 = -1;
    Tablero tablero;
};

struct EstadoRegistro {
    size_t enCola = 0;
    size_t reservados = 0;
    size_t productoresEsperando = 0;
    uint64_t eventosEscritos = 0;
    bool fallo = false;
    bool cerrado = false;
};

class RegistroEventos {
public:
    RegistroEventos() = default;
    ~RegistroEventos();
    RegistroEventos(const RegistroEventos&) = delete;
    RegistroEventos& operator=(const RegistroEventos&) = delete;

    bool abrir(const OpcionesRegistro& opciones, const Config& configuracion, string& error,
               void (*avisarError)(void*) = nullptr, void* contexto = nullptr);
    bool reservar(size_t cantidad, string& error);
    bool publicar(const EventoRegistro* eventos, size_t cantidad, size_t reserva, string& error);
    void cancelarReserva(size_t cantidad);
    bool cerrar(string& error);
    void obtenerEstado(EstadoRegistro& resultado);
    string obtenerRuta() const;

private:
    pthread_mutex_t mutex;
    pthread_cond_t hayEventos;
    pthread_cond_t hayCapacidad;
    pthread_t hilo;
    bool inicializado = false;
    bool hiloCreado = false;
    bool cerrando = false;
    bool cierrePublicado = false;
    bool anunciado = false;
    int descriptor = -1;
    vector<EventoRegistro> cola;
    size_t primero = 0;
    EstadoRegistro estado;
    IdentidadRegistro identidad;
    Config configuracion;
    TiempoRegistro inicio;
    string ruta;
    string ultimoError;
    int64_t duracionUs = 0;
    ResultadoRegistro resultadoFinal = ResultadoRegistro::SinGanador;
    void (*avisarError)(void*) = nullptr;
    void* contextoError = nullptr;

    static void* ejecutar(void* argumento);
    void escribirEventos();
    bool escribirTexto(const string& texto);
    void fallar(const string& mensaje);
    void liberarRecursos();
};

#endif
