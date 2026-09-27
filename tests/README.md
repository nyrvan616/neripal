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
.\build\windows-debug\neripal_persist_tests.exe
```

## Cobertura funcional actual

CTest ejecuta tres binarios. El rótulo vigente es 0.6.1. El recuento del cierre 0.6 (158 + 29 + 46) está en
`docs/MILESTONE_0.6.md`.

- Feed, train, sleep, wake, clean, pet y play con `CareResult` motivado.
- Cinco stats base, `needLevel()`, decay (higiene también dormida, más lenta) y
  fórmulas provisionales de salud y felicidad.
- Historial por etapa, ventana de care mistakes y entrenamiento solo con Train.
- Etapas hasta Final. Child → Adult usa `EvolutionRules` y, si hay dos salidas,
  un solo tiro de RNG que queda guardado.
- Offline comparte `stepMinute` con el juego en vivo y no suma care mistakes.
- `EvolutionNotice`: se muestra, se confirma en orden y confirmar no cambia etapa
  ni forma. La UI no ejecuta las reglas.
- Save V2, lectura de V1, migración, CRC y formas coherentes.
- Menú de ocho ítems. Status muestra affection, stimulation, etapa y forma.
- Normal no pinta aviso. Attention y Urgent comparten símbolo; Urgent es más intenso.
- Final usa el mismo placeholder que Adult.
- `DebugController::forceEvolution` no deja etapa y forma incoherentes.
- `deriveMood`, autonomía, `GameEvent` y el viewport 240x240 siguen cubiertos.

## Pendiente

- Tests contractuales para implementaciones de `IRenderer` e `IInput`.
- Stress tests de overflow de edad.
- Integración de botones/touch y tests sobre hardware.
- Cobertura y sanitizers automatizados en CI.
