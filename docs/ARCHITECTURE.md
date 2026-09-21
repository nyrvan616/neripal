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

- `core`: `Pet`, `PetState`, `CareAction`/`CareResult`, `Mood deriveMood(...)`,
  `Activity`/`IdleVariant`, `GameEvent`/`pollEvent()`, reglas y constantes de
  balance. Solo C++17 estándar. Mood no vive en el snapshot; la actividad actual
  sí, como pose de presentación. Los bordes de actividad se consumen una vez.
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

Tras un `Applied`, Autonomy interrumpe la activity (`completed=false`) y entra
Eat (Feed), Happy (Train/Clean), Sleep o Idle (Wake). `RejectedNoEnergy` puede
pasar a Tired si no está ya en Tired. `RejectedAsleep` conserva Sleep/Nap;
`RejectedAlreadySleeping` conserva Sleep; `RejectedAlreadyAwake` no cambia
activity.

## Mood

`Mood deriveMood(const PetState&)` es la única fuente de ánimo. El valor no se
guarda en `PetState` ni se cachea en Core: se calcula al leer el snapshot. La
prioridad es Resting, Tired, Dirty, Annoyed, Happy, Calm. Egg no es un mood; la
UI muestra `WAITING` y no usa el mood de cuidado.

`PetView` traduce el enum a label. No relee umbrales de `Balance` para decidir
ánimo. Autonomy recuerda el mood anterior solo como detección de flanco interno
(Dirty/Annoyed), comparándolo contra `deriveMood` del snapshot actual.
`restore()` / `reset()` reinician esa memoria para no disparar one-shots fantasma.

## Autonomía

Un solo `Activity` activo: Idle, Walk, Eat, Happy, Annoyed, Tired, Dirty, Sleep,
Nap. El snapshot en `PetState` expone pose (`activity`, `idleVariant`, `x`,
`facing`, elapsed/duration). Mood no forma parte del snapshot.

Al completar Idle, el selector (prioridad fija) elige Nap si la energía está
crítica, Walk con probabilidad ponderada por mood/energía, o un nuevo Idle. Tras
Walk se sesga a Idle. Egg y Sleep de jugador congelan el timer. Nap no está
congelado: dura 8–20 s o hasta `kNapWakeEnergy`, luego un Idle breve evita
re-entrar en Nap en el mismo instante.

Walk es 1D: presupuesto de distancia, 25 ms/px, X acotado al Home. El elapsed
grande se reparte con remainder; `kMaxActivityTransitionsPerUpdate` evita bucles
y no descarta tiempo.

## Tiempo

`Pet` recibe `IClock` e `IRandom` por constructor. `update()` compara timestamps monotónicos,
acumula edad exacta y aplica reglas en pasos deterministas de un minuto simulado.
Las constantes están centralizadas en `Balance.hpp`. Hygiene baja solo despierto.

Autonomy avanza con el mismo elapsed: acumula tiempo sobre la actividad, y si esta
termina el resto entra en la siguiente. Un tope
(`kMaxActivityTransitionsPerUpdate`) evita bucles; el sobrante se guarda para el
próximo `update` y no se descarta. Egg y Sleep de jugador congelan el decide timer
sin volcar ese tiempo al remainder. Nap sí avanza. Needs decay es independiente
de las transiciones.

- ESP32: `Esp32Clock` usa el temporizador monotónico de ESP-IDF detrás de la HAL.
- Desktop: `ScaledClock` usa `steady_clock` y cambia entre x1/x10/x100/x1000 sin
  discontinuidades.
- Tests: `FakeClock` avanza explícitamente; nunca duerme el proceso.

Un reloj que retrocede se vuelve a anclar sin producir una delta negativa. En una
etapa de persistencia habrá que decidir si el tiempo apagado cuenta y convertir el
reloj civil a una delta validada antes de restaurar el Core.

## Eventos

`Pet` guarda un anillo fijo de `kGameEventCapacity` (4) `GameEvent`. No hay
`std::vector` / `std::deque` ni `Step`: la presentación lee pose y elapsed del
snapshot. `pollEvent()` consume el más viejo; si el anillo está lleno se descarta
el más viejo. `reset()` / `restore()` vacían la cola para no reemitir bordes
fantasma.

Hoy se emiten `ActivityStarted` y `ActivityFinished`:

- Re-elección de Idle/Walk/Nap al vencer el episodio: Finished con `completed=true`,
  luego Started.
- Interrupciones de cuidado Applied y flancos Dirty/Annoyed: Finished con
  `completed=false`, luego Started. Los rechazos Asleep / AlreadySleeping /
  AlreadyAwake no emiten; `RejectedNoEnergy` puede emitir Tired.
- Construcción, `reset()` y `restore()` no emiten; Egg y Sleep congelados tampoco.

## Aleatoriedad

`Pet` recibe `IRandom` por constructor, del mismo modo que recibe `IClock`. El Core
no elige semilla ni llama a `rand()` / `std::random_device`. `XorShift32(seed)` es
la implementación portable; la seed llega siempre del composition root.

- Tests: `FakeRandom` entrega una cola de `uint32_t`. Agotar la cola falla el test;
  no envuelve ni inventa valores.
- Simulator: seed de desarrollo en `simulator/main.cpp`.
- Firmware: placeholder documentado en el composition root de Waveshare. No es
  política de producto ni el valor `1`. La fuente real (ADC, `esp_random`, NVS)
  queda para cuando exista HAL/persistencia.

`nextBounded(n)` proyecta un `nextU32()` a `[0, n)`. Needs decay no consume RNG.
Autonomy es el único consumidor: wander, facing, distancia, duración y variante
de Idle, duración de Nap. Construcción, `reset()`, `restore()`, Sleep/Wake y el
primer Idle usan una duración por defecto y no muestrean. Egg y Sleep congelados
tampoco consumen. Eat/Happy/Tired/Dirty/Annoyed usan duraciones fijas.

`restore()` / `reset()` centran `x`, restauran facing y dejan Idle o Sleep de
jugador según `sleeping`; la actividad transitoria no se conserva del snapshot
entrante.

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
coordenadas siempre están en 240x240. Home coloca el sprite en `PetState.x` con
facing; Status usa un thumbnail fijo. Eat, Dirty, Annoyed y Sleep/Nap se leen de
`activity` / `deriveMood`, no de umbrales locales. `idleFrame` es playback (bob);
Walk usa elapsed de Core para el paso. Win32 escala esas coordenadas por un
entero y la futura implementación ST7789 podrá consumir las mismas llamadas. No
se agregó LVGL: para esta interfaz aumentaría dependencias y dividiría el flujo
de render entre host y dispositivo sin aportar widgets necesarios.

## Tests

`neripal_core_tests` y `neripal_ui_tests` son binarios C++ sin framework externo.
El primero comprueba acciones, `CareResult`, límites, degradación temporal, RNG
(`XorShift32`, `FakeClock`, `FakeRandom`), la tabla de `deriveMood`, Idle/Walk/Nap,
reacciones de cuidado, flancos Dirty/Annoyed, invarianza de catch-up frente al
tamaño de paso y `pollEvent()` (consumo único, anillo fijo que descarta el más
viejo); el segundo usa un renderer falso para comprobar navegación, overlays,
banners de mood, Eat/Dirty placeholder y límites del viewport al caminar. CTest
solo descubre/ejecuta los binarios; ninguno depende de SDL, Arduino ni hardware.

## Añadir una plataforma

1. Implementar únicamente los contratos que use (`IClock`, `IInput`, `IRenderer`).
2. Crear un composition root equivalente a `simulator/main.cpp` o al `main.cpp` de
   Waveshare, inyectando `XorShift32(seed)` en `Pet` e incluyendo
   `takeCareAction()` → `Pet::apply` → `beginCareFeedback`. La seed la elige el
   root, no el Core.
3. Enlazar Core y UI sin introducir `#ifdef` de la plataforma dentro de ellos.
4. Agregar un smoke build y, cuando corresponda, pruebas contractuales del renderer.
