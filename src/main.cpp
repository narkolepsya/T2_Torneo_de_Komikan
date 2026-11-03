#include "servidor.h"
#include "consola.h"
#include <sstream>
#include <chrono>

#include <charconv>
#include <iostream>
#include <string>

using namespace std;

bool leerEnteroPositivo(const string& texto, int& resultado) {
    int valor = 0;
    auto conversion = from_chars(texto.data(), texto.data() + texto.size(), valor);
    if (texto.empty() || conversion.ec != errc() || conversion.ptr != texto.data() + texto.size() || valor <= 0) {
        return false;
    }
    resultado = valor;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        imprimirConsola("Uso: ./torneo N M [config.txt]", true);
        return 1;
    }
    int jugadores = 0;
    int mesas = 0;
    if (!leerEnteroPositivo(argv[1], jugadores) || !leerEnteroPositivo(argv[2], mesas) ||
        jugadores < 2 || jugadores % 2 != 0 || jugadores > Servidor::maximoJugadores) {
        imprimirConsola("Error: N debe ser par entre 2 y " + to_string(Servidor::maximoJugadores) +
            "; M debe ser un entero positivo hasta 2147483647, sin sufijos.", true);
        return 1;
    }
    Config configuracion;
    string error;
    bool valida = argc == 4 ? cargarConfiguracion(argv[3], configuracion, error) :
        crearConfiguracionPredeterminada(configuracion, error);
    if (!valida) {
        imprimirConsola("Error de configuracion: " + error, true);
        return 1;
    }
    configurarConsola(jugadores);
    ostringstream encabezado;
    encabezado << "\n================ TORNEO DE KOMIKAN ================\nJugadores: " << jugadores << "\nMesas disponibles: " << mesas << "\nPartidas previstas: " << jugadores / 2
         << "\nRetardos: " << configuracion.retardoMinimo << ".." << configuracion.retardoMaximo
         << " ms\nSemilla efectiva: " << configuracion.semilla;
    imprimirConsola(encabezado.str());
    auto inicio = chrono::steady_clock::now();
    Servidor servidor;
    bool correcto = servidor.ejecutar(jugadores, mesas, configuracion, error);
    if (!correcto) {
        imprimirConsola("Error del torneo: " + error, true);
    }
    EstadoTorneo estado;
    servidor.obtenerEstado(estado);
    ostringstream resumen;
    resumen << "\n" << (!correcto ? "CIERRE POR ERROR" : estado.interrumpido ? "INTERRUMPIDO POR SIGINT" : "FINALIZADO NORMALMENTE") << "\nRESUMEN DEL TORNEO\nPartidas iniciadas: " << estado.partidasIniciadas
         << " | terminadas: " << estado.partidasTerminadas
         << "; maximo de mesas activas: " << estado.maxMesasActivas
         << "; parejas que esperaron: " << estado.parejasEsperaron
         << "\nDuracion total: " << formatearDuracion(chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - inicio).count());
    resumen << "\n" << resumirResultadosConsola();
    imprimirConsola(resumen.str());
    return !correcto ? 1 : estado.interrumpido ? 130 : 0;
}
