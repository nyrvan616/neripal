# WaveshareS3Platform

Estado de la HAL para Waveshare ESP32-S3-Touch-LCD-1.54. El cierre de juego
vigente es 0.6; esta página describe solo la plataforma.

## Implementado

- `Esp32Clock` monotónico mediante `esp_timer_get_time()`.
- GPIO2 como battery power-hold y GPIO46 como backlight activo alto.
- Botones activos bajos en GPIO0, GPIO5 y GPIO4.
- Mapeo provisional V-Pet: botón 1 siguiente, botón 2 confirmar y botón 3 volver.
- Identificación de versión por Serial; `PetView` muestra la misma versión en UI.
- Contratos `IInput` e `IRenderer` completos a nivel de compilación.
- `XorShift32` inyectado en el composition root con un placeholder de seed
  (`kFirmwareRngSeedPlaceholder`). No es política de producto ni el valor `1`.
  La entropía real (ADC o `esp_random`) sigue pendiente.
- Save en NVS: `NvsSaveStorage`, namespace `neripal`, claves `slot0` y `slot1`.
  Escribe V2 y lee V1. `Esp32WallClock` alimenta el tiempo offline.
- Target PlatformIO N16R8: flash de 16 MB, PSRAM octal y particiones OTA.

## Stub deliberado

`beginFrame`, `fillRect`, `drawRect`, `drawText` y `endFrame` están vacíos. El
firmware compila y ejecuta Core, save y UI, aunque no muestra nada en el LCD.

## Pendiente de implementación física

- ST7789 por SPI2: SCK38, MOSI39, DC45, CS21, RESET40, BL46.
- CST816T compartiendo I2C en SDA42/SCL41, RESET47 e IRQ48.
- QMI8658, ES8311/ES7210, ADC de batería y SD-MMC.
- Debounce robusto de botones y comportamiento de GPIO0 durante boot.
- Comprobación de PSRAM, orientación, inversión, touch transform y consumo.

Consultar [`../../../docs/HARDWARE.md`](../../../docs/HARDWARE.md)
para el pinout y la secuencia recomendada.

Los comandos de build, upload, monitor y debug se ejecutan desde la raíz de
NeriPal y están documentados en
[`../../../docs/DEVELOPMENT.md`](../../../docs/DEVELOPMENT.md#firmware-esp32-s3).
