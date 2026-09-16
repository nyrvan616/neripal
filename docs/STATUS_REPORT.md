# Diagnóstico de entrega — NeriPal 0.1.0

Fecha del diagnóstico: 2026-09-16  
Directorio evaluado: raíz del repositorio independiente `NeriPal/`  
Objetivo evaluado: primera base arquitectónica, simulador, tests y target mínimo
para Waveshare ESP32-S3-Touch-LCD-1.54.

## Veredicto ejecutivo

NeriPal 0.1.0 **cumple el objetivo de base de desarrollo**: el Core está separado
del hardware, el simulador funciona en Windows, el tiempo es controlable, los tests
son deterministas y el firmware ESP32-S3 compila sin placa conectada.

La versión **no es todavía una demo funcional en el dispositivo**. La HAL de la
Waveshare es intencionalmente mínima y el renderer físico no dibuja: display,
touch, audio, IMU y batería siguen pendientes de integración y prueba real. Esto no
contradice el alcance original, que permitía dejar esos periféricos sin implementar
en esta etapa, pero debe quedar explícito ante cualquier expectativa de demo física.

| Dimensión | Evaluación |
|---|---|
| Base arquitectónica | Verde — implementada y compilada en host y ESP32 |
| Demo de gameplay mínimo en Windows | Verde — ejecutable y smoke-tested |
| Tests deterministas | Verde — 12/12 |
| Preparación de firmware | Verde — `firmware.bin` generado |
| Demo visual en Waveshare | Amarillo — bloqueada por renderer ST7789 pendiente |
| Preparación para producción | Rojo — fuera del alcance y aún inmadura |

## Evidencia de validación

Validaciones ejecutadas en el entorno local:

```text
Host C++/Win32: compilación correcta
CTest:          1/1 target correcto
Unit tests:     12/12 casos correctos
Simulator:      proceso iniciado y mantenido en ejecución durante smoke test
Runtime version: UI, consola desktop y Serial muestran NeriPal 0.1.0
ESP32-S3:       PlatformIO SUCCESS
Firmware:       .pio/build/waveshare_s3_154/firmware.bin generado
RAM estática:   18 932 / 327 680 bytes (5.8 %)
Flash app:      267 261 / 7 340 032 bytes (3.6 %)
Core audit:     sin Arduino, Win32, SDL, LVGL, GPIO o ESP32
```

Toolchain usada: GCC 16.1.0, CMake 4.4.1, Ninja, PlatformIO Core 6.2.0 y
`espressif32@6.12.0`.

## Cumplimiento punto por punto

Leyenda:

- **Cumplido**: implementado y validado localmente.
- **Parcial**: existe una base útil, pero no toda la capacidad sugerida.
- **Pendiente**: estaba solicitado o era deseable y todavía no pudo completarse.
- **Fuera de alcance**: el propio brief indicó no implementarlo en esta etapa.

### Proyecto, stack y separación

| Solicitud | Estado | Evidencia / observación |
|---|---|---|
| Proyecto NeriPal autónomo | Cumplido | CMake, PlatformIO, tests, scripts y documentación resuelven desde la raíz propia |
| C++ / CMake / PlatformIO / Arduino | Cumplido | Builds host y firmware separados |
| Resolución lógica 240x240 | Cumplido | `IRenderer` fija 240x240; `PetView` usa esas coordenadas |
| Game Core sin hardware | Cumplido | `src/core` usa C++ estándar e `IClock` |
| UI separada del juego | Cumplido | `PetView` recibe un `PetState` inmutable y dibuja por `IRenderer` |
| Platform/HAL separada | Cumplido | `IClock`, `IInput`, `IRenderer` y dos implementaciones de plataforma |
| Simulator separado | Cumplido | Composition root propio en `simulator/main.cpp` |
| Versión visible y consultable | Cumplido | Fuente compartida `Version.hpp`; cabecera del juego, título y consola desktop (`--version`), y Serial ESP32 |
| Debug separado de producción | Cumplido | `DebugController` solo participa en CMake desktop; PlatformIO no lo compila |
| Tests fuera de producción | Cumplido | Ejecutable separado en `tests/unit/core` |
| Sin LVGL innecesario | Cumplido | Desktop usa Win32/GDI y firmware no incorpora librerías gráficas todavía |

### Game Core y mecánica

| Solicitud | Estado | Evidencia / observación |
|---|---|---|
| Pet y PetState/Stats | Cumplido | `Pet`, `PetState` y límites centralizados |
| Hunger 0 lleno / 100 hambre máxima | Cumplido | Semántica documentada y respetada |
| Happiness, energy, health 0..100 | Cumplido | Todas las entradas y acciones se normalizan |
| Age con unidad interna adecuada | Cumplido | Edad interna en milisegundos, expuesta visualmente en minutos |
| `feed()` | Cumplido | Reduce hunger y eleva ligeramente happiness |
| `train()` | Cumplido | Consume energy, aumenta hunger/happiness/health |
| `sleep()` / `wake()` | Cumplido | Estado explícito con recuperación temporal de energía |
| `update()` y degradación temporal | Cumplido | Pasos deterministas de un minuto simulado |
| Balance fácil de cambiar | Cumplido | Constantes en `Balance.hpp` |
| Evolución | Cumplido mínimo | Baby → Child → Adult exclusivamente por edad; no hay árboles ni condiciones complejas |
| Food/Training/Sleep como módulos futuros | Parcial | Existen como acciones/estado, no como subsistemas independientes |
| Combat/Inventory/Social/Multiplayer/Persistence preparados | Parcial conceptual | Las dependencias permiten agregarlos, pero aún no existen contratos o módulos vacíos |
| Aleatoriedad determinista futura | Pendiente permitido | No existe `IRandom`; el brief permitía diferirlo hasta que fuera necesario |

### Tiempo

| Solicitud | Estado | Evidencia / observación |
|---|---|---|
| Core sin `millis()` o reloj de Windows | Cumplido | Solo conoce `IClock` |
| Reloj real ESP32 | Cumplido | `Esp32Clock` usa `esp_timer_get_time()` dentro de la plataforma |
| Reloj real desktop | Cumplido | `ScaledClock` usa `steady_clock` |
| Reloj falso para tests | Cumplido | `FakeClock` avanza explícitamente |
| Tiempo x1/x10/x100/x1000 | Cumplido | Teclas 1–4 y `DebugController` |
| Avance manual | Cumplido | Tecla `A` suma una hora simulada |
| Tests sin esperas reales | Cumplido | Todos los casos temporales usan `FakeClock` |

### UI, simulador y Debug Tools

| Solicitud | Estado | Evidencia / observación |
|---|---|---|
| Ver mascota y stats | Cumplido | Placeholder original, cuatro barras, edad, etapa y sueño |
| Feed/train/sleep/wake/reset | Cumplido | Teclado y DebugController |
| Set Hunger/Happiness/Energy/Health | Cumplido | API exacta en DebugController; teclado ajusta ±5 o extremos |
| Reusar presentación entre desktop y placa | Cumplido arquitectónico | Ambos composition roots llaman a `PetView` |
| Placeholders sin assets de terceros | Cumplido | Primitivas geométricas propias |
| Menús | Parcial | No hay sistema de menú todavía; el brief admitía una UI extremadamente simple |
| Animaciones | Parcial | Solo feedback de sueño; no existe timeline/sistema de animación |
| Input sintético | Cumplido básico | Teclado normalizado; no hay touch/mouse sintético |
| Ventana Windows sin ESP32 | Cumplido | Ejecutable Win32 enlazado y smoke-tested |

### Tests

| Caso mínimo solicitado | Estado |
|---|---|
| Feed reduce hunger | Cumplido |
| Hunger nunca baja de 0 | Cumplido |
| Train consume energy | Cumplido |
| Sleep recupera energy | Cumplido |
| Energy nunca supera 100 | Cumplido |
| Happiness permanece en rango | Cumplido mediante normalización |
| Health permanece en rango | Cumplido mediante normalización |
| Tiempo controlado incrementa hunger | Cumplido |
| Tiempo controlado modifica energy | Cumplido para sueño y vigilia |
| Tiempo sin espera real | Cumplido |

Además se prueban snapshots inválidos y evolución por edad. No hay aún cobertura,
sanitizers, property tests, pruebas de UI ni HAL.

### Target Waveshare ESP32-S3

| Solicitud | Estado | Evidencia / observación |
|---|---|---|
| Target PlatformIO inicial | Cumplido | `waveshare_s3_154`, Arduino y `espressif32@6.12.0` |
| 16 MB flash / 8 MB PSRAM | Cumplido en configuración | `qio_opi`, flash 16 MB, `BOARD_HAS_PSRAM`; falta confirmar físicamente |
| Tabla de particiones | Cumplido | Dos slots OTA de 7 MB, NVS, SPIFFS y coredump |
| Compilación sin hardware | Cumplido | `firmware.bin` generado |
| `WaveshareS3Platform` | Cumplido como esqueleto | Reloj, power-hold, backlight, botones e interfaces |
| ST7789 funcional | Pendiente permitido | Métodos de render son no-op |
| CST816T funcional | Pendiente permitido | Pinout documentado, driver no integrado |
| Botones | Parcial | Lectura edge básica; falta debounce y prueba física |
| QMI8658 / ES8311 / batería | Pendiente permitido | Documentados, no implementados |
| Audio | Fuera de alcance | Expresamente diferido por el brief |
| Verificación física | Pendiente externo | No había placa conectada |

PlatformIO muestra el nombre descriptivo del board genérico como “N8, No PSRAM”,
pero el entorno sobrescribe flash a 16 MB y selecciona memoria `qio_opi`. La
detección real de 8 MB PSRAM debe verificarse en boot sobre la placa antes de
cerrar el port.

### Documentación y Git

| Solicitud | Estado | Evidencia / observación |
|---|---|---|
| Documentar arquitectura | Cumplido | `ARCHITECTURE.md` |
| Documentar desarrollo | Cumplido | `DEVELOPMENT.md` contiene build, tests, ejecución y debug |
| Documentar hardware | Cumplido | `HARDWARE.md` registra target, pinout y estado real de la HAL |
| Repositorio Git independiente | Cumplido | `.git` inicializado con rama `main` |
| `.gitignore` adecuado | Cumplido | Ignora CMake, PlatformIO, binarios, IDE y temporales |
| Baseline inicial | Cumplido | Existe un commit inicial local para revisión antes de publicar |

## Elementos expresamente no solicitados en esta etapa

La ausencia de los siguientes elementos es intencional y no debe calificarse como
incumplimiento de la versión 0.1.0:

- Recreación o port de aplicaciones ajenas.
- Wi-Fi como mecánica de alimentación.
- ESP-NOW, multiplayer, combate o social.
- Economía, tienda o inventario complejo.
- Audio e IMU funcionales.
- Assets o contenido de terceros.
- Evolución compleja o minijuegos.

## Riesgos y deuda técnica

1. **Renderer físico vacío.** Encender backlight sin inicializar/limpiar ST7789 no
   produce una pantalla útil; es el bloqueo principal para una demo de hardware.
2. **Perfil físico sin confirmar.** Flash y PSRAM compilan con overrides, pero deben
   validarse en boot con la placa exacta.
3. **Botones sin debounce.** El edge polling es suficiente para compilar, no para
   afirmar calidad de interacción; GPIO0 además merece prueba durante reset/boot.
4. **Tiempo offline no definido.** `IClock` resuelve simulación, pero aún no existe
   política de envejecimiento mientras el dispositivo está apagado.
5. **Actualización lineal por minuto.** `Pet::update()` itera cada minuto acumulado;
   antes de soportar largos periodos offline conviene agregar cálculo por lotes o un
   límite controlado.
6. **Persistencia ausente.** `restore()` es la frontera preparada, pero faltan
   schema/versionado, serialización y adaptadores desktop/NVS.
7. **Cobertura limitada al Core.** No se prueban dibujo, input, HAL, particiones ni
   comportamiento del firmware.
8. **Desktop solo Windows.** Win32/GDI cumple el entorno pedido, pero no es portable.
9. **Sin CI.** La validación fue local; todavía no hay workflow que impida regresiones.

## Recomendación de aceptación

Aceptar esta versión como **Milestone 1: Architecture and Host Simulation** con
estas condiciones explícitas:

- No presentarla como firmware visual terminado.
- Abrir el siguiente milestone con ST7789, CST816T y pruebas físicas como criterios
  de aceptación obligatorios.

## Próximos hitos recomendados

1. **Display smoke test:** Arduino_GFX + ST7789, color sólido, orientación e inversión.
2. **Renderer completo:** mapear las primitivas de `IRenderer` y mostrar `PetView`.
3. **Touch y botones:** CST816T, transform 240x240, debounce y acciones unificadas.
4. **Diagnóstico de placa:** PSRAM, flash, batería y lectura de IDs I2C.
5. **Persistencia:** snapshot versionado y decisión de tiempo offline.
6. **UI V-Pet:** controlador de pantallas, menú y animación no bloqueante.
7. **Calidad automática:** CI host + firmware, warnings estrictos y cobertura Core.

## Comandos de reproducción

Ejecutar desde la raíz de la carpeta independiente `NeriPal/`:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
.\build\windows-debug\neripal_simulator.exe
pio run -e waveshare_s3_154
```

La guía completa y portable está en [`DEVELOPMENT.md`](DEVELOPMENT.md).
