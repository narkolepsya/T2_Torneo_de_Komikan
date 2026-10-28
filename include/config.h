#ifndef CONFIG_H
#define CONFIG_H

#include "tablero.h"

#include <cstdint>
#include <iosfwd>
#include <string>

using namespace std;

struct Config {
    DisposicionInicial disposicionInicial;
    int retardoMinimo = 50;
    int retardoMaximo = 300;
    uint32_t semilla = 0;
    bool semillaAutomatica = false;
};

bool crearConfiguracionPredeterminada(Config& resultado, string& error);
bool cargarConfiguracion(const string& ruta, Config& resultado, string& error);
bool leerConfiguracion(istream& entrada, Config& resultado, string& error);

#endif
