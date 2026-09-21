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
- La caché bloqueada (0xE0000000-0xE0003FFF) tiene una región propia en el búfer de memoria. Antes se solapaba con la memoria baja y `LCEnable` borraba las variables globales del sistema, incluida la tabla de interrupciones.
- `GXDrawDone` (0x8014fe50) está reemplazada: como no hay GPU, termina al instante activando la bandera que esperaba.
- El periodo del retrace se deduce del registro de configuración de vídeo (0xCC002002): PAL 20 ms, resto 16,683 ms.
- Estado actual: el juego ejecuta fotogramas de forma estable a unos 50 por segundo, con el hilo principal y el hilo de fotogramas, y carga los datos de arranque. Sin salida gráfica todavía no se puede saber en qué pantalla está; la lista de módulos cargados está vacía en ese punto.
- Auditoría de `main.dol`: las 14 palabras ilegales son datos dentro de la zona de código (cadena "Metrowerks" y tablas de constantes junto a 0x80006680), los 169 saltos indirectos sin tabla son saltos por puntero a función o tabla virtual y se resuelven en ejecución con `wp::call`, y los 37 `rfi` están en los vectores de excepción y en funciones ya reemplazadas o solo alcanzables por excepciones reales. No queda código alcanzable por traducir en `main.dol`.
- Salida de vídeo: `src/video.cpp` abre una ventana Win32 y, en cada retrace, convierte el framebuffer externo (YCbCr 4:2:2, dirección y ancho tomados de los registros de VI) a RGB. El framebuffer sigue vacío porque la GPU aún no se emula; el juego escribe en el FIFO de GX (0xCC008000) con instrucciones normales, así que el siguiente paso es capturar esas escrituras.
- Corrección del decodificador: en las instrucciones de formato D (`ori`, `xori`, `addi`...) el último bit pertenece al inmediato y no es el bit de registro. Se leía como bit de registro y se sobrescribía cr0. Ahora solo los códigos de operación 4, 20, 21, 23, 30, 31, 59 y 63 aceptan el bit de registro. Se comprueba con un test del decodificador y con un test de la función 0x80039510 (borrado de nodo en árbol rojo-negro) contra `_Rb_tree_insert_and_rebalance` de libstdc++ en `tests/lifted_tests.cpp`.
- Captura del FIFO de GX: las escrituras a 0xCC008000 (incluidas las de `psq_st`) se acumulan en `src/gx.cpp` y se decodifican comando a comando (registros CP, XF, BP, listas de visualización y dibujado); con `WP_LOG_GX=1` se imprime un resumen por cada copia de EFB al framebuffer.
- Diagnóstico por hilo: cada fibra tiene su propia traza de llamadas; con el temporizador se imprime la pila de todas las fibras, y `WP_WATCH=direccion` informa de los cambios de una palabra de memoria.
- El decodificador del FIFO de GX ya interpreta sin errores el flujo del juego (0 comandos desconocidos). Los pares de floats de `psq_st` van los dos al FIFO. Con `WP_LOG_GX=2` se listan los dibujados (vértices, VCD y VAT) y con `3` los primeros comandos. Cada fotograma del estado actual contiene tres cuadriláteros y una copia de EFB al framebuffer.
- Renderizador GX en Direct3D 11 (`src/gx_render.cpp`, estado y vértices en `src/gx.cpp`): decodificación de vértices por VCD/VAT con atributos directos o indexados, transformación por las matrices de XF (proyección, ventana, generación de coordenadas de textura), ubershader con las etapas TEV, decodificación de texturas (I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, CMPR), mezcla, profundidad, tijera y copia del EFB al framebuffer externo convirtiendo a YCbCr 4:2:2. La primera pantalla del juego (aviso de la correa) se dibuja correctamente. Con `WP_SAVE_FRAME=archivo.png` se guarda cada 100 fotogramas la imagen presentada.
- Detalles verificados: el cull mode 1 del juego equivale a descartar las caras con giro antihorario en pantalla; los cuatro registros BP con máscara (0xFE) se aplican al registro siguiente; las constantes TEV KONST comparten direcciones 0xE0-0xE7 con los registros de color y se distinguen por el bit 23.
- Pendiente de GX: iluminación, texturas indirectas, líneas y puntos, recorte en el plano cercano, escalado interno, copias del EFB en formatos de intensidad.
- Pendiente: gráficos (GX), audio (DSP), entrada (WPAD/KPAD), liberar las fibras de hilos terminados y contabilizar la profundidad de la traza tras un `longjmp`.
- Entrada con Wiimote real: Windows pide PIN al emparejarlo por la interfaz normal (con 1+2 y con SYNC). El emparejamiento se hará dentro del programa con la API Bluetooth de Windows, usando como PIN los 6 bytes de la dirección Bluetooth del equipo, y la lectura de los informes HID después. Mando genérico tipo Pro Controller: SDL2. Monitor del usuario: ultrawide 2K a 75 Hz.

## Objetivos del usuario

- Salida a 1080p con el juego a 60 Hz. El juego PAL elige 50 o 60 Hz según el ajuste `IPL.E60` de SYSCONF; se activará desde la configuración del programa.
- Resolución interna multiplicada por 3 o 4, dentro del renderizador propio.
- Frecuencias superiores a 60 Hz (por ejemplo 120 Hz) mediante interpolación de las matrices de los objetos entre pasos de la lógica; experimental y posterior al renderizador. El monitor del usuario es de 75 Hz, que no es múltiplo de 60.
- Juego en línea sin servidor, con un cuarto botón verde "Online" en la fila de tres botones inferiores de la selección de minijuegos. Requiere sincronización de entradas por pasos, semilla de aleatorios y reloj compartidos, intercambio de datos de Miis y conexión directa. El botón se añadirá en memoria al cargar el módulo del menú, sin modificar los recursos del juego.
- Mejoras de calidad de vida: pantalla completa sin bordes con F11, panorámico 16:9 y ultrawide, omisión de avisos y logos, mapeo de mandos, contador de fotogramas y archivo de configuración.
- Distribución: el programa no incluirá el juego; usará los archivos extraídos de la copia del usuario, con un lanzador mínimo para elegir la carpeta o el disco.

## Dolphin de referencia

- El Dolphin 2606-374 trae un servidor GDB: con `GDBPort = 2159` en `[General]` de `Dolphin.ini` escucha en el puerto 2159 y admite un único cliente por arranque. `tools/dolphin_gdb.py` implementa el cliente del protocolo GDB (registros, memoria, puntos de ruptura y de vigilancia, pasos) y con `--launch` reinicia Dolphin en cada llamada. Comprobado: parada en el punto de entrada 0x80004050, lectura de memoria y parada en un punto de ruptura de 0x80069ee0.
- Usos previstos: volcar la memoria de Dolphin en el aviso de la correa del Wiimote y comparar estados y registros con los de `wiiparty.exe` en las mismas funciones.

## Estado verificado el 2026-09-21 (auditoría del bloqueo tras la pantalla de la correa)

Verificado ejecutando `wiiparty.exe` y guardando capturas con `WP_SAVE_FRAME=ruta_%02d.png` (una cada 100 retraces):

- Sin ninguna pulsación, el juego sale solo de la pantalla de la correa (temporizador del propio juego), muestra el título y entra en la secuencia de presentación del anfitrión.
- Con A y B mantenidos (`WP_INPUT_BUTTONS=0C00`, activos tras 150 lecturas), llega al menú principal (Party Games, Pair Games, House Party y los tres botones inferiores) y se queda estable. La entrada por `KPADReadEx` llega al juego.
- Causas del bloqueo anterior, todas resueltas y comprobadas por ejecución: (1) las finalizaciones asíncronas de IOS se ejecutaban dentro de la propia petición y el DVD del juego fallaba con `freeDvdContext.inUse`; ahora se encolan y se entregan como interrupción de IPC (`ipc_deliver`); (2) la calibración del audio esperaba el contador de muestras de AI (0xCD006C08), ahora emulado en `src/audio.cpp`; (3) el arranque del DSP esperaba a un procesador que no se emula: las 22 funciones públicas del núcleo AX (0x8015ea40-0x80160b10) se sustituyen por versiones que devuelven cero (`HleZero_*`).
- Lectura del mando: `KPADReadEx` (0x801934d0, estructura de 0xF0 bytes) y `WPADProbe` (0x8017c9e0) están sustituidas; solo el canal 0 está conectado. Mapeo de teclado y ratón en `src/input.cpp`. Los mandos genéricos y el Wiimote real están pendientes.

Estado por componente:

- Implementado y probado: lectura de teclado y ratón como Wiimote, entrega diferida de IPC, contador de muestras de AI, renderizado del menú principal con la mayoría de elementos.
- Stub: núcleo de audio AX (sin sonido) y DSP.
- Parcial: GX. Faltan iluminación, texturas indirectas, líneas/puntos y copias del EFB en formatos de intensidad. 
- Pendiente: audio real (mezclador AX en el lado del PC), mandos genéricos y Wiimote, resolución interna, liberar las fibras de hilos terminados (aparecen decenas de fibras de `OSExitThread` sin liberar).
- No verificado: que los cuatro canales de KPAD den el estado esperado por los minijuegos; que el título se muestre correctamente a 16:9.

Coste de compilación: cada alta de una función sustituida regenera `functions.h` y recompila todos los módulos (unos 15 minutos). Pendiente separar esa declaración de los módulos.

## Texturas con paleta y copias del EFB (2026-09-21)

- Implementado en `src/gx.cpp` y `src/gx_render.cpp`: carga de TLUT (BP 0x64/0x65, memoria TMEM de paletas de 512 KB copiada en el momento de la carga), formatos C4, C8 y C14X2 con paleta IA8, RGB565 o RGB5A3 (registro BP 0x98/0xB8 por mapa), y copias del EFB a textura (BP 0x52 sin el bit de XFB) para los formatos RGB565, RGB5A3 y RGBA8 mediante `CopySubresourceRegion` en la GPU.
- La textura copiada se asocia a su dirección de destino y solo se usa si el tamaño coincide y una muestra de 32 palabras de la memoria de invitado no ha cambiado (si el juego escribe otra textura en esa dirección, se descarta).
- Verificado ejecutando con A+B: el menú principal muestra las miniaturas animadas de los minijuegos y ya no hay cuadrados magenta ni el aviso `unsupported texture format`. ctest 3/3.
- Limitaciones: la copia con bit de reducción a la mitad usa la textura a tamaño completo (se muestrea con UV normalizadas); RGB565 no fuerza alfa a 1; los formatos de intensidad (R4, Y8, RA4, RA8, A8, R8...) no se copian (se registra `unsupported EFB copy format`).
- Sigue visible: un cuadro blanco sobre "Pair Games", el panel de House Party oscuro y el fondo negro del menú. Causa sin investigar.

## Listas de visualización y cierre del fallo intermitente (2026-09-21)

- Causa: `GXBeginDisplayList` redirige el pipe de escritura (0xCC008000) a un búfer de memoria cambiando los registros PI (base 0xCC00300C, puntero de escritura 0xCC003014) mientras los registros CP (0xCC000020/22) siguen apuntando al FIFO principal. El runtime enviaba esos bytes al GP en vez de a la memoria, así que los comandos se ejecutaban en el momento de grabarse y la lista guardada quedaba con basura. Al llamarla después (comando 0x40) el analizador leía direcciones y longitudes absurdas: segfault intermitente (4 de 6 ejecuciones) y textos ausentes.
- Solución (`record_display_list` en `src/gx.cpp`): si la base PI difiere de la base CP, los bytes se escriben en memoria en el puntero PI y este avanza. Además, una llamada a lista fuera de rango se descarta con el aviso `GX display list out of range`.
- Verificado: 6 ejecuciones seguidas de 25 s sin fallos (antes 4 de 6 con segfault), ctest 3/3, y el menú principal muestra ya los textos (Suggestions, Rankings, Minigames, 1 to 4 players, About 45 min.).
- Diagnóstico nuevo: `WP_LOG_FROM=N` con `WP_LOG_GX=2` empieza el registro de dibujos tras la copia número N; se muestran mapas de textura y arrays de vértices.
- Sigue pendiente: fondo negro del menú (los dibujos 3D del fondo salen con vértices inválidos, causa sin investigar; las listas mal grabadas podrían haber sido la razón, por reverificar) y panel House Party parcial.

## Reverificación tras las listas de visualización (2026-09-21)

- Con `WP_LOG_FROM=1200` los dibujos del fondo ya no salen con vértices inválidos: eran consecuencia de las listas mal grabadas.
- Sin ninguna entrada, el juego pasa por la animación del logotipo, muestra el título (logotipo "Wii Party" a tamaño y encuadre correctos, "Press A and B together", "(c)2010 Nintendo") y llega solo al menú principal (capturas `t_05` a `t_08` de `WP_SAVE_FRAME`). El encuadre erróneo del título anotado antes no se reproduce.
- Sin verificar contra Dolphin: si el fondo negro del menú es el original.
- Defecto visible (corregido el 2026-09-21): la vista previa de House Party debe mostrar 4 Miis y el título debe mostrar el fondo rosa con personajes 3D; ambos son modelos 3D con esqueleto y no se dibujan bien.

## Modelos 3D con esqueleto: diagnóstico (2026-09-21)

- Referencia de Dolphin aportada por el usuario: el título tiene fondo rosa/blanco con Miis y animales 3D, el menú tiene fondo rosa degradado, paneles rojo/naranja/verde y botones morado/azul/amarillo, y House Party muestra 4 Miis. Nuestro render: fondo negro, sin modelos 3D y colores de paneles distintos (azul/morado/verde, botones turquesa/lima/morado).
- Evidencia: los dibujos de modelos (comandos 0x90/0x98/0xA0 con matriz de posición por vértice) llegan con posiciones brutas correctas, pero las matrices de XF que usan (índices 3, 6, 15, 27...) valen cero. Se cargan con cargas indexadas (comandos 0x20 y 0x30) desde arrays de 48 bytes por matriz (bases distintas por modelo en MEM1) y el contenido de esos arrays es cero también cuando el FIFO se procesa en el momento de escribirse (umbral de vaciado de 64 bytes probado y revertido). Por tanto no es un problema del retraso del análisis.
- Descartado: `PSMTXConcat` (0x8014b100) se compara con una multiplicación de matrices de referencia con 50 casos aleatorios y coincide (prueba añadida en `tests/lifted_tests.cpp`); la constante `(0,1)` que usa está en 0x802f58d0.
- Pendiente: averiguar qué código debería escribir esos arrays (cálculo de matrices mundo de los modelos) y por qué no lo hace o guarda ceros; hipótesis: una función reemplazada por HLE, un hilo/corrutina que no llega a ejecutarse, o datos del esqueleto sin cargar.
- Sin investigar todavía: diferencia de colores del menú y rendimiento (el usuario percibe 30-40 fps con caídas).

## Eventhook de STM, ruido de log y medición de fps (2026-09-21)

- `IOSSendRequest` asíncrono sobre `/dev/stm/eventhook` se completaba al instante, así que el SDK lo reenviaba sin parar (cientos de líneas de log y tiempo de CPU). Ahora esa petición queda pendiente para siempre (`ios::never_completes`), como en IOS real hasta que hay un evento de reinicio o apagado. Los avisos de ioctl sin manejar se registran una sola vez por dispositivo y comando.
- Nuevo diagnóstico `WP_LOG_FPS=1`: imprime una vez por segundo las copias EFB a XFB (fotogramas reales del juego). Medición con el menú principal: 50 fps al inicio y baja de forma sostenida hasta unos 20 fps en 30 s, así que hay una degradación con el tiempo (sospechas: fibras terminadas sin liberar, caché de texturas, coste por dibujo). Sin investigar todavía.
- `/shared2/menu/FaceLib/RFL_DB.dat` no existe en la NAND virtual: la base de datos de Miis está vacía (sin verificar si eso influye en los Miis que faltan).

## Plataformas objetivo y decisión sobre aurora (2026-09-21)

- Objetivo del usuario para versiones finales: Windows 10 y 11 (32 y 64 bits respectivamente, x86 y ARM), Linux y macOS.
- Estado actual, todo específico de Windows: ventana GDI (`src/video.cpp`), Direct3D 11 con HLSL (`src/gx_render.cpp`), hilos como fibras de Windows (`src/threads.cpp`), compilación con GCC de MinGW-w64.
- Consecuencias: para Linux y macOS hacen falta un backend gráfico multiplataforma (WebGPU con Dawn o similar), una capa de ventana y entrada portable (SDL3) y corrutinas portables. Windows 11 no existe en 32 bits; un ejecutable de 32 bits con ~320 MB de memoria de invitado y ~300 MB de código es el objetivo más arriesgado y queda para el final.
- Decisión: aurora (MIT) no se adopta entero ahora. Se mantiene el renderizador Direct3D 11 para conseguir un juego funcional y se conserva `include/wp/gx_render.h` como frontera estrecha (dibujar, copiar, limpiar, cargar paleta) para poder añadir después un backend WebGPU. Piezas sin dependencias gráficas de aurora (conversión de texturas) pueden incorporarse con su licencia MIT cuando aporten.

## DMA de la caché bloqueada: causa de los modelos 3D ausentes (2026-09-21)

- Causa: el juego mueve matrices y otros datos con DMA de la caché bloqueada (`LCLoadBlocks` en 0x80137c40 y `LCStoreBlocks` en 0x80137c70, escritura a los registros SPR 922/923 DMA_U y DMA_L). El runtime guardaba la escritura sin hacer la copia, así que los arrays de matrices de esqueleto quedaban a cero. En el volcado de RAM de Dolphin esos arrays contenían matrices reales (evidencia de que el juego sí las calcula).
- Solución: `wp::locked_cache_dma` (`src/runtime.cpp`) ejecuta la copia cuando DMA_L tiene el bit de disparo (memoria a caché con el bit de carga, caché a memoria sin él; longitud en bloques de 32 bytes, 0 significa 128) y limpia los bits de disparo y vaciado. El emisor (`tools/ppc/emit.py`) la invoca tras `mtspr 923`.
- Herramienta: `tools/recomp.py` ya no reescribe los archivos generados que no han cambiado ni borra los existentes, de modo que una modificación del emisor solo recompila los archivos afectados.
- Verificado: con el menú principal se ven ahora el fondo 3D y los cuatro Miis de House Party (capturas `d_04`, `d_05`). ctest 3/3.
- Nuevos problemas visibles: la imagen sale lavada (demasiado clara, con un velo blanco) y los fps bajan a unos 5 con escenas 3D (50 fps en pantallas 2D). Sin investigar.

## Colores del menú y velo (2026-09-21)

- El velo blanco visto tras arreglar el DMA era un desvanecido de transición; no es un fallo fijo.
- Añadidas las tablas de intercambio de canales de TEV (BP 0xF6-0xFD, selección por etapa en los bits 0-3 del registro alfa de cada etapa) al shader; antes se ignoraban. Sin efecto visible en el menú, pero es necesario para texturas de intensidad y otros materiales.
- El reloj del sistema (`time_base`) parte ahora de la fecha real desde el 2000-01-01 en vez de cero. Hipótesis descartada: los colores del menú no dependen de la fecha.
- Diagnóstico: `WP_LOG_GX=2` imprime por dibujo los registros de color de TEV (`regs=`), los konst, el orden, el color de material y el control de canales. Con él se ve que para el panel "Party Games" el juego escribe en el registro de color 1 el valor (0, 140, 255, 255), azul; en Dolphin el panel es rojo. La diferencia está por tanto en los datos que el juego decide, no en la conversión del renderizador (que se limita a decodificar lo que recibe).
- Sin resolver: qué entrada del juego elige el color (candidatos: datos de guardado `/wiiparty.bin` ausentes, base de datos de Miis `RFL_DB.dat` ausente, ajustes de SYSCONF, un cálculo de animación con coma flotante o cuantizado mal lifteado).
