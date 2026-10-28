#include "ficha.h"
#include "tablero.h"

#include <chrono>
#include <stdexcept>
#include <thread>

using namespace std;

bool esFichaValida(const Ficha& ficha) {
    if (ficha.id == Tablero::puma) {
        return ficha.tipo == Puma;
    }

    return ficha.id >= Tablero::primer_perro &&
           ficha.id <= Tablero::ultimo_perro &&
           ficha.tipo == Perro;
}

Ficha crearFicha(int id) {
    if (id < Tablero::puma || id > Tablero::ultimo_perro) {
        throw invalid_argument("Identificador de ficha fuera de rango");
    }

    Ficha ficha;
    ficha.id = id;

    if (id == Tablero::puma) {
        ficha.tipo = Puma;
    } else {
        ficha.tipo = Perro;
    }

    return ficha;
}

int generarRetardo(mt19937& generador, int minimoMs, int maximoMs) {
    if (minimoMs < 0 || maximoMs < minimoMs) {
        throw invalid_argument("Rango de retardo invalido");
    }

    uniform_int_distribution<int> distribucion(minimoMs, maximoMs);
    return distribucion(generador);
}

void esperarRetardo(int milisegundos) {
    if (milisegundos < 0) {
        throw invalid_argument("El retardo no puede ser negativo");
    }

    this_thread::sleep_for(chrono::milliseconds(milisegundos));
}