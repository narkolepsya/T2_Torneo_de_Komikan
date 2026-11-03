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

El **Torneo de Komikan Concurrente** simula partidas en tiempo real mediante un servidor que recibe `N` jugadores, forma parejas según su orden de llegada y distribuye las partidas entre un máximo de `M` mesas activas. Cuando todas las mesas están ocupadas, las parejas restantes esperan hasta que se libera una.

Cada partida enfrenta a **un Puma contra doce Perros** en un tablero de **28 posiciones**, formado por una cuadrícula principal de 5 × 5 y tres posiciones adicionales en el trono. Las fichas actúan de manera independiente, sin turnos estrictos, después de retardos aleatorios. Un Árbitro recibe sus solicitudes, valida los movimientos y actualiza el tablero para evitar conflictos entre fichas.

| Bando | Movimiento y condición de victoria |
|:---|:---|
| **Puma** | Se mueve a posiciones adyacentes libres y puede capturar saltando sobre un Perro hasta una posición vacía, siguiendo una trayectoria válida. Gana al capturar **seis o más Perros**. |
| **Perros** | Se mueven a posiciones adyacentes libres y no pueden capturar. Ganan cuando el Puma queda **sin movimientos ni capturas legales**. |

El sistema también contempla la victoria por abandono de un jugador, los registros de cada partida y la interrupción general mediante `SIGINT` (`Ctrl+C`).

El torneo consta de **`N/2` partidas independientes**, sin eliminatorias ni un campeón general.

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
├── Informe_Komikan.pdf
├── Makefile
└── README.md
```

- **`include/`** contiene las declaraciones de clases, estructuras e interfaces del proyecto.
- **`src/`** contiene la implementación de los módulos y el punto de entrada del programa.
- **`Makefile`** centraliza las reglas de compilación y limpieza.
- **`.gitignore`** define los archivos generados que Git debe excluir.
- **`Informe_Komikan.pdf`** presenta el diseño de sincronización, sus riesgos y las decisiones adoptadas.

### Responsabilidades de los módulos

| Módulo | Responsabilidad |
|:---|:---|
| `config` | Lee y valida los retardos, las posiciones iniciales y la semilla aleatoria. |
| `tablero` | Representa las posiciones y conexiones; valida los movimientos, las capturas y la movilidad del Puma. |
| `ficha` | Representa las fichas y sus características de ejecución. |
| `arbitro` | Recibe solicitudes mediante una cola FIFO, comprueba su legalidad y responde a cada ficha. |
| `juego` | Coordina los hilos de una partida, sus estados, las condiciones de victoria y la finalización. |
| `servidor` | Gestiona llegadas, emparejamientos, mesas disponibles, esperas y cierre del torneo. |
| `registro` | Guarda eventos, movimientos, capturas, tiempos, resultados y estados del tablero. |
| `consola` | Presenta eventos y tableros ASCII mediante impresiones sincronizadas. |
| `main` | Valida los argumentos, carga la configuración, inicia el servidor y muestra el resumen final. |

---

## 3. Requisitos

Para compilar y ejecutar el programa se necesita:

- Un sistema operativo **Unix compatible con POSIX**, como Linux.
- Un compilador **g++ compatible con C++17**.
- **GNU Make**.
- Soporte para **hilos POSIX (`pthread`)**.

La ejecución normal del torneo se realiza en la terminal y no requiere una interfaz gráfica.

---

## 4. Compilación

Desde la carpeta raíz del repositorio:

```bash
make
```

Este comando compila los módulos y genera el ejecutable **`torneo`**.

Para eliminar el ejecutable y los archivos objeto:

```bash
make clean
```

Para realizar una compilación limpia:

```bash
make clean
make
```

### Opciones utilizadas

| Opción | Propósito |
|:---|:---|
| `-std=c++17` | Compila con el estándar C++17. |
| `-Wall` | Habilita las advertencias comunes del compilador. |
| `-Wextra` | Habilita advertencias adicionales. |
| `-Iinclude` | Indica la ubicación de los archivos de cabecera. |
| `-pthread` | Habilita el soporte de compilación y enlace para hilos POSIX. |

---

## 5. Ejecución

El programa se ejecuta desde la carpeta raíz mediante:

```bash
./torneo N M [config.txt]
```

| Parámetro | Descripción |
|:---|:---|
| `N` | Número de jugadores. Debe ser **par**, entre **2 y 128**. |
| `M` | Cantidad máxima de mesas simultáneas. Debe ser un entero positivo. |
| `[config.txt]` | Archivo opcional con retardos, posiciones iniciales y semilla aleatoria. |

### Ejemplos

```bash
./torneo 2 1
./torneo 6 2
./torneo 16 4
./torneo 6 2 config.txt
```

En `./torneo 6 2`, el servidor forma **tres parejas** y permite que funcionen **hasta dos mesas al mismo tiempo**. Si ambas están ocupadas, la tercera pareja espera una mesa libre. El valor de `M` también puede ser mayor que la cantidad de partidas, en cuyo caso solo se utilizan las mesas necesarias.

Para ejecutar el último ejemplo, primero se debe crear `config.txt` siguiendo el formato de la sección 6. Si no se proporciona ese archivo, el programa utiliza la disposición inicial predeterminada, retardos de **50 a 300 ms** y una semilla generada automáticamente.

### Salida en terminal

La consola muestra las llegadas de los jugadores, las parejas formadas, la asignación y liberación de mesas, los movimientos, las capturas y los resultados. Los tableros ASCII se imprimen de manera sincronizada para que los mensajes de distintas mesas no se mezclen.

La variable `KOMIKAN_TABLEROS` permite controlar la cantidad de información mostrada:

```bash
KOMIKAN_TABLEROS=1 ./torneo 6 2
KOMIKAN_TABLEROS=0 ./torneo 16 4
```

| Valor | Presentación |
|:---|:---|
| `1` | Muestra los tableros ASCII y sus actualizaciones junto con los eventos. |
| `0` | Muestra la actividad general sin imprimir cada estado del tablero. |
| Sin definir | Activa la visualización detallada cuando `N` es **10 o menor**. |

Al finalizar se presenta un resumen con las partidas iniciadas y terminadas, el máximo de mesas utilizadas, las parejas que esperaron, los ganadores y la duración total.

### Comprobaciones directas

Los siguientes comandos permiten comprobar las funciones principales utilizando el ejecutable:

| Comando | Qué permite observar |
|:---|:---|
| `./torneo 2 1` | Una partida con movimientos y condiciones de victoria. |
| `./torneo 6 2` | Emparejamiento, dos mesas simultáneas, espera y reutilización. |
| `./torneo 16 4` | Ocho partidas y control del límite de cuatro mesas. |
| `./torneo 3 2` | Rechazo de una cantidad impar de jugadores. |
| `./torneo 6 0` | Rechazo de una cantidad inválida de mesas. |
| `./torneo 6 2 archivo_inexistente.txt` | Manejo de un archivo de configuración inexistente. |

Para comprobar la interrupción general, se puede iniciar un torneo y presionar **`Ctrl+C`** mientras hay partidas activas. El servidor detiene nuevas llegadas y asignaciones, notifica a los hilos activos y coordina su finalización antes de liberar los recursos.

---

## 6. Formato del archivo de configuración

El archivo opcional utiliza una asignación por línea:

```text
clave = valor
```

Debe contener los retardos mínimo y máximo y las posiciones de las **13 fichas**. El campo `semilla` es opcional. Las posiciones se expresan como `fila,columna`; también se aceptan líneas vacías y comentarios iniciados con `#`.

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
| `puma`, `perro1` ... `perro12` | Posiciones existentes del tablero, sin dos fichas en la misma posición. |

La semilla permite inicializar los generadores aleatorios de forma reproducible, pero **no asegura el mismo orden de movimientos en todas las ejecuciones**, debido a la planificación concurrente de los hilos.

---

## 7. Decisiones de diseño

### Organización modular

Se separaron la configuración, las reglas del tablero, las fichas, el arbitraje, la ejecución de partidas, la administración de mesas y la presentación de resultados. Esto permite mantener las reglas del Komikan independientes de la lógica del servidor y facilita revisar cada componente.

### Organización del torneo

El torneo se organiza en **`N/2` partidas independientes**, sin eliminatorias ni ganador general, de acuerdo con la aclaración del curso. Se exige que `N` sea par y al menos 2 para formar parejas completas. Se estableció un máximo de **128 jugadores** como límite propio de la implementación para controlar el uso de recursos. El valor de `M` debe ser positivo y puede superar `N/2`.

Cada partida activa utiliza **14 hilos**: uno para el Puma, doce para los Perros y uno para el Árbitro. Estos son adicionales a los hilos utilizados por el servidor para administrar las llegadas y las mesas.

### Representación del tablero

El tablero utiliza posiciones y conexiones definidas explícitamente. **Los círculos representan las posiciones donde pueden ubicarse las fichas y las líneas indican por dónde pueden moverse**. Un cruce de líneas sin círculo no se considera una posición nueva.

En la cuadrícula principal se permiten movimientos horizontales y verticales. El trono tiene conexiones especiales, incluidas las diagonales que aparecen dibujadas. Algunas líneas del trono conectan directamente dos posiciones aunque crucen otras líneas, por lo que no sería correcto determinar todos los movimientos solo a partir de diferencias entre filas y columnas.

Las conexiones para movimientos simples y las **trayectorias de captura** se representan por separado. Para capturar, el Puma debe saltar sobre un Perro y llegar a una posición vacía siguiendo una línea recta autorizada; no puede cambiar de dirección durante el salto.

### Selección de movimientos y capturas

Cada ficha consulta el estado del tablero y obtiene los movimientos que son legales en ese momento. Se descartan las posiciones ocupadas y las acciones que no cumplen las reglas. Después se selecciona aleatoriamente una de las opciones disponibles, en lugar de probar destinos al azar hasta encontrar uno válido.

El Puma puede elegir entre movimientos simples y capturas, **sin prioridad obligatoria para capturar**. Las capturas se realizan de una en una: si después existe otra oportunidad, se decide mediante una nueva solicitud de movimiento.

Cada ficha dispone de su propio generador aleatorio, evitando que todos los hilos modifiquen el estado de un único generador compartido.

### Concurrencia de las fichas y arbitraje

Antes de empezar, los hilos de la partida se preparan y esperan una señal de inicio común. Después, las fichas actúan independientemente, sin turnos ni alternancia de bandos.

Cuando una ficha decide moverse, envía una solicitud a una **cola FIFO** y espera la respuesta mediante su **buzón individual**. Cada ficha mantiene como máximo una solicitud pendiente.

Si no hay solicitudes, el Árbitro permanece bloqueado con `pthread_cond_wait()`, sin revisar la cola periódicamente. Cuando una ficha incorpora una petición, notifica al Árbitro para que despierte y la procese.

El Árbitro revisa nuevamente el origen, el destino y las reglas del movimiento sobre el **tablero actualizado**. Esto es necesario porque otra ficha pudo modificarlo mientras la solicitud esperaba en la cola. Si el movimiento ya no es legal, se rechaza y se informa a la ficha; si continúa activa, puede iniciar otro ciclo de decisión.

La cola FIFO establece el orden de atención de las solicitudes que han ingresado, pero no garantiza que los hilos sean ejecutados por el sistema operativo en un orden determinado.

### Sincronización y condiciones de victoria

Se utilizan **mutexes y variables de condición POSIX** para coordinar solicitudes, respuestas, estados de partida y disponibilidad de mesas. Cada partida tiene su propio tablero y mecanismos de sincronización.

Solo el Árbitro modifica la ocupación del tablero. La validación y aplicación del movimiento se realizan de forma coordinada, de manera que dos solicitudes no puedan ocupar simultáneamente la misma posición. En una captura, el desplazamiento del Puma y la retirada del Perro se aplican como una única operación.

El Árbitro comprueba las condiciones de victoria al iniciar la partida y después de procesar solicitudes. Esto permite detectar un resultado desde la disposición inicial y evita continuar aceptando movimientos después de terminar la partida.

Si un Perro es capturado, se notifica a su hilo para que deje de participar y termine ordenadamente.

### Retardos y esperas interrumpibles

Las fichas esperan retardos aleatorios dentro de los límites configurados antes de sus intentos de movimiento. Estas esperas utilizan **variables de condición temporizadas**, de modo que pueden interrumpirse cuando termina la partida o se solicita un cierre.

Si una ficha no dispone de movimientos legales, espera una modificación del tablero en lugar de consultar continuamente las mismas posiciones. De esta manera se evitan ciclos de espera activa innecesarios.

### Gestión de mesas y emparejamiento

El servidor recibe jugadores concurrentemente y forma parejas según el orden de llegada. Cada pareja ocupa una mesa disponible y, cuando todas las mesas están ocupadas, queda en **espera pasiva** mediante sincronización POSIX.

Al terminar una partida, la mesa se libera y puede ser utilizada por otra pareja. El límite `M` controla la cantidad de mesas activas, independientemente del número de hilos que utiliza cada partida.

### Manejo de abandono y SIGINT

Si un jugador abandona o presenta un fallo interno durante una partida, se declara ganador a su oponente y se coordina el cierre de los hilos de esa partida.

Ante `SIGINT` (`Ctrl+C`), el servidor detiene la llegada de nuevos jugadores y el inicio de partidas pendientes, notifica a las partidas activas y espera la finalización de sus hilos antes de liberar los recursos. También se comprueban las operaciones POSIX y se lleva control de los recursos inicializados para manejar fallos durante su creación.

### Registro de eventos

Cada ejecución guarda un registro general del torneo y archivos independientes para las partidas. Estos incluyen eventos, movimientos, capturas, resultados, tiempos y estados del tablero.

La escritura de registros se separa de la actualización del tablero para evitar mantener bloqueada la sección crítica de movimientos durante operaciones de disco. La consola también utiliza sincronización propia para imprimir mensajes completos sin mezclar sus líneas.

### Uso de GNU Make

El `Makefile` permite compilar los módulos con C++17 mediante `make` y eliminar el ejecutable y los archivos objeto con `make clean`.

---

## 8. Registros de ejecución

Cada ejecución genera una carpeta identificada dentro de `registros/`:

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
| `torneo.txt` | Llegadas, emparejamientos, mesas, esperas, liberaciones, resultados y cierre del torneo. |
| `partida_N.txt` | Estado inicial, movimientos, capturas, actualizaciones del tablero, tiempos, resultado y causa de finalización. |

Los registros permiten revisar el desarrollo de las partidas, incluso cuando se utiliza el modo resumido de la consola.

---

## 9. Integrantes

- **Gabriela Esparza**
- **Jose Larrondo**

---

<div align="center">

Desarrollado para la asignatura de **Sistemas Operativos** · 2026

</div>
