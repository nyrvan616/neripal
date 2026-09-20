# Arquitectura de NeriPal

## Capas y dirección de dependencias

NeriPal aplica una regla simple: las decisiones del juego no conocen el sistema que
las ejecuta.

```text
Simulator / WaveshareS3Platform
              |
              v
       UI / Presentation
              |
              v
          Game Core
```

- `core`: `Pet`, `PetState`, `CareAction`/`CareResult`, reglas y constantes de
  balance. Solo C++17 estándar.
- `ui`: compone las pantallas lógicas 240x240 mediante `IRenderer`; recibe un
  `PetState` inmutable y no cambia el juego. `UiController` conserva navegación,
  intención de cuidado pendiente y overlays transitorios de presentación.
- `platform`: contratos pequeños (`IClock`, `IInput`, `IRenderer`) e implementaciones
  Win32 y ESP32-S3.
- `simulator`: ensambla Core, UI y desktop; contiene herramientas exclusivamente de
  desarrollo.
- `tests`: ejecutables host. Core enlaza `neripal_core`; UI enlaza `neripal_ui`.

El Core no puede incluir Arduino, Win32, SDL, LVGL, GPIO ni drivers de pantalla. La
UI tampoco puede llamar acciones de `Pet`: encola un `CareAction` y presenta
snapshots. Una plataforma puede depender de UI/Core para ensamblar la aplicación,
nunca al revés.

## Cuidado

`Pet::apply(CareAction)` es el único despacho de Feed, Train, Sleep, Wake y Clean.
Cada acción devuelve un `CareResult` motivado. Si el resultado no es `Applied`, el
Core no muta stats.

Los composition roots (simulador y firmware) hacen el mismo cableado:

1. `UiController` encola un `CareAction` al confirmar el menú.
2. El root llama `takeCareAction()` y `Pet::apply`.
3. El root pasa acción y `CareResult` a `beginCareFeedback`.
4. `PetView` traduce el enum a overlay; no relee umbrales de `Balance` ni reevalúa
   sueño o energía para decidir el motivo.

## Tiempo

`Pet` recibe `IClock` por constructor. `update()` compara timestamps monotónicos,
acumula edad exacta y aplica reglas en pasos deterministas de un minuto simulado.
Las constantes están centralizadas en `Balance.hpp`. Hygiene baja solo despierto.

- ESP32: `Esp32Clock` usa el temporizador monotónico de ESP-IDF detrás de la HAL.
- Desktop: `ScaledClock` usa `steady_clock` y cambia entre x1/x10/x100/x1000 sin
  discontinuidades.
- Tests: `FakeClock` avanza explícitamente; nunca duerme el proceso.

Un reloj que retrocede se vuelve a anclar sin producir una delta negativa. En una
etapa de persistencia habrá que decidir si el tiempo apagado cuenta y convertir el
reloj civil a una delta validada antes de restaurar el Core.

## Simulator frente a Debug Tools

El simulador es una plataforma ejecutable: crea una superficie de dispositivo,
recoge teclado, avanza el reloj y presenta frames. `DebugController` es una
herramienta de desarrollo que ofrece velocidad, cambio de stats (incluidos hygiene),
acciones de cuidado y reset; su panel se dibuja fuera del viewport 240x240 y no se
compila en PlatformIO.

El controlador no requiere `setHungerForDebug()` en `Pet`. Copia el snapshot,
cambia el valor y usa `Pet::restore()`, que es una frontera legítima para la futura
persistencia y además normaliza invariantes.

## Presentación

`PetView` usa solo rectángulos y texto, con una mascota placeholder original. Las
coordenadas siempre están en 240x240. Home, menú y Status reciben un `UiState` con
pantalla, selección, fase de animación y feedback de cuidado; no reciben controles
de debug. Win32 escala esas coordenadas por un entero y la futura implementación
ST7789 podrá consumir las mismas llamadas. No se agregó LVGL: para esta interfaz
aumentaría dependencias y dividiría el flujo de render entre host y dispositivo sin
aportar widgets necesarios.

## Tests

`neripal_core_tests` y `neripal_ui_tests` son binarios C++ sin framework externo.
El primero comprueba acciones, `CareResult`, límites y degradación temporal con
`FakeClock`; el segundo usa un renderer falso para comprobar navegación, overlays y
límites del viewport. CTest solo descubre/ejecuta los binarios; ninguno depende de
SDL, Arduino ni hardware.

Los próximos módulos con azar deberán recibir una interfaz de fuente aleatoria,
del mismo modo que el tiempo recibe `IClock`, antes de probar evolución, combate o
minijuegos.

## Añadir una plataforma

1. Implementar únicamente los contratos que use (`IClock`, `IInput`, `IRenderer`).
2. Crear un composition root equivalente a `simulator/main.cpp` o al `main.cpp` de
   Waveshare, incluyendo `takeCareAction()` → `Pet::apply` → `beginCareFeedback`.
3. Enlazar Core y UI sin introducir `#ifdef` de la plataforma dentro de ellos.
4. Agregar un smoke build y, cuando corresponda, pruebas contractuales del renderer.
