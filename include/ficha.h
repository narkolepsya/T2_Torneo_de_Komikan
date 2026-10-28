#ifndef FICHA_H
#define FICHA_H

#include <random>

using namespace std;

enum TipoFicha {
    Puma,
    Perro
};

struct Ficha {
    int id;
    TipoFicha tipo;
};

bool esFichaValida(const Ficha& ficha);
Ficha crearFicha(int id);
int generarRetardo(mt19937& generador, int minimoMs = 50, int maximoMs = 300);
void esperarRetardo(int milisegundos);

#endif

