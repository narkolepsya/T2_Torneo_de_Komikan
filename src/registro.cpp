#include "registro.h"
#include "consola.h"

#include <cerrno>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace std;

namespace {


void comprobarRegistro(int codigo) {
    if (codigo != 0) {
        terminate();
    }
}

void informar(const string& texto, bool error) {
    imprimirConsola(texto, error);
}

string fechaUtc(int64_t milisegundos) {
    time_t segundos = milisegundos / 1000;
    tm fecha{};
    gmtime_r(&segundos, &fecha);
    char texto[32];
    strftime(texto, sizeof(texto), "%Y-%m-%dT%H:%M:%S", &fecha);
    ostringstream salida;
    salida << texto << '.' << setw(3) << setfill('0') << milisegundos % 1000 << 'Z';
    return salida.str();
}

const char* nombreTipo(TipoEventoRegistro tipo) {
    switch (tipo) {
        case TipoEventoRegistro::Inicio: return "inicio";
        case TipoEventoRegistro::Movimiento: return "movimiento";
        case TipoEventoRegistro::Captura: return "captura";
        case TipoEventoRegistro::Rechazo: return "rechazo";
        case TipoEventoRegistro::Victoria: return "victoria";
        case TipoEventoRegistro::Cierre: return "cierre";
        case TipoEventoRegistro::Error: return "error";
        case TipoEventoRegistro::LlegadaJugador: return "llegada_jugador";
        case TipoEventoRegistro::Emparejamiento: return "emparejamiento";
        case TipoEventoRegistro::AsignacionMesa: return "asignacion_mesa";
        case TipoEventoRegistro::LiberacionMesa: return "liberacion_mesa";
        case TipoEventoRegistro::ResultadoPartida: return "resultado_partida";
        case TipoEventoRegistro::EsperaMesa: return "espera_mesa";
    }
    return "desconocido";
}

const char* nombreMotivo(MotivoRegistro motivo) {
    switch (motivo) {
        case MotivoRegistro::Ninguno: return "ninguno";
        case MotivoRegistro::Confirmado: return "confirmado";
        case MotivoRegistro::OrigenObsoleto: return "origen_obsoleto";
        case MotivoRegistro::PosicionInexistente: return "posicion_inexistente";
        case MotivoRegistro::DestinoOcupado: return "destino_ocupado";
        case MotivoRegistro::NoAdyacente: return "no_adyacente";
        case MotivoRegistro::CapturaSoloPuma: return "captura_solo_puma";
        case MotivoRegistro::IntermedioSinPerro: return "intermedio_sin_perro";
        case MotivoRegistro::TrayectoriaInvalida: return "trayectoria_invalida";
        case MotivoRegistro::TipoInvalido: return "tipo_invalido";
        case MotivoRegistro::SeisCapturas: return "seis_capturas";
        case MotivoRegistro::PumaAcorralado: return "puma_acorralado";
        case MotivoRegistro::CierreSolicitado: return "cierre_solicitado";
        case MotivoRegistro::Interrupcion: return "sigint";
        case MotivoRegistro::ErrorPreparacion: return "error_preparacion";
        case MotivoRegistro::ErrorCreacionHilo: return "error_creacion_hilo";
        case MotivoRegistro::ErrorEjecucion: return "error_ejecucion";
        case MotivoRegistro::ErrorRegistro: return "error_registro";
        case MotivoRegistro::AbandonoPuma: return "abandono_puma";
        case MotivoRegistro::AbandonoPerros: return "abandono_perros";
        case MotivoRegistro::FalloJugadorPuma: return "fallo_jugador_puma";
        case MotivoRegistro::FalloJugadorPerros: return "fallo_jugador_perros";
    }
    return "desconocido";
}

const char* nombreResultado(ResultadoRegistro resultado) {
    switch (resultado) {
        case ResultadoRegistro::SinGanador: return "sin_ganador";
        case ResultadoRegistro::Puma: return "puma";
        case ResultadoRegistro::Perros: return "perros";
    }
    return "desconocido";
}

string posiciones(const Tablero& tablero, bool ocupaciones) {
    ostringstream salida;
    bool primera = true;

    for (int fila = 0; fila < Tablero::filas; fila++) {
        for (int columna = 0; columna < Tablero::columnas; columna++) {
            if (!tablero.esPosicionValida(fila, columna)) {
                continue;
            }
            if (!primera) {
                salida << ';';
            }
            primera = false;
            if (ocupaciones) {
                salida << tablero.obtenerOcupacion(fila, columna);
            } else {
                salida << fila << ',' << columna;
            }
        }
    }
    return salida.str();
}

}

const char* nombreMotivoRegistro(MotivoRegistro motivo) {
    return nombreMotivo(motivo);
}

TiempoRegistro tomarTiempoRegistro() {
    return {chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count(),
            chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now().time_since_epoch()).count()};
}

bool crearEjecucionRegistro(const string& base, EjecucionRegistro& resultado, string& error) {
    try {
        filesystem::create_directories(base);
        string patron = (filesystem::path(base) / "ejecucion_XXXXXX").string();
        vector<char> nombre(patron.begin(), patron.end());
        nombre.push_back('\0');
        char* creado = mkdtemp(nombre.data());
        if (creado == nullptr) {
            error = "No se pudo crear un directorio unico de registros: " + string(strerror(errno));
            return false;
        }
        EjecucionRegistro ejecucion;
        ejecucion.directorio = filesystem::absolute(creado).string();
        ejecucion.idTorneo = filesystem::path(creado).filename().string();
        ejecucion.inicio = tomarTiempoRegistro();
        resultado = ejecucion;
        error.clear();
        return true;
    } catch (const exception& fallo) {
        error = "No se pudo preparar registros: " + string(fallo.what());
        return false;
    }
}

RegistroEventos::~RegistroEventos() {
    if (inicializado) {
        string error;
        if (!cerrar(error) && hiloCreado) {
            terminate();
        }
        liberarRecursos();
    }
}

bool RegistroEventos::abrir(const OpcionesRegistro& opciones, const Config& datos, string& error,
                           void (*notificar)(void*), void* contexto) {
    if (hiloCreado) {
        error = "El registro anterior requiere cierre";
        return false;
    }
    liberarRecursos();
    if (opciones.capacidad < 3 || opciones.capacidad > 4096 ||
        opciones.identidad.idPartida < 0 || opciones.identidad.idMesa < 0 ||
        opciones.identidad.jugadorPuma < 0 || opciones.identidad.jugadorPerros < 0 ||
        opciones.identidad.jugadorPuma == opciones.identidad.jugadorPerros) {
        error = "Identidad o capacidad de registro invalida (capacidad 3..4096)";
        return false;
    }
    identidad = opciones.identidad;
    configuracion = datos;
    avisarError = notificar;
    contextoError = contexto;
    if (identidad.ejecucion.directorio.empty() &&
        !crearEjecucionRegistro("registros", identidad.ejecucion, error)) {
        informar(error, true);
        return false;
    }
    bool idValido = !identidad.ejecucion.idTorneo.empty() && identidad.ejecucion.idTorneo.size() <= 80 &&
        all_of(identidad.ejecucion.idTorneo.begin(), identidad.ejecucion.idTorneo.end(), [](char caracter) {
            return (caracter >= 'a' && caracter <= 'z') || (caracter >= 'A' && caracter <= 'Z') ||
                   (caracter >= '0' && caracter <= '9') || caracter == '_' || caracter == '-';
        });
    if (!idValido || identidad.ejecucion.inicio.monotonoNs <= 0 ||
        identidad.ejecucion.inicio.monotonoNs > tomarTiempoRegistro().monotonoNs) {
        error = "Contexto de ejecucion de registro incompleto";
        return false;
    }
    try {
        cola.assign(opciones.capacidad, EventoRegistro());
    } catch (const exception&) {
        error = "No se pudo reservar memoria para la cola de registros";
        return false;
    }
    int codigo = pthread_mutex_init(&mutex, nullptr);
    if (codigo != 0) {
        error = "No se pudo inicializar mutex de registro";
        return false;
    }
    codigo = pthread_cond_init(&hayEventos, nullptr);
    if (codigo != 0) {
        comprobarRegistro(pthread_mutex_destroy(&mutex));
        error = "No se pudo inicializar condicion de eventos";
        return false;
    }
    codigo = pthread_cond_init(&hayCapacidad, nullptr);
    if (codigo != 0) {
        comprobarRegistro(pthread_cond_destroy(&hayEventos));
        comprobarRegistro(pthread_mutex_destroy(&mutex));
        error = "No se pudo inicializar condicion de capacidad";
        return false;
    }
    inicializado = true;
    primero = 0;
    estado = EstadoRegistro();
    cerrando = false;
    cierrePublicado = false;
    anunciado = false;
    ultimoError.clear();
    inicio = identidad.esTorneo ? identidad.ejecucion.inicio : tomarTiempoRegistro();
    string nombre = identidad.esTorneo ? "torneo.txt" :
        "partida_" + to_string(identidad.idPartida) + ".txt";
    ruta = (filesystem::path(identidad.ejecucion.directorio) / nombre).string();
    descriptor = open(ruta.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        error = "No se pudo abrir " + ruta + ": " + strerror(errno);
        informar(error, true);
        liberarRecursos();
        return false;
    }
    Tablero geometria;
    ostringstream cabecera;
    cabecera << "KOMIKAN\tversion=1\tambito=" << (identidad.esTorneo ? "torneo" : "partida")
             << "\ttorneo=" << identidad.ejecucion.idTorneo << '\n';
    cabecera << "IDENTIDAD\tpartida=" << identidad.idPartida << "\tmesa=" << identidad.idMesa
             << "\tjugador_puma=" << identidad.jugadorPuma << "\tjugador_perros=" << identidad.jugadorPerros;
    if (identidad.esTorneo) {
        cabecera << "\tjugadores=" << identidad.cantidadJugadores << "\tmesas=" << identidad.cantidadMesas;
    }
    cabecera << '\n';
    cabecera << "TIEMPO\tinicio_utc=" << fechaUtc(inicio.realMs)
             << "\tinicio_torneo_utc=" << fechaUtc(identidad.ejecucion.inicio.realMs) << '\n';
    cabecera << "CONFIG\tretardo_min_ms=" << configuracion.retardoMinimo
             << "\tretardo_max_ms=" << configuracion.retardoMaximo << "\tsemilla=" << configuracion.semilla
             << "\tsemilla_automatica=" << configuracion.semillaAutomatica << '\n';
    cabecera << "DISPOSICION";
    for (int id = 0; id <= 12; id++) {
        Posicion posicion = configuracion.disposicionInicial.posiciones[id];
        cabecera << '\t' << id << '=' << posicion.fila << ',' << posicion.columna;
    }
    cabecera << "\nGEOMETRIA\tposiciones=" << posiciones(geometria, false) << '\n';
    cabecera << "CONEXIONES\taristas=";
    bool primeraArista = true;
    for (int fila = 0; fila < Tablero::filas; fila++) {
        for (int columna = 0; columna < Tablero::columnas; columna++) {
            if (!geometria.esPosicionValida(fila, columna)) {
                continue;
            }
            for (Posicion vecino : geometria.obtenerVecinos(fila, columna)) {
                if (fila * Tablero::columnas + columna < vecino.fila * Tablero::columnas + vecino.columna) {
                    if (!primeraArista) {
                        cabecera << ';';
                    }
                    primeraArista = false;
                    cabecera << fila << ',' << columna << '>' << vecino.fila << ',' << vecino.columna;
                }
            }
        }
    }
    cabecera << '\n';
    cabecera << "SIMBOLOS\tvacia=-1\tpuma=0\tperros=1..12\n";
    if (!escribirTexto(cabecera.str())) {
        error = ultimoError;
        liberarRecursos();
        return false;
    }
    codigo = pthread_create(&hilo, nullptr, ejecutar, this);
    if (codigo != 0) {
        error = "No se pudo crear el escritor de " + ruta + ": " + strerror(codigo);
        informar(error, true);
        liberarRecursos();
        return false;
    }
    hiloCreado = true;
    informar("Registro creado: " + (identidad.esTorneo ? ruta : filesystem::path(ruta).filename().string()), false);
    error.clear();
    return true;
}

bool RegistroEventos::reservar(size_t cantidad, string& error) {
    if (!inicializado || cantidad == 0 || cantidad > cola.size()) {
        error = "Reserva de registro invalida";
        return false;
    }
    comprobarRegistro(pthread_mutex_lock(&mutex));
    while (!estado.fallo && !cerrando &&
           (estado.reservados != 0 || estado.enCola + cantidad > cola.size())) {
        estado.productoresEsperando++;
        comprobarRegistro(pthread_cond_wait(&hayCapacidad, &mutex));
        estado.productoresEsperando--;
    }
    bool disponible = !estado.fallo && !cerrando;
    if (disponible) {
        estado.reservados += cantidad;
        error.clear();
    } else {
        error = estado.fallo ? ultimoError : "El registro ya esta cerrando";
    }
    comprobarRegistro(pthread_mutex_unlock(&mutex));
    return disponible;
}

bool RegistroEventos::publicar(const EventoRegistro* eventos, size_t cantidad, size_t reserva, string& error) {
    if (!inicializado || (eventos == nullptr && cantidad != 0)) {
        error = "Registro no inicializado o lote invalido";
        return false;
    }
    comprobarRegistro(pthread_mutex_lock(&mutex));
    if (estado.fallo || cerrando || cantidad > reserva || reserva > estado.reservados) {
        error = estado.fallo ? ultimoError : "Publicacion sin reserva valida";
        comprobarRegistro(pthread_mutex_unlock(&mutex));
        return false;
    }
    for (size_t indice = 0; indice < cantidad; indice++) {
        cola[(primero + estado.enCola) % cola.size()] = eventos[indice];
        estado.enCola++;
        if (eventos[indice].tipo == TipoEventoRegistro::Cierre) {
            cierrePublicado = true;
        }
    }
    estado.reservados -= reserva;
    if (cierrePublicado) {
        cerrando = true;
    }
    comprobarRegistro(pthread_cond_signal(&hayEventos));
    comprobarRegistro(pthread_cond_broadcast(&hayCapacidad));
    comprobarRegistro(pthread_mutex_unlock(&mutex));
    error.clear();
    return true;
}

void RegistroEventos::cancelarReserva(size_t cantidad) {
    if (!inicializado) {
        return;
    }
    comprobarRegistro(pthread_mutex_lock(&mutex));
    if (cantidad <= estado.reservados) {
        estado.reservados -= cantidad;
    }
    comprobarRegistro(pthread_cond_broadcast(&hayCapacidad));
    comprobarRegistro(pthread_mutex_unlock(&mutex));
}

bool RegistroEventos::cerrar(string& error) {
    if (!inicializado) {
        error = "Registro no inicializado";
        return false;
    }
    comprobarRegistro(pthread_mutex_lock(&mutex));
    bool reservaPendiente = estado.reservados != 0;
    cerrando = true;
    comprobarRegistro(pthread_cond_broadcast(&hayEventos));
    comprobarRegistro(pthread_cond_broadcast(&hayCapacidad));
    comprobarRegistro(pthread_mutex_unlock(&mutex));
    if (reservaPendiente) {
        fallar("Cierre con una reserva pendiente en " + ruta);
    }
    if (hiloCreado) {
        int codigo = pthread_join(hilo, nullptr);
        if (codigo != 0) {
            error = "No se pudo unir el escritor: " + string(strerror(codigo));
            informar(error, true);
            return false;
        }
        hiloCreado = false;
    }
    comprobarRegistro(pthread_mutex_lock(&mutex));
    bool completo = !estado.fallo && estado.cerrado;
    error = completo ? "" : ultimoError;
    bool informarCierre = completo && anunciado;
    anunciado = false;
    comprobarRegistro(pthread_mutex_unlock(&mutex));
    if (informarCierre) {
        informar("Registro guardado: " + filesystem::path(ruta).filename().string() + "; duracion=" + formatearDuracion(duracionUs) +
                 "; resultado=" + nombreResultado(resultadoFinal), false);
    }
    return completo;
}

void RegistroEventos::obtenerEstado(EstadoRegistro& resultado) {
    if (!inicializado) {
        resultado = EstadoRegistro();
        return;
    }
    comprobarRegistro(pthread_mutex_lock(&mutex));
    resultado = estado;
    comprobarRegistro(pthread_mutex_unlock(&mutex));
}

string RegistroEventos::obtenerRuta() const {
    return ruta;
}

void RegistroEventos::liberarRecursos() {
    if (hiloCreado) {
        terminate();
    }
    if (descriptor >= 0) {
        int archivo = descriptor;
        descriptor = -1;
        if (close(archivo) != 0) {
            informar("Error de cierre de " + ruta + ": " + strerror(errno), true);
        }
    }
    if (inicializado) {
        comprobarRegistro(pthread_cond_destroy(&hayCapacidad));
        comprobarRegistro(pthread_cond_destroy(&hayEventos));
        comprobarRegistro(pthread_mutex_destroy(&mutex));
        inicializado = false;
    }
}

bool RegistroEventos::escribirTexto(const string& texto) {
    size_t escritos = 0;
    while (escritos < texto.size()) {
        ssize_t cantidad = write(descriptor, texto.data() + escritos, texto.size() - escritos);
        if (cantidad < 0 && errno == EINTR) {
            continue;
        }
        if (cantidad <= 0) {
            int codigo = cantidad == 0 ? EIO : errno;
            fallar("Error de escritura en " + ruta + ": " + strerror(codigo));
            return false;
        }
        escritos += static_cast<size_t>(cantidad);
    }
    return true;
}

void RegistroEventos::fallar(const string& mensaje) {
    comprobarRegistro(pthread_mutex_lock(&mutex));
    bool nuevo = !estado.fallo;
    if (nuevo) {
        ultimoError = mensaje;
        estado.fallo = true;
    }
    comprobarRegistro(pthread_cond_broadcast(&hayCapacidad));
    comprobarRegistro(pthread_cond_broadcast(&hayEventos));
    comprobarRegistro(pthread_mutex_unlock(&mutex));
    if (nuevo) {
        if (avisarError != nullptr) {
            avisarError(contextoError);
        }
        informar(mensaje + "; historial incompleto", true);
    }
}

void* RegistroEventos::ejecutar(void* argumento) {
    RegistroEventos& registro = *static_cast<RegistroEventos*>(argumento);
    try {
        registro.escribirEventos();
    } catch (const exception& error) {
        registro.fallar("Error del escritor: " + string(error.what()));
    } catch (...) {
        registro.fallar("Error inesperado del escritor");
    }
    if (registro.descriptor >= 0) {
        if (fsync(registro.descriptor) != 0) {
            registro.fallar("Error de sincronizacion de " + registro.ruta + ": " + strerror(errno));
        }
        int descriptor = registro.descriptor;
        registro.descriptor = -1;
        if (close(descriptor) != 0) {
            registro.fallar("Error de cierre de " + registro.ruta + ": " + strerror(errno));
        }
    }
    comprobarRegistro(pthread_mutex_lock(&registro.mutex));
    registro.estado.cerrado = true;
    comprobarRegistro(pthread_mutex_unlock(&registro.mutex));
    return nullptr;
}

void RegistroEventos::escribirEventos() {
    uint64_t secuencia = 0;
    uint64_t aceptados = 0;
    uint64_t rechazados = 0;
    uint64_t capturas = 0;
    int64_t anteriorUs = 0;
    bool final = false;
    EventoRegistro ultimo;
    while (true) {
        comprobarRegistro(pthread_mutex_lock(&mutex));
        while (estado.enCola == 0 && !cerrando && !estado.fallo) {
            comprobarRegistro(pthread_cond_wait(&hayEventos, &mutex));
        }
        if (estado.fallo || (estado.enCola == 0 && cerrando)) {
            comprobarRegistro(pthread_mutex_unlock(&mutex));
            break;
        }
        EventoRegistro evento = cola[primero];
        primero = (primero + 1) % cola.size();
        estado.enCola--;
        comprobarRegistro(pthread_cond_broadcast(&hayCapacidad));
        comprobarRegistro(pthread_mutex_unlock(&mutex));
        int64_t tiempoUs = (evento.tiempo.monotonoNs - inicio.monotonoNs) / 1000;
        if (tiempoUs < anteriorUs || final) {
            fallar("Orden temporal o evento posterior al cierre invalido en " + ruta);
            break;
        }
        aceptados += evento.tipo == TipoEventoRegistro::Movimiento || evento.tipo == TipoEventoRegistro::Captura;
        rechazados += evento.tipo == TipoEventoRegistro::Rechazo;
        capturas += evento.tipo == TipoEventoRegistro::Captura;
        ostringstream linea;
        linea << "EVENTO\tsecuencia=" << ++secuencia << "\ttipo=" << nombreTipo(evento.tipo)
              << "\tutc=" << fechaUtc(evento.tiempo.realMs) << "\tt_us=" << tiempoUs
              << "\ttorneo_us=" << (evento.tiempo.monotonoNs - identidad.ejecucion.inicio.monotonoNs) / 1000
              << "\tintervalo_us=" << tiempoUs - anteriorUs
              << "\tpartida=" << (evento.idPartida < 0 ? identidad.idPartida : evento.idPartida)
              << "\tmesa=" << (evento.idMesa < 0 ? identidad.idMesa : evento.idMesa)
              << "\tjugador1=" << (evento.jugador1 < 0 ? identidad.jugadorPuma : evento.jugador1)
              << "\tjugador2=" << (evento.jugador2 < 0 ? identidad.jugadorPerros : evento.jugador2)
              << "\tficha=" << evento.idFicha << "\torigen=" << evento.movimiento.origen.fila << ',' << evento.movimiento.origen.columna
              << "\tintermedio=" << evento.movimiento.intermedio.fila << ',' << evento.movimiento.intermedio.columna
              << "\tdestino=" << evento.movimiento.destino.fila << ',' << evento.movimiento.destino.columna
              << "\tretirado=" << evento.idCapturado << "\tresultado=" << nombreResultado(evento.resultado)
              << "\tmotivo=" << nombreMotivo(evento.motivo) << "\taceptados=" << aceptados
              << "\trechazados=" << rechazados << "\tcapturas=" << capturas;
        if (identidad.esTorneo) {
            linea << "\tmesas_activas=" << evento.mesasActivas
                  << "\tparejas_esperando=" << evento.parejasEsperando;
        }
        linea << "\tocupaciones=" << posiciones(evento.tablero, true) << '\n';
        if (!escribirTexto(linea.str())) {
            break;
        }
        anteriorUs = tiempoUs;
        ultimo = evento;
        final = evento.tipo == TipoEventoRegistro::Cierre;
        comprobarRegistro(pthread_mutex_lock(&mutex));
        estado.eventosEscritos++;
        comprobarRegistro(pthread_mutex_unlock(&mutex));
        presentarEvento(evento, identidad, secuencia, tiempoUs);
    }
    if (!final) {
        fallar("No se pudo completar el evento de cierre de " + ruta);
        return;
    }
    ostringstream resumen;
    resumen << "FIN\tfin_utc=" << fechaUtc(ultimo.tiempo.realMs) << "\tduracion_us=" << anteriorUs
            << "\tduracion_torneo_observada_us=" << (ultimo.tiempo.monotonoNs - identidad.ejecucion.inicio.monotonoNs) / 1000
            << "\taceptados=" << aceptados << "\trechazados=" << rechazados << "\tcapturas=" << capturas
            << "\tresultado=" << nombreResultado(ultimo.resultado) << '\n';
    if (escribirTexto(resumen.str())) {
        duracionUs = anteriorUs;
        resultadoFinal = ultimo.resultado;
        comprobarRegistro(pthread_mutex_lock(&mutex));
        anunciado = true;
        comprobarRegistro(pthread_mutex_unlock(&mutex));
    }
}
