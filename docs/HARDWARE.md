# Hardware de NeriPal

## Target

El firmware apunta a Waveshare ESP32-S3-Touch-LCD-1.54 mediante el entorno
PlatformIO `waveshare_s3_154`. La configuración usa Arduino, C++17, flash de 16 MB,
PSRAM octal y la tabla local `partitions_16mb.csv`.

La tabla reserva dos slots OTA de 7 MB, NVS, SPIFFS y coredump. La capacidad de
flash y PSRAM debe confirmarse sobre la placa física durante el arranque.

## Pinout del target

| Función | Pin / configuración |
|---|---:|
| ST7789 SCK | GPIO38 |
| ST7789 MOSI | GPIO39 |
| ST7789 DC | GPIO45 |
| ST7789 CS | GPIO21 |
| ST7789 RESET | GPIO40 |
| Backlight | GPIO46, activo alto |
| LCD | 240x240, rotación 0, IPS/invertido |
| I2C SDA / SCL | GPIO42 / GPIO41 |
| CST816T IRQ / RESET | GPIO48 / GPIO47 |
| CST816T address | `0x15` |
| Botones | GPIO0, GPIO5 y GPIO4; activos bajos |
| QMI8658 IRQ | GPIO6 |
| QMI8658 address | `0x6B` |
| Battery enable/hold | GPIO2 |
| Battery ADC | GPIO1 |
| ES8311 address | `0x18` |
| Audio MCLK/BCLK/LRCK/DOUT | GPIO8/9/10/12 |
| Audio PA control | GPIO7 |
| Audio DIN | GPIO11 |
| SD-MMC CLK/CMD/D0/D1/D2/D3 | GPIO16/15/17/18/13/14 |

## Estado actual

La HAL habilita power-hold, backlight y tres botones, ofrece reloj monotónico y
reloj de pared, y guarda la partida en NVS (`NvsSaveStorage`, namespace `neripal`,
claves `slot0` y `slot1`). Los botones representan siguiente, confirmar y volver.
Ese loop incluye el menú de ocho acciones y la confirmación de `EvolutionNotice`.

El rótulo del firmware es 0.6.1: `NERIPAL 0.6.1` en pantalla y `NeriPal 0.6.1` por serial.

Tampoco están integrados CST816T, QMI8658, ADC de batería, audio o SD-MMC. La
semilla RNG del firmware sigue siendo un placeholder del composition root.

## Secuencia recomendada

1. Sostener el dominio de alimentación mediante GPIO2.
2. Inicializar Serial y comprobar flash y PSRAM detectadas.
3. SPI2 y ST7789 arrancan en 0.6.1; el primer frame se limpia antes del backlight.
4. `IRenderer` dibuja `PetView`. El rótulo es `NERIPAL 0.6.1`.
5. Inicializar una única instancia de I2C en GPIO42/GPIO41.
6. Integrar CST816T y normalizar coordenadas al espacio lógico 240x240.
7. Agregar IMU, batería y audio mediante adaptadores separados.
8. Validar orientación, inversión, touch, debounce, consumo y niveles de batería
   sobre hardware real.

## Comandos

Ejecutar desde la raíz de NeriPal:

```powershell
pio run -e waveshare_s3_154
pio run -e waveshare_s3_154 -t upload
pio device monitor -e waveshare_s3_154
```
