#include "consola.h"
#include "servidor.h"

#include <atomic>
#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

namespace {
pthread_mutex_t mutexConsola = PTHREAD_MUTEX_INITIALIZER;
atomic<bool> detalle{true};
array<string, Servidor::maximoJugadores / 2> resultados;

class BloqueoConsola {
public:
    BloqueoConsola() {
        if (pthread_mutex_lock(&mutexConsola) != 0) { terminate(); }
    }
    ~BloqueoConsola() {
        if (pthread_mutex_unlock(&mutexConsola) != 0) { terminate(); }
    }
    BloqueoConsola(const BloqueoConsola&) = delete;
    BloqueoConsola& operator=(const BloqueoConsola&) = delete;
};

string posicion(Posicion posicion) {
    return "(" + to_string(posicion.fila) + "," + to_string(posicion.columna) + ")";
}

string motivoLegible(MotivoRegistro motivo) {
    switch (motivo) {
        case MotivoRegistro::SeisCapturas: return "seis capturas";
        case MotivoRegistro::PumaAcorralado: return "Puma acorralado";
        case MotivoRegistro::Interrupcion: return "interrupcion por SIGINT";
        case MotivoRegistro::CierreSolicitado: return "cierre solicitado";
        case MotivoRegistro::AbandonoPuma: return "abandono del jugador Puma";
        case MotivoRegistro::AbandonoPerros: return "abandono del jugador Perros";
        default: return nombreMotivoRegistro(motivo);
    }
}
}

string formatearDuracion(int64_t microsegundos) {
    ostringstream texto;
    texto << fixed << setprecision(3) << microsegundos / 1000000.0 << " s";
    return texto.str();
}

string resumirResultadosConsola() {
    BloqueoConsola bloqueo;
    string resumen;
    for (const string& resultado : resultados) {
        if (!resultado.empty()) {
            resumen += resultado + "\n";
        }
    }
    return resumen;
}

void configurarConsola(int jugadores) {
    BloqueoConsola bloqueo;
    resultados.fill("");
    const char* opcion = getenv("KOMIKAN_TABLEROS");
    detalle.store(opcion == nullptr ? jugadores <= 10 : string(opcion) != "0");
}

void imprimirConsola(const string& mensaje, bool error) {
    BloqueoConsola bloqueo;
    (error ? cerr : cout) << mensaje << endl;
}

void presentarEvento(const EventoRegistro& evento, const IdentidadRegistro& identidad,
                     uint64_t secuencia, int64_t tiempoUs) {
    ostringstream mensaje;
    int partida = evento.idPartida < 0 ? identidad.idPartida : evento.idPartida;
    int mesa = evento.idMesa < 0 ? identidad.idMesa : evento.idMesa;
    if (!identidad.esTorneo) {
        bool tablero = evento.tipo == TipoEventoRegistro::Inicio || evento.tipo == TipoEventoRegistro::Movimiento ||
            evento.tipo == TipoEventoRegistro::Captura || evento.tipo == TipoEventoRegistro::Cierre;
        if (!detalle.load() || !tablero) {
            return;
        }
        mensaje << "\n+--- MESA " << mesa << " | PARTIDA " << partida << " | EVENTO " << secuencia
                << " | " << formatearDuracion(tiempoUs) << " ---+\n";
        if (evento.tipo == TipoEventoRegistro::Inicio) {
            mensaje << "Estado inicial | Puma: jugador " << identidad.jugadorPuma
                    << " | Perros: jugador " << identidad.jugadorPerros << '\n';
        } else if (evento.tipo == TipoEventoRegistro::Cierre) {
            mensaje << "Estado final | Resultado: " << (evento.resultado == ResultadoRegistro::Puma ? "Puma" :
                evento.resultado == ResultadoRegistro::Perros ? "Perros" : "sin ganador")
                    << " | " << motivoLegible(evento.motivo) << '\n';
        } else {
            mensaje << (evento.idFicha == 0 ? "Puma" : "Perro " + to_string(evento.idFicha))
                    << ": " << posicion(evento.movimiento.origen) << " -> " << posicion(evento.movimiento.destino);
            if (evento.tipo == TipoEventoRegistro::Captura) {
                mensaje << " | Captura al Perro " << evento.idCapturado << " en " << posicion(evento.movimiento.intermedio);
            }
            mensaje << '\n';
        }
        evento.tablero.imprimirTablero(mensaje);
        mensaje << "+--- FIN MESA " << mesa << " | PARTIDA " << partida << " ---+";
    } else {
        switch (evento.tipo) {
            case TipoEventoRegistro::Inicio: mensaje << "\n=== TORNEO EN CURSO ==="; break;
            case TipoEventoRegistro::LlegadaJugador:
                mensaje << "  Llega jugador " << evento.jugador1; break;
            case TipoEventoRegistro::Emparejamiento:
                mensaje << "  Pareja " << partida << ": Puma = jugador " << evento.jugador1
                        << " | Perros = jugador " << evento.jugador2; break;
            case TipoEventoRegistro::AsignacionMesa:
                mensaje << "  Mesa " << mesa << " recibe partida " << partida; break;
            case TipoEventoRegistro::EsperaMesa:
                mensaje << "  Partida " << partida << " espera una mesa libre"; break;
            case TipoEventoRegistro::LiberacionMesa:
                mensaje << "  Mesa " << mesa << " libre tras partida " << partida; break;
            case TipoEventoRegistro::ResultadoPartida:
                mensaje << "  Partida " << partida << " terminada: ";
                if (evento.resultado == ResultadoRegistro::SinGanador) {
                    mensaje << "sin ganador";
                } else {
                    bool puma = evento.resultado == ResultadoRegistro::Puma;
                    mensaje << "gana jugador " << (puma ? evento.jugador1 : evento.jugador2)
                            << (puma ? " (Puma)" : " (Perros)");
                }
                mensaje << " | " << motivoLegible(evento.motivo); break;
            case TipoEventoRegistro::Cierre:
                mensaje << "\n=== " << (evento.motivo == MotivoRegistro::CierreSolicitado ? "FINALIZACION NORMAL" :
                    evento.motivo == MotivoRegistro::Interrupcion ? "INTERRUPCION POR SIGINT" : "CIERRE POR ERROR") << " ==="; break;
            case TipoEventoRegistro::Error:
                mensaje << "  ERROR: " << motivoLegible(evento.motivo); break;
            default: return;
        }
    }
    if (identidad.esTorneo && evento.tipo == TipoEventoRegistro::ResultadoPartida && partida >= 1 && static_cast<size_t>(partida) <= resultados.size()) {
        BloqueoConsola bloqueo;
        resultados[partida - 1] = mensaje.str();
    }
    imprimirConsola(mensaje.str(), evento.tipo == TipoEventoRegistro::Error);
}
