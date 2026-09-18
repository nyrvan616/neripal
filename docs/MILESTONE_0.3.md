# Diagnóstico de cierre — NeriPal 0.3.0

Fecha: 2026-09-17  
Directorio: raíz independiente `NeriPal/`  
Objetivo: loop básico de crianza jugable (Feed, Train, Sleep, Wake, Clean),
Hygiene, degradación temporal, feedback visual y tests deterministas del Core/UI.

## Veredicto

NeriPal 0.3.0 **cumple el objetivo de crianza básica en el dispositivo lógico
240x240**. Las acciones de cuidado viven en el menú del juego, no solo en Debug.
El Core comunica rechazos con `CareResult`; la presentación no duplica umbrales.
Hygiene degrada en vigilia y se recupera con Clean.

La versión **no es una demo visual en hardware**. El renderer ST7789 sigue siendo
stub: el firmware compila y ejecuta el mismo loop, pero no dibuja en el LCD.

| Dimensión | Evaluación |
|---|---|
| Loop de cuidado en menú 240x240 | Verde — Feed/Train/Sleep/Wake/Clean + Status |
| Hygiene y balance temporal | Verde — Core + tests |
| Feedback visual por acción/rechazo | Verde — overlays host/tests; no validado en LCD |
| Debug separado del viewport | Verde — panel desktop 300px; firmware sin DebugController |
| Tests deterministas | Verde — Core y UI con FakeClock / renderer falso |
| Demo física Waveshare | Amarillo — ST7789 no-op |
| Features 0.4+ | Fuera de alcance — no implementadas |

## Evidencia de validación

```text
Host C++/Win32:   compilación correcta (preset windows-debug)
CTest:            2/2
Unit tests Core:  28 casos
Unit tests UI:    13 casos
ESP32-S3:         PlatformIO SUCCESS (waveshare_s3_154)
Firmware:         .pio/build/waveshare_s3_154/firmware.bin
Core audit:       sin Arduino, Win32, SDL, LVGL, GPIO o ESP32 en src/core
```

Limitación explícita: overlays y menú validados en host y tests. El display físico
no se afirma como funcionando.

## Cumplimiento del milestone

| Solicitud 0.3 | Estado |
|---|---|
| Alimentar / entrenar / dormir / despertar / limpiar | Cumplido en Core y menú de dispositivo |
| Felicidad, energía, salud | Ya existían; se mantienen y se ligan a hygiene |
| Higiene | Cumplido: stat, decay despierto, Clean, barra HYG |
| Feedback visual por acción | Cumplido: overlays 900 ms según `CareResult` |
| Balance básico | Cumplido: constantes en `Balance.hpp` |
| Debug fuera de 240x240 | Cumplido |
| Tests Core deterministas | Cumplido |

## Fuera de 0.3 (intencional)

Idle autónomo, sueño espontáneo, enfermedad, eventos one-shot, poop, tipos de
comida, noche, persistencia, ST7789 funcional, audio, ESP-NOW, combate, inventario,
assets de terceros.

## Arquitectura

UI no llama a `Pet`. Core no dibuja. Simulator y firmware usan el mismo despacho
`takeCareAction` → `Pet::apply` → `beginCareFeedback`.

## Próximo hito

No abrir 0.4 (mascota “viva”) como requisito de hardware. El bloqueo físico sigue
siendo ST7789/CST816T. El roadmap 0.4 puede comenzar en host cuando se decida
comportamiento autónomo, sin copiar ese trabajo a esta entrega.
