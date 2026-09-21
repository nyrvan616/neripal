# Hardware de NeriPal

## Target

El firmware 0.4.0 apunta a Waveshare ESP32-S3-Touch-LCD-1.54 mediante el entorno
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

## Estado en 0.4.0

La HAL habilita power-hold, backlight y tres botones, ofrece reloj monotónico y
acepta las primitivas de `IRenderer`. Los botones representan siguiente, confirmar
y volver para la navegación V-Pet, incluido el menú de cuidado. El renderer físico
sigue siendo no-op: el firmware compila y ejecuta Core/UI (Feed/Train/Sleep/Wake/
Clean), pero aún no presenta la interfaz en el LCD.

Tampoco están integrados CST816T, QMI8658, ADC de batería, audio o SD-MMC. La
lectura de botones es provisional y requiere debounce y validación de GPIO0 durante
boot.

## Secuencia recomendada

1. Sostener el dominio de alimentación mediante GPIO2.
2. Inicializar Serial y comprobar flash y PSRAM detectadas.
3. Inicializar SPI2 y ST7789; limpiar el primer frame antes de activar backlight.
4. Implementar las primitivas de `IRenderer` y mostrar `PetView`.
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
