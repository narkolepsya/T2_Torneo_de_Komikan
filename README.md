<div align="center">

# Tarea 2 — Torneo de Komikan Concurrente

Simulación de partidas simultáneas de Komikan con árbitros independientes, hilos POSIX y sincronización en tiempo real.

---

**Universidad Diego Portales** · Escuela de Informática y Telecomunicaciones

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![GNU Make](https://img.shields.io/badge/GNU_Make-A42E2B?style=for-the-badge&logo=gnu&logoColor=white)
![Linux](https://img.shields.io/badge/Plataforma-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![POSIX](https://img.shields.io/badge/Sincronizaci%C3%B3n-POSIX-5965A8?style=for-the-badge)
![Finalizado](https://img.shields.io/badge/Estado-Finalizado-2E8B57?style=for-the-badge)

</div>

---

## 1. Descripción del proyecto

El **Torneo de Komikan Concurrente** simula un conjunto de partidas en tiempo real, administradas por un servidor que recibe `N` jugadores, forma parejas según su llegada y distribuye las partidas entre un máximo de `M` mesas activas. Cuando no hay mesas disponibles, las parejas quedan en espera hasta que una partida finaliza y libera su mesa.

Cada partida enfrenta a **un Puma contra doce Perros** sobre un tablero de **28 posiciones**, con una cuadrícula principal de 5 × 5 y tres posiciones adicionales en la zona del trono. Las fichas actúan independientemente, sin turnos estrictos: cada una intenta ejecutar movimientos después de retardos aleatorios. Un Árbitro valida las solicitudes y actualiza el tablero compartido, evitando que dos movimientos aceptados ocupen una misma posición.

| Bando | Movimiento y objetivo |
|:---|:---|
| **Puma** | Se desplaza a posiciones adyacentes libres y puede capturar saltando sobre un Perro hacia una posición libre. Gana al capturar **seis o más Perros**. |
| **Perros** | Se desplazan a posiciones adyacentes libres, sin capturar. Ganan cuando el Puma queda **sin movimientos legales**, incluidas las capturas. |

El sistema también contempla victorias por abandono, registros de los estados del tablero y la interrupción general mediante `SIGINT` (`Ctrl+C`). El torneo consta de **`N/2` partidas independientes**, sin eliminatorias ni campeón general.

---

## 2. Estructura del proyecto

```text
T2_Torneo_de_Komikan/
├── include/
│   ├── arbitro.h
│   ├── config.h
│   ├── consola.h
│   ├── ficha.h
│   ├── juego.h
│   ├── registro.h
│   ├── servidor.h
│   └── tablero.h
├── src/
│   ├── arbitro.cpp
│   ├── config.cpp
│   ├── consola.cpp
│   ├── ficha.cpp
│   ├── juego.cpp
│   ├── main.cpp
│   ├── registro.cpp
│   ├── servidor.cpp
│   └── tablero.cpp
├── .gitignore
├── Makefile
└── README.md
```

- **`include/`** reúne las declaraciones de clases, estructuras e interfaces.

- **`src/`** contiene la implementación y el punto de entrada del programa.

- **`Makefile`** centraliza la compilación y limpieza del ejecutable.

- **`.gitignore`** excluye los archivos generados que correspondan a sus reglas.

### Responsabilidades de los módulos

| Módulo | Responsabilidad |
|:---|:---|
| `config` | Lee y valida posiciones iniciales, retardos y semilla aleatoria. |
| `tablero` | Representa las posiciones y conexiones; determina movimientos, capturas y movilidad del Puma. |
| `ficha` | Mantiene la identidad y características de cada ficha, incluidos sus retardos. |
| `arbitro` | Recibe solicitudes, las ordena en una cola FIFO y vuelve a validar los movimientos antes de aplicarlos. |
| `juego` | Coordina los hilos de las fichas, el Árbitro, los estados y la finalización de una partida. |
| `servidor` | Administra llegadas, parejas, asignación y reutilización de mesas y cierre del torneo. |
| `registro` | Escribe eventos, resultados, tiempos e instantáneas de los tableros. |
| `consola` | Presenta eventos y tableros ASCII sin intercalar bloques de distintas mesas. |
| `main` | Valida argumentos, carga la configuración, ejecuta el servidor y muestra el resumen. |

---

## 3. Requisitos

Para compilar y ejecutar el proyecto se necesita un entorno **Unix compatible con POSIX** (por ejemplo, Linux), un compilador **g++ con soporte C++17**, **GNU Make** y soporte para **hilos POSIX (`pthread`)**.

No se requiere una interfaz gráfica ni instalar herramientas de análisis para ejecutar el torneo.

---

## 4. Compilación

Desde la raíz del repositorio, ejecutar:

```bash
make
```

Se genera el ejecutable **`torneo`**. Para eliminarlo junto con los archivos objeto:

```bash
make clean
```

Para reconstruir el proyecto desde cero:

```bash
make clean
make
```

### Opciones utilizadas

| Opción | Propósito |
|:---|:---|
| `-std=c++17` | Utiliza el estándar C++17. |
| `-Wall` y `-Wextra` | Habilitan advertencias del compilador. |
| `-Iinclude` | Incorpora las cabeceras del proyecto. |
| `-pthread` | Habilita el soporte de compilación y enlace para hilos POSIX. |

---

## 5. Ejecución

La interfaz de ejecución es:

```bash
./torneo N M [config.txt]
```

| Parámetro | Descripción |
|:---|:---|
| `N` | Número de jugadores. Debe ser **par**, desde **2 hasta 128**. |
| `M` | Número máximo de mesas concurrentes; debe ser un entero positivo. |
| `[config.txt]` | Archivo opcional para definir retardos, posiciones iniciales y semilla. |

### Ejemplos

```bash
./torneo 2 1
./torneo 6 2
./torneo 16 4
./torneo 6 2 config.txt
```

En `./torneo 6 2`, el servidor organiza **tres partidas**, permite **hasta dos mesas activas** y mantiene la pareja restante en espera cuando ambas mesas están ocupadas. El número `M` puede superar las partidas previstas: el sistema solo utiliza las mesas necesarias.

Para utilizar el último ejemplo, primero debe crearse `config.txt` con el formato de la sección 6. Si no se especifica configuración, se utiliza la disposición inicial predeterminada, con retardos entre **50 y 300 ms** y semilla generada automáticamente.

### Salida en terminal

La consola identifica la llegada de jugadores, la formación de parejas, la asignación de mesas y los resultados. Cada bloque de tablero corresponde a una partida y una mesa; su impresión está sincronizada para evitar que se mezclen líneas de distintos hilos.

La visualización detallada puede configurarse mediante la variable `KOMIKAN_TABLEROS`:

```bash
KOMIKAN_TABLEROS=1 ./torneo 6 2
KOMIKAN_TABLEROS=0 ./torneo 16 4
```

| Valor | Presentación |
|:---|:---|
| `1` | Muestra los tableros ASCII iniciales, actualizados y finales, junto con los eventos. |
| `0` | Presenta un resumen de la actividad del torneo, sin imprimir cada tablero. |
| Sin definir | Activa los tableros detallados cuando `N` es **10 o menor**. |

Al finalizar se indica cuántas partidas comenzaron y terminaron, el máximo de mesas utilizadas, las parejas que esperaron, los resultados individuales y la duración del torneo.

### Comprobaciones directas

Los siguientes comandos permiten revisar el funcionamiento del **ejecutable publicado**, sin archivos adicionales:

| Comando | Qué permite observar |
|:---|:---|
| `./torneo 2 1` | Una partida con movimientos y condición de victoria. |
| `./torneo 6 2` | Formación de tres parejas, dos mesas simultáneas y reutilización. |
| `./torneo 16 4` | Ocho partidas y control del límite de cuatro mesas. |
| `./torneo 3 2` | Rechazo de una cantidad impar de jugadores. |
| `./torneo 6 0` | Rechazo de un número inválido de mesas. |
| `./torneo 6 2 archivo_inexistente.txt` | Manejo de un archivo de configuración inexistente. |

Para comprobar la interrupción, iniciar un torneo con partidas activas y presionar **`Ctrl+C`** una vez. El servidor detiene la asignación de nuevas partidas, notifica a las activas y coordina su finalización y la liberación de recursos.

---

## 6. Formato del archivo de configuración

El archivo opcional utiliza una asignación por línea con la estructura `clave = valor`. Se requieren los dos retardos y las posiciones de las **13 fichas**. La `semilla` es opcional. Las posiciones se expresan como `fila,columna`; se permiten líneas vacías y comentarios iniciados con `#`.

### Ejemplo válido de `config.txt`

```ini
retardoMinimo = 50
retardoMaximo = 300
semilla = 1234
puma = 4,2
perro1 = 4,0
perro2 = 4,4
perro3 = 5,0
perro4 = 5,1
perro5 = 5,2
perro6 = 5,3
perro7 = 5,4
perro8 = 6,0
perro9 = 6,1
perro10 = 6,2
perro11 = 6,3
perro12 = 6,4
```

| Campo | Validación |
|:---|:---|
| `retardoMinimo`, `retardoMaximo` | Enteros no negativos, con mínimo menor o igual que máximo. |
| `semilla` | Valor entero para inicializar la generación aleatoria; opcional. |
| `puma`, `perro1` ... `perro12` | Posiciones válidas del tablero, sin dos fichas en la misma posición. |

La semilla fija la inicialización del generador aleatorio bajo la misma configuración, pero **no impone un orden idéntico de ejecución entre hilos**, pues la planificación concurrente puede variar.

---

## 7. Decisiones de diseño

### Organización modular

Se separaron el procesamiento de la configuración, las reglas del tablero, la ejecución de las fichas, el arbitraje, la administración de mesas y los mecanismos de salida. Esta división mantiene las reglas del Komikan independientes del servidor y facilita revisar cada responsabilidad.

### Organización del torneo

Se adoptó el modelo de **`N/2` partidas independientes**, sin eliminatorias ni ganador general, de acuerdo con la aclaración del curso. Para evitar jugadores sin pareja se exige que `N` sea par y al menos 2. El máximo de **128 jugadores** es una restricción propia de la implementación para acotar el uso de recursos, no una regla del juego. `M` debe ser positivo y puede superar `N/2`.

### Concurrencia de las fichas y arbitraje

Cada partida ejecuta **13 hilos de fichas** y un **hilo Árbitro**. Las fichas actúan independientemente, sin turnos, y envían solicitudes a una **cola FIFO**. El Árbitro las atiende y **revalida cada acción sobre el tablero actualizado** antes de aplicarla. Así, una solicitud que era legal al prepararse puede rechazarse si otra ficha ocupó antes su destino. El Árbitro permanece bloqueado mediante `pthread_cond_wait()` mientras no hay solicitudes y despierta cuando una ficha notifica la cola; no consulta el tablero periódicamente.

### Selección de movimientos y capturas

Las fichas obtienen sus movimientos legales y seleccionan aleatoriamente entre las opciones disponibles, en vez de probar coordenadas al azar hasta encontrar una válida. El tablero modela explícitamente las conexiones y las trayectorias de salto, incluidas las particulares del trono, evitando inferir movimientos solo por diferencias entre coordenadas.

### Gestión de mesas y emparejamiento

El servidor recibe jugadores concurrentemente y forma parejas según el orden de llegada. Cada pareja ocupa una mesa disponible; si se alcanza el límite `M`, espera de forma **pasiva**. Cuando una partida termina, la mesa se libera y se asigna a una pareja pendiente. El límite de mesas es independiente del número de fichas que actúan en cada partida.

### Sincronización y atomicidad

Se utilizan **mutexes y variables de condición POSIX** para coordinar colas, estados y disponibilidad. Cada partida mantiene un tablero propio, y las modificaciones quedan centralizadas en el Árbitro. Un desplazamiento y la eliminación de la ficha capturada se aplican como **una operación validada**, evitando estados intermedios visibles entre movimientos aceptados. La consola emplea un mutex independiente para imprimir bloques completos.

### Retardos y esperas interrumpibles

Los ciclos de decisión respetan los retardos aleatorios configurados. Las esperas de las fichas se implementan con **variables de condición temporizadas**, que pueden notificarse cuando la partida termina o se solicita su interrupción, sin necesitar esperar a que expire un retardo elevado.

### Manejo de abandono y SIGINT

Ante el abandono de un jugador se declara vencedor a su oponente y se cierra la partida correspondiente. La señal `SIGINT` se gestiona de manera coordinada: se detienen las nuevas asignaciones, se notifica a las partidas activas y se espera a los hilos antes de destruir recursos. La implementación comprueba los resultados de operaciones POSIX y diferencia los recursos inicializados de aquellos cuya creación falló.

### Registro de eventos

Cada ejecución genera un registro del torneo y uno por partida, con secuencias, tiempos, movimientos, capturas, resultados y estados del tablero. La escritura se separa de la actualización de posiciones para no incorporar operaciones de disco a la sección crítica de los movimientos.

### Uso de GNU Make

El `Makefile` permite compilar todos los módulos con C++17 mediante `make` y eliminar los archivos producidos con `make clean`.

---

## 8. Registros de ejecución

El torneo genera una carpeta diferenciada para cada ejecución, cuyo identificador se muestra al iniciar el programa:

```text
registros/
└── ejecucion_XXXXXX/
    ├── torneo.txt
    ├── partida_1.txt
    ├── partida_2.txt
    └── ...
```

| Archivo | Información que conserva |
|:---|:---|
| `torneo.txt` | Llegadas, emparejamientos, mesas asignadas, esperas, liberaciones, resultados y cierre del servidor. |
| `partida_N.txt` | Estado inicial, movimientos y capturas, instantáneas del tablero, tiempos, resultado y causa de finalización. |

Los registros permiten reconstruir la evolución de cada partida y comprobar las acciones que el Árbitro aceptó, aunque se utilice el modo resumido de consola.

---

## 9. Integrantes

- **Gabriela Esparza**

- **Jose Larrondo**

---

<div align="center">

Desarrollado para la asignatura de **Sistemas Operativos** · 2026

</div>
