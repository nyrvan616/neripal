# Tests de NeriPal

Los tests cubren Game Core y controladores de UI sin enlazar Arduino, Win32, SDL,
LVGL ni drivers. `FakeClock` permite avanzar el tiempo sin esperas reales; el test
de UI usa un renderer falso sin abrir ventanas.

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
- Evolución Egg → Baby → Child → Adult con edad controlada.
- Menú 2x3, SLEEP/WAKE según snapshot, STATUS con barra HYG.
- Overlay de feedback aparece y caduca con tiempo controlado.
- Rechazo muestra el label del `CareResult` (ASLEEP/TIRED/RESTING/AWAKE).
- Render de cada pantalla y overlays dentro del viewport 240x240.
- Ausencia de etiquetas de debug en las vistas del dispositivo.

## Pendiente

- Tests contractuales para implementaciones de `IRenderer` e `IInput`.
- Stress tests de deltas temporales grandes y overflow.
- Integración de botones/touch y tests sobre hardware.
- Cobertura y sanitizers automatizados en CI.
