# Desarrollo de NeriPal

## Modelo de carpeta independiente

Esta guía asume que el desarrollador posee únicamente la carpeta `NeriPal/`. La
carpeta es la raíz del proyecto y debe contener, entre otros, estos archivos:

```text
NeriPal/
  CMakeLists.txt
  CMakePresets.json
  platformio.ini
  README.md
  include/
  src/
  simulator/
  tests/
```

No se necesitan repositorios vecinos, rutas absolutas ni material externo para
compilar, probar, ejecutar o depurar el proyecto.

Todos los comandos siguientes se ejecutan desde la raíz de `NeriPal`:

```powershell
Set-Location C:\ruta\a\NeriPal
```

## Herramientas

- CMake 3.20 o posterior.
- Ninja.
- Un compilador C++17 para Windows: MinGW-w64/WinLibs o Visual Studio Build Tools.
- PlatformIO Core para el firmware; también puede usarse la extensión de VS Code.
- VS Code y las extensiones recomendadas son opcionales.

Verificación rápida del entorno:

```powershell
cmake --version
ninja --version
pio --version
```

## Compilar, probar y ejecutar el simulador

El preset genera todo dentro de `build/windows-debug/`, que es descartable y está
ignorado por Git:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
.\build\windows-debug\neripal_simulator.exe
```

Consultar la versión sin abrir la ventana:

```powershell
.\build\windows-debug\neripal_simulator.exe --version
```

Ejecutar directamente la suite y ver sus casos individuales:

```powershell
.\build\windows-debug\neripal_core_tests.exe
.\build\windows-debug\neripal_ui_tests.exe
```

El script portable obtiene la raíz a partir de su propia ubicación, por lo que no
depende del directorio padre de NeriPal:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -Configuration Release
```

El script usa `build/windows/`; el preset usa `build/windows-debug/`. Son flujos
alternativos y no deben mezclarse al buscar el ejecutable resultante.

## Depurar el simulador

El preset `windows-debug` compila con símbolos de depuración.

Con VS Code y CMake Tools:

```powershell
code .
```

Seleccione el preset `windows-debug`, el target `neripal_simulator` y use la orden
**CMake: Debug**. No hace falta crear rutas absolutas en `launch.json`.

Con MinGW/GDB desde terminal:

```powershell
gdb --args .\build\windows-debug\neripal_simulator.exe
```

Dentro de GDB, por ejemplo:

```text
break main
run
```

Los archivos `.vscode/launch.json` y `.vscode/c_cpp_properties.json` creados por
PlatformIO contienen rutas locales y están ignorados por Git. Cada desarrollador
debe permitir que PlatformIO los regenere en su propia máquina; no son parte de la
configuración portable del proyecto.

## Firmware ESP32-S3

Compilar sin placa conectada:

```powershell
pio run -e waveshare_s3_154
```

El firmware queda en `.pio/build/waveshare_s3_154/firmware.bin`.

Cargar y abrir el monitor serie, con la placa conectada:

```powershell
pio run -e waveshare_s3_154 -t upload
pio device monitor -e waveshare_s3_154
```

Iniciar el depurador de PlatformIO, si la conexión USB/JTAG o el probe disponible
es compatible:

```powershell
pio debug -e waveshare_s3_154
```

La depuración física depende del hardware y sus drivers; el build del firmware no.

## Limpiar artefactos

```powershell
cmake --build --preset windows-debug --target clean
pio run -e waveshare_s3_154 -t clean
```

Si se elimina manualmente `build/` o `.pio/`, CMake y PlatformIO los regeneran. No
contienen código fuente ni datos necesarios para reconstruir el proyecto.

## Regla para nuevos comandos

Los nuevos scripts y ejemplos deben resolver archivos desde la raíz del propio
repositorio o desde la ubicación del script. No deben mencionar rutas personales,
el antiguo directorio contenedor ni asumir repositorios hermanos.
