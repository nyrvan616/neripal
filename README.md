# NeriPal 0.4.0

Mascota virtual retro-moderna para Windows y la placa Waveshare
ESP32-S3-Touch-LCD-1.54. El proyecto comparte un Game Core C++17 entre el
simulador y el firmware, con presentación 240x240, tiempo inyectable y tests
deterministas.

NeriPal está preparado como un repositorio independiente. No necesita el
directorio que lo contiene ni repositorios hermanos. En esta documentación,
"raíz del proyecto" significa la carpeta `NeriPal/` que contiene
`CMakeLists.txt` y `platformio.ini`.

## Estado ejecutivo

| Área | Estado | Alcance actual |
|---|---|---|
| Game Core | Operativo | Crianza 0.3 + Autonomy (Idle/Walk/Nap), `deriveMood`, `GameEvent` |
| Simulador Windows | Operativo | Menú de cuidado 240x240, mascota que camina sola, Debug Tools laterales |
| UI V-Pet | Operativa | Home con X/facing, Eat/Dirty/Annoyed placeholder, overlays |
| Tests | Operativos | Core + UI deterministas (`FakeClock`, `FakeRandom`) |
| Build ESP32-S3 | Operativo | Firmware compilable; seed RNG inyectada en el root |
| HAL Waveshare | Base parcial | Reloj, power-hold, backlight y tres botones |
| Display/touch/audio/IMU | Pendiente | Pinout documentado; ST7789 sigue stub |

Esta versión hace que la mascota se sienta viva en el dispositivo lógico. No está
lista para una demo visual sobre la placa física: el renderer ST7789 no dibuja.

El cierre de 0.4 está en [`docs/MILESTONE_0.4.md`](docs/MILESTONE_0.4.md). El de
0.3 permanece en [`docs/MILESTONE_0.3.md`](docs/MILESTONE_0.3.md). El diagnóstico
histórico de 0.1 está en [`docs/STATUS_REPORT.md`](docs/STATUS_REPORT.md).

## Funcionalidad disponible

- `PetState`: stats, edad, sueño, etapa y snapshot de activity (`x`, facing, elapsed).
- Acciones de cuidado: feed, train, sleep, wake, clean, con rechazos motivados.
- Autonomía: Idle (variantes por mood), Walk 1D, Nap si la energía es crítica.
- Reacciones: Eat, Happy, Tired, Dirty, Annoyed; Sleep visual con Z.
- `deriveMood` como única fuente de ánimo; `pollEvent()` one-shot de activity.
- Menú de dispositivo: FEED, TRAIN, SLEEP/WAKE, CLEAN, STATUS, HOME.
- Overlays de feedback (~900 ms) según `CareResult`, dentro de 240x240.
- Degradación temporal: hambre, energía, hygiene en vigilia, felicidad y salud.
- Evolución mínima por edad: Egg (preview), Baby, Child y Adult.
- Reloj desktop acelerable x1, x10, x100 y x1000.
- RNG inyectado (`XorShift32`) desde el composition root; tests con `FakeRandom`.
- Debug Tools fuera del viewport: stats (incl. hygiene), L=clean, sin setters en Pet.
- Firmware con el mismo cableado de cuidado que el simulador; sin DebugController.
- Versión visible en la cabecera del juego, consola desktop y Serial del ESP32.

## No incluido todavía

- Render real en ST7789 y lectura CST816T.
- Persistencia, recuperación del tiempo apagado o versionado de saves.
- Enfermedad completa, medicina, muerte, poop, día/noche.
- Audio ES8311, IMU QMI8658 y medición/calibración de batería.
- Wi-Fi gameplay, ESP-NOW, combate, multiplayer, tienda o inventario.
- Evolución ramificada o minijuegos.
- Assets visuales finales y pipeline de sprites.
- CI, tests contractuales de HAL y validación sobre hardware físico.

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
| `X` / `Enter` | Confirmar (FEED/TRAIN/SLEEP/CLEAN aplican al Pet) |
| `C` / `Backspace` | Volver |
| `Esc` | Salir |

El panel lateral de debug conserva estas herramientas de desarrollo:

| Tecla | Acción de debug |
|---|---|
| `F`, `T` | Alimentar, entrenar |
| `S`, `W` | Dormir/despertar |
| `L` | Limpiar |
| `R` | Reset |
| `1`, `2`, `3`, `4` | Tiempo x1, x10, x100, x1000 |
| `A` | Avanzar una hora simulada |
| `E` | Forzar la siguiente evolución visual |
| `Tab` | Seleccionar stat (incluye hygiene) |
| `Up` / `Down` | Cambiar stat seleccionado ±5 |
| `Home` / `End` | Llevar stat a 100/0 |

Más detalles en [`simulator/README.md`](simulator/README.md).

## Estructura

```text
include/neripal/           API pública de Core, UI y contratos HAL
src/core/                  reglas puras de juego
src/ui/                    presentación lógica 240x240
src/platform/desktop/      ventana, input y reloj Windows
src/platform/waveshare_s3/ HAL y composition root ESP32
simulator/debug/           herramientas exclusivas de desarrollo
tests/unit/core/           tests host y FakeClock
tests/unit/ui/             tests de navegación, overlays y viewport
assets/placeholders/       reserva para arte original
docs/                      arquitectura, desarrollo, hardware y diagnóstico
```

## Documentación

- [`docs/README.md`](docs/README.md): índice documental.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): capas y dependencias.
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md): compilación, ejecución, tests y debug.
- [`docs/HARDWARE.md`](docs/HARDWARE.md): configuración, pinout y estado de la HAL.
- [`docs/MILESTONE_0.3.md`](docs/MILESTONE_0.3.md): cierre de la 0.3.
- [`docs/STATUS_REPORT.md`](docs/STATUS_REPORT.md): diagnóstico histórico de la 0.1.
- [`tests/README.md`](tests/README.md): alcance de pruebas.
- [`src/platform/waveshare_s3/README.md`](src/platform/waveshare_s3/README.md): estado real de la HAL.

## Próximo hito recomendado

Implementar `IRenderer` sobre ST7789 con Arduino_GFX, manteniendo intactos Core y
`PetView`; después integrar CST816T y validar físicamente rotación, inversión,
touch, PSRAM y batería. El comportamiento autónomo del roadmap 0.4 no forma parte
de esta entrega.
