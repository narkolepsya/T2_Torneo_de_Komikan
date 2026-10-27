#include <iostream>
#include <string>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        cerr << "Uso: ./torneo N M [config.txt]" << endl;
        return 1;
    }

    int jugadores, mesas;

    try {
        jugadores = stoi(argv[1]);
        mesas = stoi(argv[2]);
    } catch (...) {
        cerr << "Error: N y M deben ser numeros enteros." << endl;
        return 1;
    }

    if (jugadores < 2 || mesas < 1) {
        cerr << "Error: N debe ser al menos 2 y M debe ser mayor que 0." << endl;
        return 1;
    }

    cout << "TORNEO DE KOMIKAN" << endl;
    cout << "Jugadores: " << jugadores << endl;
    cout << "Mesas disponibles: " << mesas << endl;

    if (argc == 4) {
        cout << "Archivo de configuracion: " << argv[3] << endl;
    }

    return 0;
}