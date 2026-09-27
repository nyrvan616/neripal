# Cierre — NeriPal 0.6

Fecha: 2026-09-26  
Directorio: `project/NeriPal/`  
Objetivo: ciclo evolutivo real, con historial de crianza, evolución offline y aviso persistente.

Este cierre dejó el rótulo en `0.4.0`. 0.6.1 lo pasa a `0.6.1` en `include/neripal/Version.hpp` y en `project(NeriPal VERSION 0.6.1)`. La pantalla muestra `NERIPAL 0.6.1` y el serial `NeriPal 0.6.1`.

## Objetivo

Ciclo evolutivo real de NeriPal.

La edad abre la etapa siguiente. El historial de crianza elige la forma cuando hay más de una salida. El estado de este minuto y la historia de la etapa son datos distintos.

## Implementado

- Cinco stats base: Hunger, Energy, Hygiene, Affection, Stimulation.
- Niveles Normal, Attention y Urgent, consultados solo con `needLevel()`.
- Care mistakes: una ventana por episodio urgente, un mistake al vencerla, sin repetir el mismo episodio.
- Historial de crianza por etapa (`StageHistory`), más el total de mistakes de la vida.
- Etapas Egg → Baby → Child → Adult → Final, en un solo paso cada vez.
- Nacimiento como `GameEventKind::Hatched` al pasar de Egg a Baby. No es una etapa.
- Forma (`FormId`) separada de la etapa. Una vez elegida, se conserva.
- Motor declarativo `EvolutionRules`: la primera regla que coincide gana.
- Selección de forma a partir del historial de la etapa que se deja, no del stat instantáneo.
- RNG solo si la regla ganadora tiene más de una salida. Se tira una vez y la forma queda guardada.
- Evolución offline, incluido Child → Adult, por el mismo `stepMinute` que el juego en vivo.
- `EvolutionNotice`: cola persistente. Confirmarla no cambia etapa ni forma.
- Save V2, lectura de V1 y migración explícita.
- UI: menú con Pet y Play, números en Status, señales de necesidad en Home y overlay de evolución.

Fuera de este cierre: enfermedad, medicina, excesos de cuidado, envejecimiento, muerte, despedida, nuevo huevo, personalidad, entorno, audio y arte definitivo.

## Reglas conceptuales

- La edad determina cuándo puede evolucionar.
- El historial determina hacia qué forma, en el único punto de 0.6 que tiene varias salidas: Child → Adult.
- El estado actual (los cinco stats, salud y felicidad de este minuto) no elige la forma.
- El historial (mistakes, entrenamientos, minutos con salud o felicidad en banda) sí entra en las reglas.
- La forma no representa personalidad ni calidad.
- Ninguna rama fixture debe leerse como mejor o peor.
- AdultA, AdultB, AdultC y AdultSecret son fixtures temporales de validación. No son criaturas aprobadas ni un ranking.

Egg usa forma `None`. Baby y Child usan `Juvenile`. Adult y Final usan una forma adulta. Final conserva la forma que ya tenía Adult. Si el destino adulto no trae una forma adulta válida, el fallback de debug y de migración V1 es `AdultC`. Ese fallback no es el motor de reglas.

## Persistencia

Save V2 es lo que se escribe siempre. El codec sigue decodificando V1.

| Dato | Valor |
|---|---|
| Payload V2 | 221 bytes |
| Blob V2 (cabecera de 12 + payload) | 233 bytes |
| Capacidad del slot | 320 bytes |
| Margen libre en el slot | 87 bytes |
| Magia | `NP01` (`0x4E503031`) |
| Versión | 2 |
| CRC | obligatorio |

La cabecera es magia, versión, longitud y CRC. Un blob con versión distinta de 1 o 2 se rechaza. Un V1 cuya versión se reescriba a 2 sin el largo V2 es `BadLength`, no una migración silenciosa.

Se persisten los campos necesarios para reproducir el snapshot: stats (incluidos affection y stimulation), fase de necesidades, edad, resto del minuto, etapa, forma, causa de sueño, siesta, episodios abiertos, historial de las cinco etapas Egg–Final, mistakes de por vida y la cola de avisos. No se persisten `NeedLevel`, `TrainingLevel`, mood ni la pose de presentación.

`CareRecord` tiene ocho huecos de historial. Solo los cinco de etapa viajan en el save (30 bytes cada uno). Los tres huecos de reserva se quedan en cero y no se guardan.

Migración V1 → snapshot usable por V2, hecha en el codec antes de `restoreSnapshot`:

- affection y stimulation en 70;
- fase de necesidades en 0;
- historial, episodios y avisos vacíos;
- Egg → `None`, Baby y Child → `Juvenile`, Adult y Final → `AdultC`.

Cargar no vuelve a evaluar `EvolutionRules` ni consume el RNG de evolución. Un aviso pendiente sigue en la cola hasta `confirmEvolutionNotice()`.

## Offline

`update()` y `applyOffline()` comparten `stepMinute`.

- Las necesidades y la edad avanzan durante el apagado.
- El historial de la etapa se suma en los minutos de esa etapa. Al cruzar una compuerta, los minutos siguientes cuentan para la etapa nueva.
- La evolución puede ocurrir offline, también Child → Adult.
- Los care mistakes no se abren, no avanzan, no vencen y no se suman offline. Las necesidades sí siguen decayendo.
- Si la regla ganadora tiene dos salidas, el RNG se consume una vez en ese cruce y la forma queda en el snapshot. Encender, restaurar o cargar no vuelve a tirar.
- Cada cruce empuja un `EvolutionNotice`. La cola sobrevive al restore del save. El anillo de `GameEvent` no: se vacía al restaurar. Confirmar el aviso solo lo saca de la cola.

El minuto en el que se aterriza permanece en la etapa anterior. Una edad que ya superaba la compuerta avanza una sola etapa y no encadena el resto de compuertas ya vencidas. Un hueco largo desde edad 0 sí cruza cada compuerta que el tiempo agregado alcanza.

## Balance provisional

Estos números viven en `Balance.hpp` y en la tabla fixture de `EvolutionRules.cpp`. No son reglas definitivas de diseño.

- Umbrales de necesidad. Hambre: Attention desde 60, Urgent desde 80. El resto de stats base, que bajan: Attention en 40 o menos, Urgent en 20 o menos.
- Cadencias por minuto simulado: hambre +1 siempre; energía −1 despierto y +2 dormido; higiene −1 despierto y más lenta dormida; affection −1 cada 3 minutos despierto y cada 6 dormido; stimulation −1 por minuto despierto y estable dormido.
- Ventana de atención: 15 minutos despiertos. El minuto que abre la ventana no consume un paso. Dormir la pausa. Solo Urgent abre la ventana. Attention no es un mistake y no arranca el reloj.
- Efectos de acción: Feed, Train, Clean, Pet y Play tienen deltas provisionales en `careEffect`. Sleep y Wake no cambian stats por el delta.
- Entrenamiento: solo Train cuenta. Bajo es menos de 6, moderado desde 6, frecuente desde 18, contado en la etapa.
- Duraciones de etapa: huevo 1 h, Child a las 6 h, Adult a las 24 h, Final a las 72 h.
- Tabla fixture Child → Adult, en este orden: mistakes exactos 0, entrenamiento moderado y al menos 90 % de minutos con salud y felicidad buenas → `AdultSecret` o `AdultB` con un solo `nextBounded(2)`; entrenamiento frecuente → `AdultA`; felicidad buena al menos 60 % y como máximo 2 mistakes → `AdultB`; si nada coincide → `AdultC`.
- Salud −1 por minuto solo con hambre 100, energía 0 o higiene 0. Felicidad −1 por minuto si, después del decay de ese minuto, alguna necesidad base está Urgent. Un punto en total, no uno por necesidad.
- Bandas del historial: un minuto cuenta como bueno desde 70 y como pobre por debajo de 40. Esos cortes no son `needLevel()`.

## Diferencias frente a los documentos de diseño

Se releyeron `references/neripal-research/crianzaNecesidadesEvoluciones.md` y `references/neripal-research/reglasEvolucionesStats`. Esos textos dejan los números y el peso de la evolución como pendientes. 0.6 los cubre con valores provisionales. No los convierte en diseño cerrado.

| Diferencia | Clase |
|---|---|
| Umbrales, cadencias, deltas de acción, 15 minutos, 6/18 entrenamientos, 1 h/6 h/24 h/72 h | Valor provisional de balance |
| Tabla AdultA/B/C/Secret y el fallback `AdultC` | Fixture temporal |
| La ventana de mistake empieza solo en Urgent, no en Attention | Valor provisional de balance. El diseño no fijaba la regla exacta |
| Stimulation dormida no se mueve. El diseño admite “muy poco o estable” | Valor provisional de balance |
| Salud y felicidad usan las fórmulas cortas de arriba. Enfermedad y exceso quedan fuera | Valor provisional de balance |
| Solo hay un punto con varias formas: Child → Adult. El resto de etapas no elige rama | Fixture temporal |
| `deriveMood` no mira Affection ni Stimulation | Deuda técnica, anotada para 0.7 |
| Sobrealimentar, despertar de más o entrenar de más no suman mistakes | Deuda hacia 0.7. El diseño los nombra; 0.6 no los cuenta |
| `kNeedsSettleSteps` (400) sigue declarado y ya no corta el catch-up | Deuda técnica |
| Personalidad, entorno, exceso como huella, muerte y ciclo nuevo | Fuera de 0.6, como pedían ambos documentos |

No apareció una contradicción entre stats, historial, etapas, reglas, offline, Save V2 y la UI que obligara a cambiar comportamiento. La UI llama a Core y muestra el resultado: no recalcula umbrales, no ejecuta `EvolutionRules` y no confirma un aviso antes de mostrarlo.

## Pendiente para 0.7 y después

No implementado en este cierre:

- enfermedad, distinta de salud baja;
- medicina;
- consecuencias de sobrealimentación y sobreentrenamiento;
- envejecimiento después de Final;
- muerte;
- despedida;
- nuevo huevo;
- ciclo completo;
- personalidad;
- entorno;
- formas y sprites definitivos;
- reglas evolutivas definitivas, en reemplazo de la tabla fixture;
- ajuste de mood para Affection y Stimulation;
- audio real. El gancho `urgentSoundRequested()` existe y no reproduce sonido. El aviso urgente ya es visual.

## Deuda técnica y hardware

`WaveshareS3Platform::drawText` está vacío. También lo están `beginFrame`, `fillRect`, `drawRect` y `endFrame`. El firmware compila y corre el mismo loop que el host, pero no dibuja en el LCD.

0.6 queda verificada por build de host, tests deterministas y build de firmware. La UI en el dispositivo físico no está verificada.

`DebugController::forceEvolution` escribe etapa y una forma coherente mediante `restore()`. No ejecuta `EvolutionRules` y no representa el juego. `restore()` además borra avisos pendientes y episodios de cuidado. Es solo herramienta de debug. El panel de debug no entra en el binario de PlatformIO.

Los tres huecos de reserva del historial no viajan en el save. Guardarlos sumaría 90 bytes y el blob pasaría de 320.

## Evidencia de validación

```text
Host C++/Win32:   preset windows-debug, sin cambios de comportamiento
CTest:            3/3
Unit tests Core:  158
Unit tests UI:    29
Unit tests Save:  46
Total:            233
ESP32-S3:         PlatformIO SUCCESS (waveshare_s3_154)
RAM estática:     19 604 / 327 680 bytes (6.0 %)
Flash app:        308 353 / 7 340 032 bytes (4.2 %)
Hardware:         no flasheado; LCD no ejercitado
```

No se corrigió ningún bug durante este cierre. Los tests ya pasaban con el comportamiento de 0.6.

## Pruebas manuales en el dispositivo

Hoy el LCD no pinta. Estas pruebas sirven cuando `IRenderer` deje de ser un stub. El simulador de escritorio puede hacer las mismas comprobaciones antes.

1. Menú de ocho ítems: Feed, Train, Sleep, Clean, Pet, Play, Status, Home. Pet y Play deben mostrar feedback y mover affection o stimulation.
2. Con todos los stats en Normal, Home no debe mostrar símbolos de necesidad.
3. Bajar o subir un solo stat hasta Attention y hasta Urgent. El símbolo no cambia; Urgent se ve más intenso y no depende solo del color.
4. Status muestra los cinco stats base, Health, Happiness, la etapa y el identificador de forma. Final usa la misma rana que Adult.
5. Dejar pasar una compuerta de edad, en vivo y tras un apagado. Debe aparecer el aviso de evolución, en orden si hay varios, y confirmarlo no debe cambiar etapa ni forma.
6. Guardar, apagar y encender con un aviso sin confirmar. El aviso sigue. La forma no se vuelve a sortear.
7. Una partida V1 antigua debe abrir en una forma coherente con su etapa, sin historial nuevo inventado.
