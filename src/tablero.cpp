#include "tablero.h"

#include <ostream>
#include <stdexcept>
#include <string>

using namespace std;

const Tablero::ListaAdyacencia Tablero::adyacencias = Tablero::crearAdyacencias();
const vector<vector<Posicion>> Tablero::lineasCaptura = Tablero::crearLineasCaptura();

Tablero::Tablero() {
    inicializarTablero();
}

bool Tablero::cargarDisposicion(const DisposicionInicial& disposicion) {
    for (int fila = 0; fila < filas; fila++) {
        for (int columna = 0; columna < columnas; columna++) {
            if (esPosicionValida(fila, columna) && posiciones[fila][columna] != vacia) {
                return false;
            }
        }
    }

    bool ocupadas[filas][columnas]{};

    for (const Posicion& posicion : disposicion.posiciones) {
        if (!esPosicionValida(posicion.fila, posicion.columna)) {
            return false;
        }

        if (ocupadas[posicion.fila][posicion.columna]) {
            return false;
        }

        ocupadas[posicion.fila][posicion.columna] = true;
    }

    for (int id = puma; id <= ultimo_perro; id++) {
        const Posicion& posicion = disposicion.posiciones[id];
        posiciones[posicion.fila][posicion.columna] = id;
    }

    return true;
}

bool Tablero::buscarFicha(int id, Posicion& resultado) const {
    if (id < puma || id > ultimo_perro) {
        return false;
    }

    for (int fila = 0; fila < filas; fila++) {
        for (int columna = 0; columna < columnas; columna++) {
            if (posiciones[fila][columna] == id) {
                resultado = {fila, columna};
                return true;
            }
        }
    }

    return false;
}

bool Tablero::moverFicha(int id, Posicion origen, Posicion destino) {
    if (id < puma || id > ultimo_perro) {
        return false;
    }

    if (!esPosicionValida(origen.fila, origen.columna) ||
        !esPosicionValida(destino.fila, destino.columna)) {
        return false;
    }

    if (posiciones[origen.fila][origen.columna] != id ||
        !sonAdyacentes(origen, destino) ||
        posiciones[destino.fila][destino.columna] != vacia) {
        return false;
    }

    posiciones[origen.fila][origen.columna] = vacia;
    posiciones[destino.fila][destino.columna] = id;
    return true;
}

bool Tablero::capturarPerro(Posicion origen, Posicion intermedio, Posicion destino, int& idCapturado) {
    if (!esPosicionValida(origen.fila, origen.columna) ||
        !esPosicionValida(intermedio.fila, intermedio.columna) ||
        !esPosicionValida(destino.fila, destino.columna)) {
        return false;
    }

    if (posiciones[origen.fila][origen.columna] != puma ||
        !esTrayectoriaCaptura(origen, intermedio, destino)) {
        return false;
    }

    int perro = posiciones[intermedio.fila][intermedio.columna];

    if (perro < primer_perro || perro > ultimo_perro ||
        posiciones[destino.fila][destino.columna] != vacia) {
        return false;
    }

    posiciones[origen.fila][origen.columna] = vacia;
    posiciones[intermedio.fila][intermedio.columna] = vacia;
    posiciones[destino.fila][destino.columna] = puma;
    idCapturado = perro;
    return true;
}

bool Tablero::esTrayectoriaCaptura(Posicion origen, Posicion intermedio, Posicion destino) const {
    if (!sonAdyacentes(origen, intermedio) || !sonAdyacentes(intermedio, destino)) {
        return false;
    }

    auto mismaPosicion = [](Posicion primera, Posicion segunda) {
        return primera.fila == segunda.fila && primera.columna == segunda.columna;
    };

    for (const vector<Posicion>& linea : lineasCaptura) {
        for (size_t inicio = 0; inicio + 2 < linea.size(); inicio++) {
            if (!mismaPosicion(intermedio, linea[inicio + 1])) {
                continue;
            }

            if ((mismaPosicion(origen, linea[inicio]) && mismaPosicion(destino, linea[inicio + 2])) ||
                (mismaPosicion(destino, linea[inicio]) && mismaPosicion(origen, linea[inicio + 2]))) {
                return true;
            }
        }
    }

    return false;
}

vector<vector<Posicion>> Tablero::crearLineasCaptura() {
    vector<vector<Posicion>> lineas;

    for (int fila = 2; fila < filas; fila++) {
        vector<Posicion> linea;

        for (int columna = 0; columna < columnas; columna++) {
            linea.push_back({fila, columna});
        }

        lineas.push_back(linea);
    }

    for (int columna = 0; columna < columnas; columna++) {
        vector<Posicion> linea;

        if (columna == 1 || columna == 3) {
            linea.push_back({1, columna});
        } else if (columna == 2) {
            linea.push_back({0, 2});
        }

        for (int fila = 2; fila < filas; fila++) {
            linea.push_back({fila, columna});
        }

        lineas.push_back(linea);
    }

    lineas.push_back({{0, 2}, {1, 1}, {2, 0}});
    lineas.push_back({{0, 2}, {1, 3}, {2, 4}});
    return lineas;
}

void Tablero::inicializarTablero() {
    for (int fila = 0; fila < filas; fila++) {
        for (int columna = 0; columna < columnas; columna++) {
            if (esPosicionValida(fila, columna)) {
                posiciones[fila][columna] = vacia;
            } else {
                posiciones[fila][columna] = inexistente;
            }
        }
    }
}

bool Tablero::esPosicionValida(int fila, int columna) const {
    if (fila < 0 || fila >= filas || columna < 0 || columna >= columnas) {
        return false;
    }

    if (fila >= 2) {
        return true;
    }

    if (fila == 0) {
        return columna == 2;
    }

    return fila == 1 && (columna == 1 || columna == 3);
}

int Tablero::obtenerOcupacion(int fila, int columna) const {
    if (!esPosicionValida(fila, columna)) {
        throw out_of_range("La posicion consultada no existe en el tablero");
    }

    return posiciones[fila][columna];
}

const vector<Posicion>& Tablero::obtenerVecinos(int fila, int columna) const {
    if (!esPosicionValida(fila, columna)) {
        throw out_of_range("La posicion consultada no existe en el tablero");
    }

    return adyacencias[fila][columna];
}

bool Tablero::sonAdyacentes(Posicion origen, Posicion destino) const {
    if (!esPosicionValida(origen.fila, origen.columna) ||
        !esPosicionValida(destino.fila, destino.columna)) {
        return false;
    }

    for (const Posicion& vecino : obtenerVecinos(origen.fila, origen.columna)) {
        if (vecino.fila == destino.fila && vecino.columna == destino.columna) {
            return true;
        }
    }

    return false;
}

void Tablero::conectar(ListaAdyacencia& conexiones, Posicion origen, Posicion destino) {
    conexiones[origen.fila][origen.columna].push_back(destino);
    conexiones[destino.fila][destino.columna].push_back(origen);
}

Tablero::ListaAdyacencia Tablero::crearAdyacencias() {
    ListaAdyacencia conexiones{};

    for (int fila = 2; fila < filas; fila++) {
        for (int columna = 0; columna < columnas; columna++) {
            if (columna + 1 < columnas) {
                conectar(conexiones, {fila, columna}, {fila, columna + 1});
            }
            if (fila + 1 < filas) {
                conectar(conexiones, {fila, columna}, {fila + 1, columna});
            }
        }
    }

    conectar(conexiones, {0, 2}, {1, 1});
    conectar(conexiones, {0, 2}, {1, 3});
    conectar(conexiones, {1, 1}, {2, 0});
    conectar(conexiones, {1, 3}, {2, 4});
    conectar(conexiones, {1, 1}, {1, 3});
    conectar(conexiones, {0, 2}, {2, 2});
    conectar(conexiones, {1, 1}, {2, 1});
    conectar(conexiones, {1, 3}, {2, 3});

    return conexiones;
}

void Tablero::imprimirTablero(ostream& salida) const {
    vector<string> dibujo = {
        "0              [  .]",
        "             /// | \\\\\\",
        "1        [  .]---+---[  .]",
        "       /// |     |     | \\\\\\",
        "2  [  .]-[  .]-[  .]-[  .]-[  .]",
        "     |     |     |     |     |",
        "3  [  .]-[  .]-[  .]-[  .]-[  .]",
        "     |     |     |     |     |",
        "4  [  .]-[  .]-[  .]-[  .]-[  .]",
        "     |     |     |     |     |",
        "5  [  .]-[  .]-[  .]-[  .]-[  .]",
        "     |     |     |     |     |",
        "6  [  .]-[  .]-[  .]-[  .]-[  .]"
    };

    for (int fila = 0; fila < filas; fila++) {
        for (int columna = 0; columna < columnas; columna++) {
            if (!esPosicionValida(fila, columna)) {
                continue;
            }

            int ocupacion = obtenerOcupacion(fila, columna);
            string ficha;

            if (ocupacion == vacia) {
                ficha = ".";
            } else if (ocupacion == puma) {
                ficha = "P";
            } else {
                ficha = to_string(ocupacion);
            }

            ficha = "[" + string(3 - ficha.size(), ' ') + ficha + "]";
            dibujo[fila * 2].replace(3 + columna * 6, 5, ficha);
        }
    }

    salida << "TABLERO KOMIKAN\n";
    salida << "     0     1     2     3     4\n";

    for (const string& linea : dibujo) {
        salida << linea << '\n';
    }

    salida << "\nPosiciones especiales: T=(0,2), SI=(1,1), SD=(1,3)\n";
    salida << "[.] : casilla vacia\n";
    salida << "[P] : Puma (0)\n";
    salida << "[1] a [12] : Perros\n";
    salida << "+ : cruce de lineas sin posicion\n";
}
