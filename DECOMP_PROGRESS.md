# Decomp progress

## Estado

- Toolchain: CMake 4.4, Ninja, GCC 15.2 (MinGW-w64), Rust/Cargo, Python 3.12, JDK 25, nodtool 1.4.4. Sin cross-compiler PPC (no hace falta: es recompilación, no matching)
- Ghidra 12.1.3 en `ghidra/install/` (ignorada por Git)
- Disco: `game/wiiparty.rvz` extraído en `extracted/` (ID SUPP01, PAL)
- Binario: DOL parseado (`tools/dol.py`); 115 REL descomprimidos a `build/rel/` con `tools/lz11.py` (todos válidos, formato REL v3, parser en `tools/rel.py`)
- Ghidra: DOL cargado con `ghidra/scripts/LoadDol.java` y el procesador `PowerPC:BE:32:Gekko_Broadway` del GameCube Loader; `tools/ghidra_import.py` importa, analiza, aplica `analysis/symbols.csv` y exporta la lista de funciones
- MCP de Ghidra: puente 6.0.0 instalado en `.venv/`, extensión en `%APPDATA%\ghidra\ghidra_12.1.3_PUBLIC\Extensions\GhidraMCP` (compilada para 12.1.2, versión parcheada a 12.1.3); pendiente activarla en el CodeBrowser
- Funciones identificadas por Ghidra: 6115 (`analysis/dol_functions.csv`), 94,8 % del código cubierto (75,4 % con el procesador genérico); `FindFunctions.java` añade 812
- Funciones implementadas: 0
- Lifter: `tools/recomp.py` genera C++ para 7014 funciones del DOL (473 789 instrucciones, 20 archivos en `build/recomp/`) y compila sin errores ni avisos con GCC
- Build: CMake + Ninja (`build/out`), librerías `wp_runtime` y `wp_dol`
- Tests: 3 suites en CTest (traductor Python, runtime C++, función lifteada del DOL contra referencia escrita a partir del decompilador de Ghidra)
- Próximo objetivo: traducir el resto de módulos REL (`python tools/recomp_rel.py --all`), la capa GX (gráficos), DSP/audio y entrada

## Arquitectura objetivo

- CPU: PowerPC 750CL (Broadway), 32 bits, big-endian, con paired singles
- ABI: PowerPC EABI (Wii/GameCube SDK); r1 pila, r2/r13 small data areas
- Enfoque: lifter DOL/REL -> C++ + runtime que reimplemente RVL SDK (OS, VI, GX, PAD/WPAD, DVD)

## Binarios

- `sys/main.dol`: 2 294 304 bytes, entrada `0x80004050`, SDK RVL de 2009-2010
- Texto: `0x80004000` (0x2720) y `0x800070e0` (0x1d5f20)
- Datos: `0x80006720`, `0x80006bc0`, `0x801dd000`, `0x801dd0c0`, `0x801dd0e0`, `0x80207f80`, `0x802f5440`, `0x802f6900`
- BSS: `0x80231980`, tamaño `0xc6e88`
- `files/rel/*.rel.lz`: 115 módulos comprimidos con LZ11 (cabecera `0x11` + tamaño). Incluyen `boot`, `menu`, `loading`, `openmess`, `ranking`, `inst`, `mg1xx`-`mg5xx` (minijuegos), `mr*` (tableros/modos) y `ms*`
- Sin mapa de símbolos en el disco

## Ejecución (Hito 1)

- `build/out/wiiparty.exe extracted [segundos]` carga el DOL, prepara la memoria baja y ejecuta `entry`; si no avanza imprime la pila de llamadas
- Con los datos de arranque reales (`src/boot.cpp`, contrastados con volcados de RAM de Dolphin en `reference/ram-dumps/`), `OSInit` sigue el camino verdadero, comprueba la unidad de disco (`0x8015bb60`) y termina. El juego llega a `main`, pasa varios retrace de vídeo, crea hilos, lee datos del disco, inicializa GX, VI, LYT, G3D, EF y RFL, carga y enlaza `boot.rel`, ejecuta su `_prolog`, lee `locale/en_EU/boot/strap.arc.lz` y entra en el bucle de tareas de la escena (`0x80069ee0`), sin bloquearse
- Con datos de arranque incompletos llegó a `main` (`0x800070e0`) y esperaba el retrace vertical en `VIWaitForRetrace` (`0x80147860`)
- Estrategia: no se simula el hardware del SDK, se sustituyen funciones por manejadores del runtime (`analysis/hle_functions.csv` lista los nombres, `analysis/symbols.csv` los resuelve a direcciones, `src/hle.cpp` los implementa)
- Sustituidos hasta ahora: `EXIInit/Lock/Unlock/Probe/GetID` (bus EXI sin dispositivos), `__OSInitAudioSystem` (DSP), `OSRealModeCall` (BAT), `IOSSendRequest` (todo IPC con IOS, simulado en `src/ios.cpp`: dispositivos `/dev/stm`, `/dev/fs`, `/dev/es`, `/dev/di` y archivos NAND)
- `sc` es un no-op (solo vacía cachés en este SDK)
- Dispositivo `/dev/di` (en `src/ios.cpp`): responde como una unidad normal. Inquiry (0x12) devuelve ceros, lectura sin cifrar (0x8d) fuera del disco da error 0x52100, ReportKey (0xa4) da error 0x53100 y RequestError (0xe0) entrega el último error; los demás comandos devuelven éxito
- Interrupciones: `WP_POLL` en cada salto hacia atrás cada 16384 vueltas llama a `poll_interrupts` (`src/interrupts.cpp`); si el juego tiene EE activado y toca el retrace (periodo fijo de 20 ms de momento), activa los indicadores de `0xCC002030/34` y llama al manejador de la tabla `0x80003040 + 4*24` con el contexto actual en `r4`, y después a `OSSelectThread` (`0x8013fad0`)
- Hilos (`src/threads.cpp`): cada hilo del juego es una fibra de Windows. Las llamadas a `OSSaveContext` (`0x80138290`) se traducen con `setjmp` (`wp_jump`) y `OSLoadContext` (`0x80138310`, sustituido) vuelve a ese punto con `longjmp` o arranca una fibra nueva si el contexto salió de `OSInitContext`; los registros se guardan en el propio `OSContext` del juego. `OSSwitchFiber` (`0x80138400`) cambia el `r1` y llama a la función. Solo Windows por ahora
- Mandos: `WPADInit` y `KPADInit` (`0x8017b5a0`, `0x80193cf0`) están sustituidos por versiones vacías (sin mandos conectados). Diseño previsto: capa neutra con el estado de un mando y backends enchufables (teclado y ratón primero; mandos genéricos y Wii Remotes reales después)
- `OSReport` (`0x80138880`) y `OSPanic` (`0x80138900`) están sustituidos y muestran el texto formateado por el juego (`src/format.cpp`)
- REL (`tools/recomp_rel.py NOMBRE...` o `--all`, salida en `build/rel_code/`): el juego descomprime y enlaza cada módulo con su propio `OSLink`. Cada módulo se traduce con direcciones sintéticas (`sección << 24 | desplazamiento`); las relocaciones se resuelven al generar: llamadas al DOL directas (`f_XXXXXXXX`), llamadas internas directas (`f_<módulo>_<dirección>`), direcciones de datos como `g_<módulo>_bases[sección] + adición` y a otros módulos con `wp::external_address`. `src/modules.cpp` identifica el módulo cargado por una firma (identificador, secciones y tamaño de bss) y actualiza sus bases al primer salto. Los destinos del DOL que piden los módulos se guardan en `build/rel_dol_targets.csv` y `recomp.py` los añade como entradas
- Módulo `boot` (identificador 1): 5 funciones, 248 instrucciones; sus 31 llamadas al DOL, 18 direcciones de datos propias y 2 punteros en datos se resuelven correctamente
- NAND virtual (`src/nand.cpp`, carpeta `game/nand`, ignorada): `SYSCONF` por defecto (inglés, 4:3) generado al montar, archivos de partidas y ajustes con lectura, escritura y búsqueda, y órdenes de `/dev/fs` (crear archivo y carpeta, borrar, renombrar, atributos, listar)
- Estado de vídeo inicial (`src/boot.cpp`): registro de control `0xCC002002 = 0x0101` (PAL, activo) y registros de interrupción `0xCC002030/34 = 0x1001`. Si la pantalla no está activa `VIInit` programa NTSC y el juego elige `/locale/en_US`
- Depuración: `WP_LOG_DISC=1` muestra las lecturas del disco, `WP_DUMP=archivo` vuelca la memoria al fallar
- Pendiente: derivar la frecuencia del retrace del modo de vídeo que configure el juego, liberar fibras de hilos terminados, y lectura real de datos del disco, hilos del SO, tiempo, DVD real, lectura de archivos NAND (SYSCONF, `play_rec.dat`)

## Lifter

- `tools/ppc/decoder.py`: decodificador de instrucciones PowerPC/Gekko, incluidos paired singles. Decodifica 472 251 de 472 265 palabras de código; las 14 restantes son datos incrustados
- `tools/ppc/emit.py`: traduce cada instrucción a C++ sobre `wp::Cpu`
- `tools/ppc/cfg.py`: etiquetas de salto y tablas de saltos (104 resueltas)
- `include/wp/cpu.h`, `include/wp/memory.h`, `src/runtime.cpp`: contexto de CPU, memoria big-endian y semántica de instrucciones
- Cada función es `f_XXXXXXXX(wp::Cpu&)`; `bl` a función conocida es llamada directa, el resto pasa por `wp::call` (búsqueda en `g_function_table`)
- Instrucciones sin soporte: solo `rfi` (39 usos, manejadores de excepción)

## Herramientas locales

- `reference/dtk/dtk-windows-x86_64.exe` (decomp-toolkit 1.8.4, Apache-2.0): `dol info` y `rel info` confirman el formato de DOL y REL leído por `tools/dol.py` y `tools/rel.py`
- `tools/merge_rels.py`: fusiona el DOL con un módulo en `build/elf/<módulo>.elf` (todos con `--all`, 87 s), para abrirlos en Ghidra con las relocaciones resueltas. Un único ELF con los 106 módulos es inviable: dtk crece de forma desproporcionada (10 módulos 5 s, 20 módulos 29 s, 40 módulos más de 100 s)
- 8 grupos de módulos comparten identificador interno (`mg103`/`mg418`, `mg110`/`mg411`, `mg111`/`mg417`, `mg210`/`mg504`, `mg215`/`mg408`, `mg216`/`mg506`, `mg507`/`mg508`/`mg509`, `ms601`/`ms602`), por eso no se pueden cargar a la vez
- `tools/ghidra_decompile.py 0xDIRECCION ...`: decompila funciones sin abrir Ghidra (necesita el proyecto cerrado)

## Estructuras, símbolos y offsets

- Memoria baja real (de un volcado propio): FST al final de MEM1 (`0x81800000` menos el tamaño de `fst.bin`), BI2 justo debajo, MEM2 termina en `0x93600000` con el heap de IOS en los últimos `0x20000`, tabla de manejadores de interrupciones en `0x80003000`, modo de vídeo PAL = 1

- Símbolos: `analysis/symbols.csv` guarda los nombres puestos a mano (versionado); `reference/symbols/SUPP01.map` es el mapa exportado de Dolphin con su base de firmas (`Sys/totaldb.dsy`), local y sin versionar, y `tools/import_map.py` lo convierte a `build/dolphin_symbols.csv` (2544 nombres reales, 5280 entradas sin nombre). Los 22 nombres manuales coinciden con el mapa en todos los casos donde este tiene nombre. La pila de llamadas del runtime imprime los nombres
- Petición IPC de IOS: `+0x00` orden, `+0x04` resultado, `+0x08` descriptor, `+0x0C` argumentos, `+0x20` callback, `+0x24` argumento del callback; comandos 1 open, 2 close, 6 ioctl, 7 ioctlv
- Puntero al heap IPC: `r13-0x78e4`; liberar petición: `0x80177300`
- Retrace de vídeo: contador en `r13-0x713c`, cola de hilos en `r13-0x7160`

## Hipótesis y problemas conocidos

- Lifter: el bit OE (desbordamiento) se ignora; `fres`/`frsqrte` usan `1/x` en vez de la tabla de estimación de Broadway; `blrl` y llamadas indirectas dependen de `g_function_table`
- Lifter: 172 `bctr` sin tabla de saltos detectada; 158 son llamadas virtuales (`lwz`+`mtctr`+`bctr`), tratadas como salto indirecto y retorno; quedan 14 por revisar
- Lifter: además de `dol_functions.csv`, descubre entradas por destinos de `bl`, saltos externos, punteros en datos, constantes de dirección `lis`+`addi` y destinos de tablas; una función que cae en la siguiente la enlaza con una llamada final
- Lifter: límites de función = siguiente entrada de `dol_functions.csv`; sin runtime todavía (memoria sin inicializar, hardware, `sc`, excepciones)

- Ghidra 12.1.3 no trae el procesador Gekko/Broadway; se usa el del GameCube Loader (instalado en `%APPDATA%\ghidra\ghidra_12.1.3_PUBLIC\Extensions\`, fuente en `ghidra/extensions/`)
- Los nombres de funciones muy pequeñas del mapa de Dolphin pueden ser incorrectos (por ejemplo `OSGetCurrentContext` en varios getters distintos), porque la base de firmas asigna el mismo nombre a cuerpos idénticos
- Ghidra no tiene ABI EABI; se usa el `default` (System V PPC 32)
- La BSS de la cabecera solapa con `.data5`-`.data7`; `LoadDol.java` solo crea bloques BSS en los huecos

- La lógica del juego vive sobre todo en los REL: el recompilador tendrá que soportar carga dinámica y relocaciones de REL, no solo el DOL
- El DOL contiene el SDK y el motor común; los REL se cargan por escena

## Plan

1. Revisar cobertura del análisis del DOL
2. Importar los REL en Ghidra con relocaciones
3. Identificar funciones del SDK y tabla de símbolos en `analysis/`
4. Lifter de instrucciones PPC y sistema de build CMake
5. Extender el lifter a los REL

## Estado tras traducir todos los módulos

- Los 115 módulos REL se traducen (`python tools/recomp_rel.py --all`, unos 650 MB de C++) y el ejecutable completo compila (unos 316 MB). Recompilar todo tarda del orden de 15 minutos con 12 hilos.
- Perfilador de diagnóstico: `WP_PROFILE=1` muestrea la función más interna cada milisegundo y la imprime al vencer el temporizador; con `WP_DUMP=archivo` vuelca también la memoria del invitado.
- El motor de tareas del juego (0x80069af0-0x8006a96c) usa `setjmp` (0x801c8a1c) y `longjmp` (0x801c8b20) como corrutinas. El lifter emite `setjmp` nativo en las llamadas a `setjmp` y el runtime reemplaza `longjmp` por un cambio de fibra; un búfer cuyo enlace o pila difieren del guardado se trata como contexto nuevo y arranca una fibra en la dirección guardada. Los módulos se tienen que regenerar para incluir este cambio.
- Estado actual: las tareas del juego ya se ejecutan. El hilo principal queda esperando en `GXDrawDone` (0x8014fe50) a que la interrupción de fin de dibujado del procesador gráfico active una bandera; hace falta una implementación de GX o de esa interrupción.
- Pendiente para dejar `main.dol` al 100 %: 14 palabras ilegales, 169 saltos indirectos sin resolver y 37 usos de `rfi`.
