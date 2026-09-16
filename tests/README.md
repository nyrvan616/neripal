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

- Feed reduce hunger y respeta el límite inferior.
- Train consume energía.
- Sleep recupera energía sin superar 100.
- Happiness y health son normalizados dentro de rango.
- El tiempo controlado incrementa hunger y modifica energy según sueño/vigilia.
- La edad avanza sin `sleep()` real.
- Restauración normaliza un snapshot inválido.
- La evolución Baby → Child → Adult usa edad controlada.
- Navegación Home → menú → Status y retorno.
- Wrap de selección y transición interpolada de menú.
- Idle de mascota con tiempo controlado.
- Render de cada pantalla dentro del viewport 240x240.
- Ausencia de etiquetas de debug en las vistas del dispositivo.

## Pendiente

- Tests contractuales para implementaciones de `IRenderer` e `IInput`.
- Stress tests de deltas temporales grandes y overflow.
- Integración de botones/touch y tests sobre hardware.
- Cobertura y sanitizers automatizados en CI.
