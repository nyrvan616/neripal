# NeriPal 0.2.0

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
| Game Core | Operativo | Stats, feed, train, sleep/wake, edad, degradación y evolución simple |
| Simulador Windows | Operativo | Ventana 240x240 escalada, teclado y Debug Tools |
| UI V-Pet | Operativa | Home, menú y Status con primer pase visual color inspirado en estética retro de GBA |
| Tests | Operativos | 18/18 tests deterministas: 13 Core y 5 UI |
| Build ESP32-S3 | Operativo | Firmware compilado para 16 MB flash y PSRAM octal |
| HAL Waveshare | Base parcial | Reloj, power-hold, backlight y tres botones |
| Display/touch/audio/IMU | Pendiente | Pinout y estrategia documentados; drivers aún no integrados |

Esta versión está lista para continuar el desarrollo y para demostrar arquitectura
y gameplay mínimo en Windows. No está lista todavía para una demo visual sobre la
placa física, porque el renderer ST7789 sigue siendo un stub sin salida gráfica.

El contraste completo contra el brief está en
[`docs/STATUS_REPORT.md`](docs/STATUS_REPORT.md).

## Funcionalidad disponible

- `PetState`: hunger, happiness, energy, health, age, sleeping y evolution stage.
- Acciones: feed, train, sleep, wake y reset.
- Actualización de necesidades por tiempo simulado.
- Evolución mínima por edad: Baby, Child y Adult.
- Reloj desktop acelerable x1, x10, x100 y x1000.
- Avance manual de una hora simulada.
- Edición de stats desde Debug Tools, sin setters de debug dentro del Core.
- Home, menú principal y pantalla Status en el mismo viewport lógico 240x240.
- Navegación de tres botones: siguiente, confirmar y volver.
- Idle de mascota y selector de menú temporizados, sin bloqueos.
- Panel de debug separado visualmente del dispositivo en el simulador.
- Vista placeholder original creada con primitivas del renderer.
- Firmware mínimo con tres botones orientados a navegación V-Pet.
- Versión visible en la cabecera del juego, consola desktop y Serial del ESP32.

## No incluido todavía

- Render real en ST7789 y lectura CST816T.
- Persistencia, recuperación del tiempo apagado o versionado de saves.
- Audio ES8311, IMU QMI8658 y medición/calibración de batería.
- Wi-Fi gameplay, ESP-NOW, combate, multiplayer, tienda o inventario.
- Evolución ramificada, minijuegos o aleatoriedad abstraída.
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
| `X` / `Enter` | Confirmar |
| `C` / `Backspace` | Volver |
| `Esc` | Salir |

El panel lateral de debug conserva estas herramientas de desarrollo:

| Tecla | Acción de debug |
|---|---|
| `F`, `T` | Alimentar, entrenar |
| `S`, `W` | Dormir/despertar |
| `R` | Reset |
| `1`, `2`, `3`, `4` | Tiempo x1, x10, x100, x1000 |
| `A` | Avanzar una hora simulada |
| `E` | Forzar la siguiente evolución visual |
| `Tab` | Seleccionar stat |
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
assets/placeholders/       reserva para arte original
docs/                      arquitectura, desarrollo, hardware y diagnóstico
```

## Documentación

- [`docs/README.md`](docs/README.md): índice documental.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): capas y dependencias.
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md): compilación, ejecución, tests y debug.
- [`docs/HARDWARE.md`](docs/HARDWARE.md): configuración, pinout y estado de la HAL.
- [`docs/STATUS_REPORT.md`](docs/STATUS_REPORT.md): diagnóstico para liderazgo.
- [`tests/README.md`](tests/README.md): alcance de pruebas.
- [`src/platform/waveshare_s3/README.md`](src/platform/waveshare_s3/README.md): estado real de la HAL.

## Próximo hito recomendado

Implementar `IRenderer` sobre ST7789 con Arduino_GFX, manteniendo intactos Core y
`PetView`; después integrar CST816T y validar físicamente rotación, inversión,
touch, PSRAM y batería. La persistencia versionada debería entrar antes de expandir
gameplay o evolución.
