#ifndef CONSOLA_H
#define CONSOLA_H

#include "registro.h"
#include <string>

using namespace std;

string resumirResultadosConsola();
string formatearDuracion(int64_t microsegundos);
void configurarConsola(int cantidadJugadores);
void imprimirConsola(const string& mensaje, bool error = false);
void presentarEvento(const EventoRegistro& evento, const IdentidadRegistro& identidad,
                     uint64_t secuencia, int64_t tiempoUs);

#endif
