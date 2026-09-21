# Tests de NeriPal

Los tests cubren Game Core y controladores de UI sin enlazar Arduino, Win32, SDL,
LVGL ni drivers. `FakeClock` permite avanzar el tiempo sin esperas reales;
`FakeRandom` entrega una cola de muestras `uint32_t` (agotarla falla el caso). El
test de UI usa un renderer falso sin abrir ventanas.

## Ejecutar

Ejecutar desde la raíz independiente de NeriPal, no desde `tests/`:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Para ver cada caso:

```powershell
.\build\windows-debug\neripal_core_tests.exe
.\build\windows-debug\neripal_ui_tests.exe
```

## Cobertura funcional actual

- Feed, train, sleep, wake y clean con `CareResult` motivado.
- Hygiene: clamp, decay despierto, Clean y efecto sobre felicidad/salud.
- `Pet::apply` despacha `CareAction` sin duplicar reglas.
- Restauración normaliza un snapshot inválido, incluida hygiene.
- `XorShift32` produce una secuencia fija por semilla; `FakeRandom` reproduce
  valores scripted y falla al agotarse; `nextBounded` usa una muestra por llamada.
- Cuidado no consume RNG en stats. `update()` despierto puede Walk/Idle/Nap y sí
  consume muestras; Egg y Sleep de jugador congelan el timer y no muestrean.
- Evolución Egg → Baby → Child → Adult con edad controlada.
- Menú 2x3, SLEEP/WAKE según snapshot, STATUS con barra HYG.
- Overlay de feedback aparece y caduca con tiempo controlado.
- Rechazo muestra el label del `CareResult` (ASLEEP/TIRED/RESTING/AWAKE).
- `deriveMood` sigue una tabla de prioridad (Resting/Tired/Dirty/Annoyed/Happy/Calm);
  Egg no pisa el mood; `PetState` no cachea el valor. El banner de Home/Status
  traduce el enum (`DIRTY`/`TIRED`/`HAPPY`/`ANNOYED`/`RESTING`/`CALM`) o `WAITING`.
- Idle/Walk/Nap: snapshot de `Activity`, decide timer, wander por distancia,
  catch-up con remainder y tope de transiciones; el mismo elapsed total produce el
  mismo `activity`/`x`/`facing`/`IdleVariant` aunque se parta en pasos distintos.
- Care Applied → Eat/Happy/Sleep/Idle; `RejectedNoEnergy` → Tired; el resto de
  rechazos no cambia activity. Flancos Dirty/Annoyed son one-shot.
- `GameEvent`: `ActivityStarted`/`ActivityFinished` sin `Step`; `pollEvent()`
  consume una vez; el anillo embebido no crece y, lleno, descarta el más viejo.
- Eat/Dirty placeholder y walk en extremos permanecen dentro de 240x240.
- Ausencia de etiquetas de debug en las vistas del dispositivo.

## Pendiente

- Tests contractuales para implementaciones de `IRenderer` e `IInput`.
- Stress tests de overflow de edad.
- Integración de botones/touch y tests sobre hardware.
- Cobertura y sanitizers automatizados en CI.
