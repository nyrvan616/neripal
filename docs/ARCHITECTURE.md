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

- `core`: `Pet`, `PetState`, `CareAction`/`CareResult`, `needLevel()`,
  `CareHistory`, `EvolutionRules`, `EvolutionNotice`, etapa y `FormId` por
  separado, `Mood deriveMood(...)`, `Activity`/`IdleVariant`,
  `GameEvent`/`pollEvent()`, reglas y constantes de balance. Solo C++17
  estándar. Mood no vive en el snapshot; la actividad actual sí, como pose de
  presentación. Los bordes de actividad se consumen una vez.
- `ui`: compone las pantallas lógicas 240x240 mediante `IRenderer`; recibe un
  `PetState` inmutable y no cambia el juego. `UiController` conserva navegación,
  intención de cuidado pendiente, la copia del `EvolutionNotice` ya mostrado y
  overlays transitorios de presentación. Los niveles de necesidad salen de
  `needLevel()`, no de umbrales copiados en la vista.
- `platform`: contratos pequeños (`IClock`, `IInput`, `IRenderer`) e implementaciones
  Win32 y ESP32-S3.
- `simulator`: ensambla Core, UI y desktop; contiene herramientas exclusivamente de
  desarrollo.
- `tests`: ejecutables host. Core enlaza `neripal_core`; UI enlaza `neripal_ui`
  y, para `DebugController`, también persistencia y el reloj de escritorio;
  persistencia enlaza `neripal_persist`.

El Core no puede incluir Arduino, Win32, SDL, LVGL, GPIO ni drivers de pantalla. La
UI tampoco puede llamar acciones de `Pet`: encola un `CareAction` y presenta
snapshots. Una plataforma puede depender de UI/Core para ensamblar la aplicación,
nunca al revés.

## Cuidado

`Pet::apply(CareAction)` es el único despacho de Feed, Train, Sleep, Wake, Clean,
Pet y Play. Cada acción devuelve un `CareResult` motivado. Si el resultado no es
`Applied`, el Core no muta stats. Pet y Play no tienen reglas propias en la UI:
el menú encola la acción y el composition root llama a `apply`.

Los composition roots (simulador y firmware) hacen el mismo cableado:

1. `UiController` encola un `CareAction` al confirmar el menú.
2. El root llama `takeCareAction()` y `Pet::apply`.
3. El root pasa acción y `CareResult` a `beginCareFeedback`.
4. `PetView` traduce el enum a overlay; no relee umbrales de `Balance` ni reevalúa
   sueño o energía para decidir el motivo.
5. Si hay un `EvolutionNotice` y la UI no está mostrando uno, el root lo presenta.
   `confirmEvolutionNotice()` se llama solo después de que el usuario confirma el
   aviso ya dibujado. Esa llamada no cambia etapa ni forma y no ejecuta
   `EvolutionRules`.

Tras un `Applied`, Autonomy interrumpe la activity (`completed=false`) y entra
Eat (Feed), Happy (Train, Clean, Pet o Play), Sleep o Idle (Wake). `RejectedNoEnergy` puede
pasar a Tired si no está ya en Tired. `RejectedAsleep` conserva Sleep/Nap;
`RejectedAlreadySleeping` conserva Sleep; `RejectedAlreadyAwake` no cambia
activity.

## Necesidades e historial

Los cinco stats base viven en `PetState`. `needLevel(Need, value)` es la única
autoridad de Normal, Attention y Urgent. Salud y felicidad son stats compuestos
persistentes; su fórmula de 0.6 está en `applyNeedsStep` y es balance provisional.

`CareHistory` guarda, por etapa, mistakes, entrenamientos y minutos en banda de
salud y felicidad, más los episodios abiertos de las cinco necesidades. Ese
historial no es el stat de este minuto. Presentation no abre episodios ni suma
mistakes.

## Etapa, forma y reglas

`EvolutionStage` y `FormId` son campos distintos. La edad, comparada con las
compuertas de `evolution::`, decide si Core puede avanzar una etapa. `EvolutionRules`
elige la forma solo cuando el cruce tiene varias salidas; en 0.6 ese cruce es
Child → Adult. El resto de cruces asigna la forma coherente de esa etapa dentro
de Core. La regla ganadora se resuelve una vez. Si tiene dos salidas, consume un
`IRandom::nextBounded` y la forma queda en el snapshot.

`EvolutionNotice` es una cola propia, no un `GameEvent`. Sobrevive al save. El
anillo de eventos no. La UI muestra el aviso y el root lo confirma; no vuelve a
correr las reglas.

## Persistencia

`SaveSession` escribe siempre V2 y decodifica V1 y V2. La migración V1 ocurre en
el codec y deja una forma coherente con la etapa. `restoreSnapshot` aplica el
resultado y no llama a `EvolutionRules` ni al RNG de evolución. El detalle de
tamaños, campos y migración está en `MILESTONE_0.6.md`.

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

`Pet` recibe `IClock` e `IRandom` por constructor. `update()` y `applyOffline()`
comparten `stepMinute`: el mismo minuto simulado aplica necesidades y, si la edad
llega a la compuerta, una etapa. Las constantes están en `Balance.hpp`. Hygiene
baja despierto y, más lento, también dormido. El reloj civil no entra al Core:
`SaveSession` convierte el hueco de pared en dos deltas y llama a `applyOffline`.
Offline no avanza la ventana de care mistakes. El detalle está en `MILESTONE_0.6.md`.

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

Un reloj que retrocede se vuelve a anclar sin producir una delta negativa. El
tiempo apagado sí cuenta, con los topes `kMaxAgeOfflineMs` y `kMaxNeedsOfflineMs`,
después de que `SaveSession` valida el reloj de pared.

## Eventos

`Pet` guarda un anillo fijo de `kGameEventCapacity` (4) `GameEvent`. No hay
`std::vector` / `std::deque` ni `Step`: la presentación lee pose y elapsed del
snapshot. `pollEvent()` consume el más viejo; si el anillo está lleno se descarta
el más viejo. `reset()` / `restore()` vacían la cola para no reemitir bordes
fantasma.

El nacimiento es `GameEventKind::Hatched` al pasar de Egg a Baby en vivo. No es
una etapa. Offline no rellena ese anillo: el aviso de evolución va a
`EvolutionNotice`.

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
  política de producto ni el valor `1`. La partida sí se guarda en NVS. La
  entropía de la semilla (ADC o `esp_random`) sigue pendiente.

`nextBounded(n)` proyecta un `nextU32()` a `[0, n)`. Needs decay no consume RNG.
Autonomy lo consume en wander, facing, distancia, duración y variante de Idle, y
duración de Nap. `resolveEvolution` lo consume solo cuando la regla ganadora tiene
más de una salida, una vez, y la forma persiste. Construcción, `reset()`,
`restore()`, cargar un save, Sleep/Wake y el primer Idle usan una duración por
defecto y no muestrean la evolución otra vez. Egg y Sleep congelados tampoco
consumen autonomía. Eat/Happy/Tired/Dirty/Annoyed usan duraciones fijas.

`restore()` / `reset()` centran `x`, restauran facing y dejan Idle o Sleep de
jugador según `sleeping`; la actividad transitoria no se conserva del snapshot
entrante.

## Simulator frente a Debug Tools

El simulador es una plataforma ejecutable: crea una superficie de dispositivo,
recoge teclado, avanza el reloj y presenta frames. `DebugController` es una
herramienta de desarrollo que ofrece velocidad, cambio de los siete stats editables,
acciones de cuidado (incluidas Pet y Play) y reset. Su panel se dibuja fuera del
viewport 240x240 y no se compila en PlatformIO.

El controlador no agrega setters de debug en `Pet`. Copia el estado, cambia el
valor y usa `Pet::restore()`, que normaliza los stats. `forceEvolution` escribe la
etapa siguiente y una forma coherente por esa misma vía. No llama a
`EvolutionRules` y no es el ciclo de juego. `restore()` vacía avisos y episodios.

## Presentación

`PetView` usa solo rectángulos y texto, con una mascota placeholder original. Las
coordenadas siempre están en 240x240. Home coloca el sprite en `PetState.x` con
facing y, como máximo, las necesidades que `needLevel()` no marca Normal. Urgent
usa el mismo símbolo con más intensidad. Status muestra los números, la etapa y
el identificador de forma; no es la pantalla del sprite. Eat, Dirty, Annoyed y
Sleep/Nap se leen de `activity` / `deriveMood`, no de umbrales locales.
`idleFrame` es playback (bob); Walk usa elapsed de Core para el paso. Win32
escala esas coordenadas por un entero. El `IRenderer` de Waveshare acepta las
mismas llamadas y hoy las deja vacías, `drawText` incluido. No se agregó LVGL.

## Tests

`neripal_core_tests`, `neripal_ui_tests` y `neripal_persist_tests` son binarios
C++ sin framework externo. Core cubre cuidado, necesidades, historial, etapas,
reglas, offline y avisos. UI cubre el menú, las señales de necesidad, el aviso de
evolución y el viewport, con un renderer falso. Persistencia cubre V1, V2, la
migración y el snapshot. CTest solo descubre y ejecuta los binarios. Ninguno
depende de SDL, Arduino ni hardware. El recuento del cierre 0.6 está en
`MILESTONE_0.6.md`.

## Añadir una plataforma

1. Implementar únicamente los contratos que use (`IClock`, `IInput`, `IRenderer`).
2. Crear un composition root equivalente a `simulator/main.cpp` o al `main.cpp` de
   Waveshare, inyectando `XorShift32(seed)` en `Pet` e incluyendo
   `takeCareAction()` → `Pet::apply` → `beginCareFeedback`. La seed la elige el
   root, no el Core.
3. Enlazar Core y UI sin introducir `#ifdef` de la plataforma dentro de ellos.
4. Agregar un smoke build y, cuando corresponda, pruebas contractuales del renderer.
