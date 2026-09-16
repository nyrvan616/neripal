# Simulador de NeriPal

El simulador es una aplicación Win32 sin SDL ni LVGL. Usa las mismas reglas de
Core y el mismo `PetView` que consumirá el dispositivo. La ventana escala el espacio
lógico 240x240 mediante GDI.

## Ejecutar

Todos los comandos de esta página parten de la raíz independiente de NeriPal, no
desde la carpeta `simulator/`:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
.\build\windows-debug\neripal_simulator.exe
```

El ejecutable abre una consola de diagnóstico además de la ventana 240x240. La
versión se imprime al inicio y también puede consultarse sin abrir la ventana:

```powershell
.\build\windows-debug\neripal_simulator.exe --version
```

## Controles

| Tecla | Acción |
|---|---|
| `F`, `T` | Feed, train |
| `S`, `W` | Alternar sueño, despertar |
| `R` | Reset completo del Pet |
| `1` a `4` | Velocidad x1, x10, x100, x1000 |
| `A` | Avanzar una hora simulada |
| `Tab` | Cambiar stat seleccionado |
| `Up`, `Down` | Ajustar ±5 |
| `Home`, `End` | Ajustar a 100 o 0 |
| `Esc` | Cerrar |

## Separación de debug

`simulator/debug/DebugController` se compila únicamente dentro del ejecutable de
desktop. Para editar stats toma un `PetState`, lo modifica y llama a la operación
normal `Pet::restore()`. El firmware no compila este directorio y el API público de
`Pet` no contiene métodos exclusivos de debug.

Para iniciar una sesión paso a paso con VS Code o GDB, consultar
[`../docs/DEVELOPMENT.md`](../docs/DEVELOPMENT.md#depurar-el-simulador).

## Limitaciones

- Solo Windows; no existe backend Linux/macOS.
- La vista actual es estática salvo por estado de sueño y cambios de stats.
- No hay mouse/touch sintético, captura automática de frames ni tests visuales.
- El placeholder se dibuja con primitivas; todavía no existe pipeline de sprites.
