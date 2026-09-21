# Wii Party — Mejoras de calidad de vida (QoL)

> Documento de especificación de funcionalidades para la versión nativa de PC.
>
> **Alcance:** mejoras de presentación, accesibilidad, fluidez, configuración, guardado y conectividad, manteniendo la identidad visual y el comportamiento original del juego siempre que sea posible.

---

## 1. Vídeo, rendimiento y presentación

### 1.1. Relación de aspecto dinámica

**Objetivo:** adaptar el juego a formatos panorámicos y ultrapanorámicos sin deformar la interfaz ni los elementos 2D.

**Requisitos:**
- Desvincular la cámara y el campo de visión de las relaciones 4:3 y 16:9 originales.
- Calcular dinámicamente el encuadre según la resolución de salida.
- Añadir soporte para formatos como 21:9 y 32:9.
- Mantener los elementos `.brlyt` correctamente anclados a sus posiciones relativas.
- Evitar el estiramiento de menús, marcadores y otros elementos de la interfaz.

### 1.2. Alta frecuencia de refresco

**Objetivo:** ofrecer una presentación fluida a 120 Hz, 144 Hz y frecuencias superiores sin alterar la lógica del juego.

**Requisitos:**
- Separar el ciclo de presentación gráfica de la lógica interna del juego.
- Tratar `VIWaitForRetrace` de forma compatible con la ejecución desacoplada.
- Utilizar un control temporal basado en `std::chrono` o una solución equivalente.
- Mantener independientes la frecuencia de renderizado, la lógica, las físicas y los temporizadores de los minijuegos.
- Evitar que el aumento de FPS acelere animaciones o mecánicas jugables.

### 1.3. Texturas en alta resolución

**Objetivo:** permitir el uso de texturas personalizadas de mayor resolución.

**Requisitos:**
- Crear una ruta de sustitución de recursos dentro de `extracted/`.
- Buscar primero una versión personalizada del recurso en el sistema de archivos del PC.
- Utilizar la textura HD disponible en lugar del recurso `.brtex` original cuando exista.
- Mantener un mecanismo de fallback al recurso original.
- Documentar la convención de nombres y la estructura de directorios.

---

## 2. Fluidez y ritmo de juego

### 2.1. Aceleración y omisión de diálogos

**Objetivo:** reducir el tiempo dedicado a textos y conversaciones repetitivas.

**Requisitos:**
- Permitir avanzar rápidamente los cuadros de diálogo.
- Añadir una acción configurable para omitir texto de forma instantánea.
- Permitir, opcionalmente, aumentar la velocidad de aparición de los diálogos.
- Evitar que la aceleración altere los eventos asociados a la finalización de cada diálogo.

### 2.2. Omitir introducciones repetitivas

**Objetivo:** permitir saltar secuencias introductorias que el jugador ya conoce.

**Requisitos:**
- Añadir una opción para omitir la presentación del anfitrión al iniciar los minijuegos.
- Permitir omitir animaciones repetitivas del tablón principal cuando proceda.
- Mantener disponibles las secuencias originales mediante una opción configurable.
- Garantizar que omitir una animación no impida ejecutar sus eventos de inicialización.

### 2.3. Modo de tablero rápido

**Objetivo:** agilizar las partidas de tablero sin modificar sus reglas.

**Requisitos:**
- Añadir una opción activable desde la configuración.
- Acelerar el desplazamiento de los Miis por las casillas.
- Reducir la duración de las animaciones del lanzamiento de dados.
- Mantener intactas las reglas, los resultados aleatorios y las condiciones de victoria.
- Aplicar la aceleración únicamente a las secuencias compatibles.

### 2.4. Precarga de recursos

**Objetivo:** reducir los tiempos de espera entre escenas y minijuegos.

**Requisitos:**
- Aprovechar la memoria disponible en el PC para precargar recursos.
- Preparar la carga anticipada de los módulos `.rel` y de los recursos asociados.
- Sustituir las esperas derivadas del acceso al DVD original por operaciones de almacenamiento local.
- Ejecutar la precarga en segundo plano cuando no interfiera con la ejecución.
- Incorporar límites de memoria y mecanismos de liberación de recursos.

---

## 3. IA y sistema de Miis

### 3.1. Dificultad global de la CPU

**Objetivo:** permitir establecer una dificultad fija para los Miis controlados por la IA.

**Requisitos:**
- Añadir una opción global de dificultad.
- Permitir seleccionar niveles como `Experto` o `Maestro`, si están disponibles en la lógica del juego.
- Aplicar la configuración a los modos de juego compatibles.
- Mantener una opción para utilizar la dificultad original de cada modo.
- Evitar modificar de forma accidental la dificultad de los jugadores humanos.

### 3.2. Corrección del mapeo de dificultad de los Miis

**Objetivo:** garantizar que las estadísticas y los niveles de IA se asignen correctamente.

**Requisitos:**
- Revisar las tablas internas relacionadas con los Miis controlados por la CPU.
- Verificar la correspondencia entre identificadores, estadísticas y niveles de dificultad.
- Corregir las asignaciones inconsistentes detectadas.
- Comprobar de forma específica los Miis considerados legendarios, como Matt, Saburo o Elisa.
- Validar el comportamiento resultante en los modos de juego afectados.

---

## 4. Interfaz, configuración y guardado

### 4.1. Menú de opciones integrado

**Objetivo:** ofrecer una configuración accesible desde el propio juego, sin depender de herramientas externas.

**Requisitos:**
- Integrar un menú de opciones en la pantalla de inicio.
- Mantener la estética visual y la navegación de Wii Party.
- Permitir configurar:
  - Resolución de salida.
  - Volumen.
  - Filtros gráficos.
  - Asignación de controles.
  - Opciones de relación de aspecto y frecuencia de refresco, cuando proceda.
- Implementar el menú mediante la infraestructura gráfica y de interfaz disponible en la versión nativa.

### 4.2. Sistema de logros

**Objetivo:** añadir logros opcionales específicos de la versión de PC.

**Requisitos:**
- Crear un sistema de eventos de logros integrado en el motor.
- Mostrar notificaciones emergentes durante la partida.
- Definir logros basados en acciones o desafíos concretos.
- Guardar el progreso localmente en un archivo `.json`.
- Evitar que los logros modifiquen las reglas del juego.
- Permitir desactivar las notificaciones.

### 4.3. Cambio de mandos en caliente

**Objetivo:** gestionar dinámicamente los dispositivos de entrada durante la ejecución.

**Requisitos:**
- Detectar la conexión y desconexión de mandos.
- Permitir cambiar entre teclado y mandos de Xbox, PlayStation, Switch u otros dispositivos compatibles.
- Reasignar dispositivos a los jugadores 1 a 4 sin reiniciar el juego.
- Mantener una capa de abstracción común para los distintos tipos de mando.
- Gestionar la pérdida temporal de conexión sin bloquear los hilos de juego.

### 4.4. Perfiles y copias de seguridad

**Objetivo:** sustituir el sistema de guardado único por una gestión flexible de partidas.

**Requisitos:**
- Implementar perfiles de usuario independientes en el PC.
- Permitir múltiples ranuras de guardado.
- Crear copias de seguridad automáticas del progreso.
- Mantener la compatibilidad con los datos de guardado originales cuando sea viable.
- Validar los archivos antes de cargarlos.
- Gestionar archivos dañados o incompatibles de forma segura.

---

## 5. Modo online nativo

> **Principio de diseño:** el modo online debe integrarse en la interfaz original de Wii Party. No se añadirá un lanzador externo ni un menú moderno separado para acceder a esta función.

### 5.1. Integración en la interfaz original

**Objetivo:** añadir el acceso al modo online dentro del flujo de selección de minijuegos.

**Requisitos:**
- Modificar el layout de la pantalla de selección de minijuegos.
- Añadir un cuarto botón junto a los tres botones originales.
- Situar preferentemente el botón en la tercera posición, sujeto a las limitaciones del layout.
- Conservar las proporciones, animaciones y navegación del diseño original.

### 5.2. Diseño del botón online

**Requisitos:**
- Utilizar una base de color verde coherente con la paleta de Wii Party.
- Incorporar un icono blanco que combine la silueta de un Mii con un símbolo de conexión a Internet.
- Mostrar el texto `Online`.
- Mantener la coherencia tipográfica y visual con el resto de la interfaz.
- Preparar estados visuales para reposo, selección, pulsación y bloqueo.

### 5.3. Menú de conexión

**Objetivo:** ofrecer una pantalla de conexión integrada visualmente en el juego.

**Requisitos:**
- Ejecutar una escena HLE nativa en C++ al seleccionar el botón online.
- Diseñar el menú de conexión desde cero.
- Reutilizar o reproducir, cuando sea posible, las texturas, sonidos, transiciones y animaciones del juego original.
- Mantener la navegación y la presentación coherentes con el resto de Wii Party.
- Separar la lógica de red de la lógica de presentación.

### 5.4. Arquitectura de red

**Objetivo:** permitir partidas entre varios equipos mediante una infraestructura de red específica para la versión de PC.

**Requisitos:**
- Definir una capa de abstracción para sockets y transporte.
- Evaluar el código de red de Dolphin como referencia técnica, sin asumir compatibilidad directa.
- Seleccionar un modelo de red adecuado: servidor, host-cliente o peer-to-peer.
- Diseñar un sistema de sincronización determinista para los estados relevantes de la partida.
- Gestionar latencia, pérdida de paquetes, reconexión y abandono de jugadores.
- Separar la sincronización de la lógica de juego, la presentación y la entrada.
- Definir mecanismos de validación para evitar estados divergentes entre clientes.

---

## 6. Prioridades de implementación

### Prioridad 1 — Bloqueos y fundamentos

- [ ] Conectar y validar el sistema de entrada.
- [ ] Permitir superar la pantalla inicial de la correa del Wiimote.
- [ ] Sustituir progresivamente los stubs de audio.
- [ ] Verificar la estabilidad del bucle principal y del renderizado.
- [ ] Establecer pruebas de regresión básicas.

### Prioridad 2 — Mejoras de uso inmediato

- [ ] Implementar el menú de opciones.
- [ ] Añadir cambio de mandos en caliente.
- [ ] Implementar perfiles y ranuras de guardado.
- [ ] Añadir aceleración de diálogos y modo de tablero rápido.
- [ ] Incorporar la precarga de recursos.

### Prioridad 3 — Mejoras gráficas

- [ ] Implementar la relación de aspecto dinámica.
- [ ] Separar renderizado y lógica para soportar altas frecuencias de refresco.
- [ ] Añadir soporte para texturas HD.
- [ ] Validar la interfaz 2D en distintas resoluciones.

### Prioridad 4 — Funcionalidades extendidas

- [ ] Implementar el sistema de logros.
- [ ] Revisar y corregir el sistema de dificultad de la CPU.
- [ ] Diseñar la interfaz del modo online.
- [ ] Definir y validar la arquitectura de red.
- [ ] Implementar el modo online por fases.

---

## 7. Criterios generales de calidad

- Mantener la identidad visual y sonora de Wii Party.
- Evitar cambios en las reglas originales salvo que una opción lo indique expresamente.
- Separar las mejoras de PC de la lógica recompilada del juego siempre que sea posible.
- Documentar cada modificación relevante.
- Incorporar pruebas de regresión para las funciones afectadas.
- Priorizar la estabilidad sobre la incorporación de nuevas características.
- Evitar dependencias innecesarias y mantener tiempos de compilación razonables.
- Registrar errores y estados de depuración de forma estructurada.
