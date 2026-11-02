#ifndef TABLERO_H
#define TABLERO_H

#include <array>
#include <iosfwd>
#include <vector>

using namespace std;

struct Posicion {
    int fila;
    int columna;
};

struct DisposicionInicial {
    array<Posicion, 13> posiciones{};
};

class Tablero {
public:
    static constexpr int filas = 7;
    static constexpr int columnas = 5;
    static constexpr int inexistente = -2;
    static constexpr int vacia = -1;
    static constexpr int puma = 0;
    static constexpr int primer_perro = 1;
    static constexpr int ultimo_perro = 12;

    Tablero();

    bool cargarDisposicion(const DisposicionInicial& disposicion);
    bool buscarFicha(int id, Posicion& resultado) const;
    bool moverFicha(int id, Posicion origen, Posicion destino);
    bool capturarPerro(Posicion origen, Posicion intermedio, Posicion destino, int& idCapturado);

    bool esPosicionValida(int fila, int columna) const;
    bool sonAdyacentes(Posicion origen, Posicion destino) const;

    int obtenerOcupacion(int fila, int columna) const;
    const vector<Posicion>& obtenerVecinos(int fila, int columna) const;
    void imprimirTablero(ostream& salida) const;

private:
    using ListaAdyacencia = array<array<vector<Posicion>, columnas>, filas>;
    int posiciones[filas][columnas];

    static const ListaAdyacencia adyacencias;
    static const vector<vector<Posicion>> lineasCaptura;
    static vector<vector<Posicion>> crearLineasCaptura();
    bool esTrayectoriaCaptura(Posicion origen, Posicion intermedio, Posicion destino) const;
    static ListaAdyacencia crearAdyacencias();
    static void conectar(ListaAdyacencia& conexiones, Posicion origen,
                         Posicion destino);
    void inicializarTablero();
};

#endif
