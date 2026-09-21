# Diagnóstico de cierre — NeriPal 0.4.0

Fecha: 2026-09-20  
Directorio: raíz independiente `NeriPal/`  
Objetivo: mascota “viva” en host — comportamiento autónomo (Idle/Walk/Nap),
reacciones visuales y eventos one-shot, sin acoplar Core al renderer.

## Veredicto

NeriPal 0.4.0 **cumple el objetivo de vida propia en el dispositivo lógico
240x240**. Idle varía, la mascota camina sola, el cuidado dispara Eat/Happy/Tired
y los moods Dirty/Annoyed tienen one-shots. Sleep de jugador sigue hasta Wake;
Nap autónomo auto-despierta. Mood se deriva de `PetState` y no se cachea.

La versión **no es una demo visual en hardware**. El renderer ST7789 sigue
stub: el firmware compila y ejecuta el mismo loop, pero no dibuja en el LCD.

| Dimensión | Evaluación |
|---|---|
| Autonomía Idle/Walk/Nap | Verde — Core + tests de invarianza de paso |
| Reacciones de cuidado | Verde — Applied interrumpe; rechazos no (salvo Tired) |
| Mood / one-shots | Verde — `deriveMood` + flancos Dirty/Annoyed |
| RNG inyectable | Verde — `IRandom` / `FakeRandom` / seed en el root |
| Separación Core/UI | Verde — UI no elige activity ni umbrales de mood |
| Tests deterministas | Verde — Core y UI con FakeClock / FakeRandom |
| Demo física Waveshare | Amarillo — ST7789 no-op |
| Features 0.5+ | Fuera de alcance — no implementadas |

## Evidencia de validación

```text
Host C++/Win32:   tests y UI compilados (preset windows-debug)
CTest:            2/2
Unit tests Core:  78 casos
Unit tests UI:    18 casos
ESP32-S3:         PlatformIO SUCCESS (waveshare_s3_154)
Firmware:         .pio/build/waveshare_s3_154/firmware.bin
RAM estática:     19 076 / 327 680 bytes (5.8 %)
Flash app:        275 045 / 7 340 032 bytes (3.7 %)
Core audit:       sin Arduino, Win32, SDL, LVGL, GPIO o ESP32 en src/core
```

Limitación explícita: overlays, walk y reacciones validados en host y tests.
El display físico no se afirma como funcionando. Si `neripal_simulator.exe` está
abierto, el linker desktop puede fallar por archivo bloqueado; CTest no depende
de ese ejecutable.

## Cumplimiento del milestone

| Solicitud 0.4 | Estado |
|---|---|
| Idle y variantes | Cumplido — pool según mood, decide timer |
| Walk 1D | Cumplido — distancia/facing, clamp 240x240 |
| Eat / Happy / Tired / Dirty / Annoyed | Cumplido — activity + placeholder |
| Sleep visual | Cumplido — paleta + Z; Sleep de jugador intacto |
| Nap autónomo | Cumplido — energy crítica, auto-wake |
| Eventos one-shot | Cumplido — anillo fijo ActivityStarted/Finished |
| RNG determinista | Cumplido — seed desde composition root |
| Catch-up de elapsed | Cumplido — remainder + tope de transiciones |

## Fuera de 0.4 (intencional)

Enfermedad completa, medicina, muerte, persistencia, poop, día/noche, minijuegos,
multiplayer, touch gameplay, IMU, combate, ST7789 funcional, audio, ESP-NOW,
assets de terceros.

## Arquitectura

UI no llama a `Pet`. Core no dibuja. `deriveMood` es la única fuente de ánimo.
Autonomy avanza con `IClock` e `IRandom`. Simulator y firmware inyectan
`XorShift32(seed)` en el composition root; Core no elige semilla.

## Próximo hito

0.5 persistencia y tiempo offline. El bloqueo físico sigue siendo ST7789/CST816T.
