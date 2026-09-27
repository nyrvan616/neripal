# NeriPal

Mascota virtual retro-moderna para Windows y la placa Waveshare
ESP32-S3-Touch-LCD-1.54. El proyecto comparte un Game Core C++17 entre el
simulador y el firmware, con presentación 240x240, tiempo inyectable y tests
deterministas.

El rótulo vigente es 0.6.1: `include/neripal/Version.hpp` y
`project(NeriPal VERSION 0.6.1)`. La pantalla muestra `NERIPAL 0.6.1` y el
serial `NeriPal 0.6.1`. El cierre de juego sigue en la milestone 0.6.

NeriPal está preparado como un repositorio independiente. No necesita el
directorio que lo contiene ni repositorios hermanos. En esta documentación,
"raíz del proyecto" significa la carpeta `NeriPal/` que contiene
`CMakeLists.txt` y `platformio.ini`.

## Estado ejecutivo

| Área | Estado | Alcance actual |
|---|---|---|
| Game Core | Operativo | Necesidades, historial, etapas hasta Final, reglas fixture, offline |
| Simulador Windows | Operativo | Menú de 8 ítems, evolución, save en disco, Debug Tools laterales |
| UI V-Pet | Operativa en host | Home, Status, señales de necesidad y aviso de evolución |
| Tests | Operativos | Core, UI y persistencia (`FakeClock`, `FakeRandom`) |
| Build ESP32-S3 | Operativo | Mismo loop que el host; save NVS; seed RNG aún placeholder |
| HAL Waveshare | Base parcial | Reloj, power-hold, backlight, tres botones y NVS |
| Display/touch/audio/IMU | Pendiente | `IRenderer` completo sigue vacío, `drawText` incluido |

0.6.1 es el rótulo del simulador y del firmware. Está verificada en host, tests
y build de firmware.

El cierre de 0.6 está en [`docs/MILESTONE_0.6.md`](docs/MILESTONE_0.6.md). Los de
0.4 y 0.3, y el diagnóstico de 0.1, quedan como historia en `docs/`.

## Funcionalidad disponible

- `PetState`: cinco stats base, salud, felicidad, edad, sueño, etapa, forma y pose.
- Acciones: feed, train, sleep, wake, clean, pet y play, con rechazos motivados.
- Autonomía: Idle, Walk 1D, Nap, y reacciones Eat, Happy, Tired, Dirty, Annoyed.
- Etapas Egg → Baby → Child → Adult → Final. La edad abre la etapa; el historial elige la forma fixture en Child → Adult.
- `EvolutionNotice` persistente. Confirmarlo no cambia etapa ni forma.
- Save V2 en el simulador (`neripal-slot0.bin` / `neripal-slot1.bin`) y NVS en firmware. Lectura y migración de V1.
- Tiempo apagado: necesidades y edad avanzan; los care mistakes no se generan offline.
- Menú: FEED, TRAIN, SLEEP/WAKE, CLEAN, PET, PLAY, STATUS, HOME.
- Home muestra solo necesidades que no están en Normal. Status muestra los números, la etapa y el identificador de forma.
- Reloj desktop x1, x10, x100 y x1000. RNG inyectado desde el composition root.
- Debug Tools fuera del viewport. `E` fuerza etapa y forma coherente y no ejecuta las reglas de juego.
- Firmware con el mismo cableado de cuidado y de avisos que el simulador, sin `DebugController`.

## No incluido todavía

- Render real en ST7789 y lectura CST816T.
- Enfermedad, medicina, muerte, despedida, nuevo huevo y ciclo completo.
- Consecuencias de sobrealimentación o sobreentrenamiento, personalidad y entorno.
- Audio real. El gancho de aviso urgente existe y no reproduce sonido.
- Formas, sprites y reglas evolutivas definitivos. AdultA/B/C/Secret son fixtures.
- IMU QMI8658, medición de batería y SD.
- Wi-Fi gameplay, ESP-NOW, combate, multiplayer, tienda o inventario.
- CI, tests contractuales de HAL y validación de la UI sobre hardware físico.

## Requisitos en Windows

- Compilador C++17 para Windows: Visual Studio o MinGW-w64/WinLibs.
- CMake 3.20 o superior y Ninja.
- PlatformIO Core o la extensión PlatformIO de VS Code para firmware.

El entorno usado para la validación final tuvo GCC 16.1.0, CMake 4.4.1,
PlatformIO Core 6.2.0 y `espressif32@6.12.0`.

## Inicio rápido

Abra PowerShell en la raíz del proyecto. Por ejemplo, después de clonar o copiar
solo esta carpeta:

```powershell
Set-Location C:\ruta\a\NeriPal
```

Compile, pruebe y ejecute el simulador:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
.\build\windows-debug\neripal_simulator.exe
```

Para consultar la versión sin abrir la ventana:

```powershell
.\build\windows-debug\neripal_simulator.exe --version
```

El script equivalente compila el host y ejecuta los tests:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Para compilar firmware sin conectar la placa:

```powershell
pio run -e waveshare_s3_154
```

La carga y el monitor serie, cuando haya hardware disponible, serán:

```powershell
pio run -e waveshare_s3_154 -t upload
pio device monitor -e waveshare_s3_154
```

Los flujos de compilación Debug/Release, depuración con VS Code o GDB, limpieza,
firmware y solución de problemas están reunidos en
[`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md).

## Controles del simulador

| Tecla | Acción |
|---|---|
| `Z` / `Right` | Siguiente opción del dispositivo |
| `X` / `Enter` | Confirmar (las acciones de cuidado aplican al Pet; un aviso de evolución se confirma) |
| `C` / `Backspace` | Volver |
| `Esc` | Salir |

El panel lateral de debug conserva estas herramientas de desarrollo:

| Tecla | Acción de debug |
|---|---|
| `F`, `T` | Alimentar, entrenar |
| `S`, `W` | Dormir/despertar |
| `L` | Limpiar |
| `P`, `Y` | Acariciar, jugar |
| `R` | Reset |
| `1`, `2`, `3`, `4` | Tiempo x1, x10, x100, x1000 |
| `A` | Avanzar una hora simulada |
| `E` | Forzar la etapa siguiente y una forma coherente (no es el motor de reglas) |
| `Tab` | Seleccionar stat (hambre, felicidad, energía, salud, higiene, affection, stimulation) |
| `Up` / `Down` | Cambiar stat seleccionado ±5 |
| `Home` / `End` | Llevar stat a 100/0 |

Más detalles en [`simulator/README.md`](simulator/README.md).

## Estructura

```text
include/neripal/           API pública de Core, UI y contratos HAL
src/core/                  reglas puras de juego
src/ui/                    presentación lógica 240x240
src/persist/               codec y sesión de save
src/platform/desktop/      ventana, input, reloj y save en disco
src/platform/waveshare_s3/ HAL, NVS y composition root ESP32
simulator/debug/           herramientas exclusivas de desarrollo
tests/unit/core/           tests de juego
tests/unit/ui/             tests de presentación
tests/unit/persist/        tests de save
assets/placeholders/       reserva para arte original; 0.6.1 sigue en rectángulos
docs/                      arquitectura, desarrollo, hardware y cierres
```

## Documentación

- [`docs/README.md`](docs/README.md): índice documental.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): capas y dependencias.
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md): compilación, ejecución, tests y debug.
- [`docs/HARDWARE.md`](docs/HARDWARE.md): configuración, pinout y estado de la HAL.
- [`docs/MILESTONE_0.6.md`](docs/MILESTONE_0.6.md): cierre del ciclo evolutivo. El rótulo vigente es 0.6.1.
- [`docs/MILESTONE_0.4.md`](docs/MILESTONE_0.4.md) y [`docs/MILESTONE_0.3.md`](docs/MILESTONE_0.3.md): cierres históricos.
- [`docs/STATUS_REPORT.md`](docs/STATUS_REPORT.md): diagnóstico histórico de la 0.1.
- [`tests/README.md`](tests/README.md): alcance de pruebas.
- [`src/platform/waveshare_s3/README.md`](src/platform/waveshare_s3/README.md): estado real de la HAL.

## Próximo trabajo

El ciclo de 0.7 (enfermedad, envejecimiento, muerte y reglas definitivas) no está
empezado. El rótulo vigente es 0.6.1. El detalle del cierre de juego está en
[`docs/MILESTONE_0.6.md`](docs/MILESTONE_0.6.md).
