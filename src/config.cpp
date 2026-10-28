#include "config.h"

#include <array>
#include <charconv>
#include <exception>
#include <fstream>
#include <istream>
#include <random>

using namespace std;

namespace {

string recortarEspacios(const string& texto) {
    size_t inicio = texto.find_first_not_of(" \t\r\n");

    if (inicio == string::npos) {
        return "";
    }

    size_t fin = texto.find_last_not_of(" \t\r\n");
    return texto.substr(inicio, fin - inicio + 1);
}

bool convertirEntero(const string& texto, int& resultado) {
    if (texto.empty()) {
        return false;
    }

    const char* inicio = texto.data();
    const char* fin = inicio + texto.size();
    auto conversion = from_chars(inicio, fin, resultado);
    return conversion.ec == errc() && conversion.ptr == fin;
}

bool convertirSemilla(const string& texto, uint32_t& resultado) {
    if (texto.empty()) {
        return false;
    }

    const char* inicio = texto.data();
    const char* fin = inicio + texto.size();
    auto conversion = from_chars(inicio, fin, resultado);
    return conversion.ec == errc() && conversion.ptr == fin;
}

string nombreFicha(int id) {
    return id == Tablero::puma ? "puma" : "perro" + to_string(id);
}

int obtenerIdFicha(const string& clave) {
    for (int id = Tablero::puma; id <= Tablero::ultimo_perro; id++) {
        if (clave == nombreFicha(id)) {
            return id;
        }
    }

    return -1;
}

bool interpretarPosicion(const string& texto, Posicion& resultado) {
    size_t coma = texto.find(',');

    if (coma == string::npos || texto.find(',', coma + 1) != string::npos) {
        return false;
    }

    return convertirEntero(recortarEspacios(texto.substr(0, coma)), resultado.fila) &&
           convertirEntero(recortarEspacios(texto.substr(coma + 1)), resultado.columna);
}

bool generarSemilla(Config& configuracion, string& error) {
    try {
        random_device fuente;
        uniform_int_distribution<uint32_t> distribucion;
        configuracion.semilla = distribucion(fuente);
        configuracion.semillaAutomatica = true;
        return true;
    } catch (const exception&) {
        error = "No se pudo generar la semilla automatica";
        return false;
    }
}

bool validarDisposicion(const DisposicionInicial& disposicion,
                       const array<size_t, 13>& lineas, string& error) {
    Tablero tablero;

    if (tablero.cargarDisposicion(disposicion)) {
        return true;
    }

    for (int id = Tablero::puma; id <= Tablero::ultimo_perro; id++) {
        const Posicion& posicion = disposicion.posiciones[id];
        string contexto = "Linea " + to_string(lineas[id]) + ": " + nombreFicha(id);

        if (!tablero.esPosicionValida(posicion.fila, posicion.columna)) {
            error = contexto + " utiliza la posicion inexistente (" +
                    to_string(posicion.fila) + "," + to_string(posicion.columna) + ")";
            return false;
        }

        for (int anterior = Tablero::puma; anterior < id; anterior++) {
            const Posicion& otra = disposicion.posiciones[anterior];

            if (posicion.fila == otra.fila && posicion.columna == otra.columna) {
                error = contexto + " comparte posicion con " + nombreFicha(anterior) +
                        " (linea " + to_string(lineas[anterior]) + ")";
                return false;
            }
        }
    }

    error = "Disposicion inicial invalida";
    return false;
}

}

bool crearConfiguracionPredeterminada(Config& resultado, string& error) {
    Config configuracion;
    configuracion.disposicionInicial.posiciones = {{{4, 2}, {4, 0}, {4, 4},
        {5, 0}, {5, 1}, {5, 2}, {5, 3}, {5, 4},
        {6, 0}, {6, 1}, {6, 2}, {6, 3}, {6, 4}}};
    array<size_t, 13> lineas{};

    if (!validarDisposicion(configuracion.disposicionInicial, lineas, error) ||
        !generarSemilla(configuracion, error)) {
        return false;
    }

    resultado = configuracion;
    error.clear();
    return true;
}

bool cargarConfiguracion(const string& ruta, Config& resultado, string& error) {
    ifstream archivo(ruta);

    if (!archivo.is_open()) {
        error = ruta + ": no se pudo abrir el archivo de configuracion";
        return false;
    }

    if (!leerConfiguracion(archivo, resultado, error)) {
        error = ruta + ": " + error;
        return false;
    }

    return true;
}

bool leerConfiguracion(istream& entrada, Config& resultado, string& error) {
    constexpr int campoRetardoMinimo = Tablero::ultimo_perro + 1;
    constexpr int campoRetardoMaximo = campoRetardoMinimo + 1;
    constexpr int campoSemilla = campoRetardoMaximo + 1;
    constexpr int cantidadCampos = campoSemilla + 1;

    Config configuracion;
    array<size_t, cantidadCampos> lineasCampos{};
    string linea;
    size_t numeroLinea = 0;

    try {
        while (getline(entrada, linea)) {
            numeroLinea++;
            size_t comentario = linea.find('#');

            if (comentario != string::npos) {
                linea.erase(comentario);
            }

            linea = recortarEspacios(linea);

            if (linea.empty()) {
                continue;
            }

            string contexto = "Linea " + to_string(numeroLinea) + ": ";
            size_t igual = linea.find('=');

            if (igual == string::npos || linea.find('=', igual + 1) != string::npos) {
                error = contexto + "se esperaba una asignacion clave = valor";
                return false;
            }

            string clave = recortarEspacios(linea.substr(0, igual));
            string valor = recortarEspacios(linea.substr(igual + 1));
            int id = obtenerIdFicha(clave);
            int campo = id;

            if (clave == "retardoMinimo") {
                campo = campoRetardoMinimo;
            } else if (clave == "retardoMaximo") {
                campo = campoRetardoMaximo;
            } else if (clave == "semilla") {
                campo = campoSemilla;
            } else if (id < 0) {
                error = contexto + "clave desconocida: " + clave;
                return false;
            }

            if (lineasCampos[campo] != 0) {
                error = contexto + clave + " esta repetido; primera aparicion en linea " +
                        to_string(lineasCampos[campo]);
                return false;
            }

            lineasCampos[campo] = numeroLinea;
            bool valido;

            if (id >= 0) {
                valido = interpretarPosicion(valor, configuracion.disposicionInicial.posiciones[id]);
            } else if (campo == campoSemilla) {
                valido = convertirSemilla(valor, configuracion.semilla);
            } else {
                int& retardo = campo == campoRetardoMinimo ? configuracion.retardoMinimo : configuracion.retardoMaximo;
                valido = convertirEntero(valor, retardo) && retardo >= 0;
            }

            if (!valido) {
                error = contexto + "valor invalido para " + clave + ": " + valor;
                return false;
            }
        }
    } catch (const ios_base::failure&) {
        error = "Error de lectura del archivo de configuracion";
        return false;
    }

    if (entrada.bad() || !entrada.eof()) {
        error = "Error de lectura del archivo de configuracion";
        return false;
    }

    for (int campo = 0; campo < campoSemilla; campo++) {
        if (lineasCampos[campo] == 0) {
            string clave = campo <= Tablero::ultimo_perro ? nombreFicha(campo) :
                           (campo == campoRetardoMinimo ? "retardoMinimo" : "retardoMaximo");
            error = "Falta el campo obligatorio " + clave;
            return false;
        }
    }

    if (configuracion.retardoMinimo > configuracion.retardoMaximo) {
        error = "retardoMinimo (linea " + to_string(lineasCampos[campoRetardoMinimo]) +
                ") supera retardoMaximo (linea " + to_string(lineasCampos[campoRetardoMaximo]) + ")";
        return false;
    }

    array<size_t, 13> lineasFichas{};

    for (int id = Tablero::puma; id <= Tablero::ultimo_perro; id++) {
        lineasFichas[id] = lineasCampos[id];
    }

    if (!validarDisposicion(configuracion.disposicionInicial, lineasFichas, error)) {
        return false;
    }

    if (lineasCampos[campoSemilla] == 0 && !generarSemilla(configuracion, error)) {
        return false;
    }

    resultado = configuracion;
    error.clear();
    return true;
}
