# osu!3DS 🌸

**osu!3DS** es un port nativo y completo de **osu!** para la familia de consolas **Nintendo 3DS, 2DS, New 3DS y New 2DS**, programado desde cero en C++20 con `libctru`, `Citro2D` y `Citro3D`.

El juego está ultra-optimizado para funcionar a **60 FPS estables** incluso en la consola original (Old 3DS), aprovechando al máximo la doble pantalla, el stylus táctil y el **efecto 3D estereoscópico real**.

---

## ✨ Características Principales

### 🎮 Los 4 Modos de Juego de osu!
1. **osu!standard (STD):**
   - **Modo Táctil Directo:** Toca círculos, sliders y spinners con el stylus directamente en la pantalla inferior.
   - **Modo Tablet Virtual:** Juega viendo la pantalla superior mientras usas la pantalla táctil como una tableta gráfica virtual.
   - Sliders curvados Bézier renderizados por hardware GPU, animación de serpiente (*snake sliders*), approach circles precisos y spinners.
2. **osu!taiko:**
   - Carril de notas dinámico con Don (rojo), Katsu (azul), Drumrolls (redobles amarillos) y Denden (spinner gigante).
   - Mascota animada **Don-chan** que salta al compás del BPM de la canción.
   - Tambor interactivo táctil de 4 zonas en la pantalla inferior y compatibilidad completa con botones físicos (Cruceta, ABXY, Gatillos L/R).
   - Fondo alternable: cambia entre el festival japonés animado con linternas y fuegos artificiales o el fondo/video original del beatmap.
3. **osu!catch (Catch the Beat):**
   - Atrapa frutas, gotas pequeñas (*droplets*) y bananas con el receptor y plato.
   - Efecto visual de **Hyperdash** con destellos y estela roja de aceleración extrema.
   - Control con Circle Pad analógico, Cruceta o botones táctiles.
4. **osu!mania:**
   - Modos 4K y 7K con iluminación de columna y partículas de impacto al pulsar cada nota.
   - Mallas 3D para notas largas (*hold notes*).
   - Ajuste de **Scroll Speed al vuelo** con la cruceta o gatillos.

---

### 👓 Efecto 3D Estereoscópico Real
- Aprovecha el deslizador **3D Slider** de la pantalla superior de la Nintendo 3DS.
- **Profundidad de capas dinámica:** Las notas viajan desde el fondo del túnel 3D hacia el jugador, y los popups de juicio (`300`, `100`, `50`, `Miss`) flotan fuera de la pantalla.

### 🎬 Soporte para Videos de Fondo en Beatmaps
- Reproducción fluida de videos musicales en formato adaptado a la 3DS, sincronizados al milisegundo con la pista de audio.
- Incluye el script `convertir_videos_3ds.ps1` para optimizar cualquier video con un solo clic.

### ⌨️ Teclado Táctil QWERTY y Filtros en Vivo
- Buscador interactivo en la pantalla inferior que filtra tu biblioteca de canciones **en tiempo real mientras escribes** con el stylus, sin pausar el juego ni congelar pantallas.
- **Chips de filtro rápido:** Filtra instantáneamente por modo (`[STD]`, `[TAIKO]`, `[CATCH]`, `[MANIA]`), dificultades mínimas (`[*4+]`, `[*5+]`) o restablece con `[TODOS]`.

### ⚡ 15 Modificadores de Juego (Mods)
- **Dificultad y Velocidad:** Easy (EZ), NoFail (NF), HalfTime (0.75x HT), HardRock (HR), SuddenDeath (SD), Perfect (PF), DoubleTime (1.5x DT), Nightcore (NC).
- **Visuales y Asistencia:** Hidden (HD), Flashlight (FL), SpunOut (SO), Relax (RX), AutoPilot (AP), Auto y Cinema.

### 📊 Récords Locales y Sistema de PP
- Cálculo en tiempo real de **PP (Performance Points)** según estrellas, precisión y mods.
- Guardado automático de los mejores puntajes locales en la tarjeta SD por canción y modo.
- Pantalla de resultados detallada estilo *osu!lazer* con desglose de aciertos, combo máximo, gráfica de densidad de esfuerzo (*strain graph*) y grado (SS, S, A, B, C, D, F).

### 📡 Servidor Wi-Fi y Carga Inalámbrica
- Servidor web integrado: transfiere canciones `.osz` o carpetas descomprimidas directamente desde el navegador de tu PC o smartphone sin tener que extraer la tarjeta SD.

---

## 📥 Instalación

### Requisitos
- Una consola de la familia Nintendo 3DS / 2DS con Custom Firmware (Luma3DS).
- Tarjeta SD con espacio para tus canciones.

### Métodos de Instalación:
1. **Formato CIA (Recomendado):**
   - Copia `osu-3ds.cia` a tu tarjeta SD e instálalo con **FBI**.
   - Aparecerá directamente en el Menú HOME de tu consola con su icono y banner musical propio.
2. **Formato 3DSX (Homebrew Launcher):**
   - Copia `osu-3ds.3dsx` y `osu-3ds.smdh` a la carpeta `sdmc:/3ds/osu-3ds/`.
   - Ejecútalo desde el **Homebrew Launcher**.

---

## 🎵 Cómo Añadir Canciones (Beatmaps)

Puedes colocar tus canciones en formato `.osz` o como carpetas descomprimidas en cualquiera de las siguientes rutas de tu tarjeta SD:
- `sdmc:/3ds/osu-3ds/songs/`
- `sdmc:/osu-3ds/songs/`

> **Nota:** Al abrir el juego, cualquier archivo `.osz` colocado en estas carpetas será extraído y organizado automáticamente. También puedes pulsar **[Y]** en el menú de canciones para activar el servidor Wi-Fi y subirlas desde tu navegador web.

---

## 🕹️ Controles Principales

| Botón | Acción |
| :--- | :--- |
| **Stylus / Táctil** | Tocar círculos/sliders, usar teclado de búsqueda, navegar menús |
| **A / Touch** | Seleccionar canción / Iniciar partida |
| **B** | Volver al menú anterior / Cerrar buscador |
| **Cruceta Arriba / Abajo** | Navegar lista de canciones |
| **Cruceta Izquierda / Derecha** | Cambiar dificultad del mapa |
| **SELECT** | Abrir/Cerrar menú de Mods |
| **START** | Cambiar criterio de ordenación (Título, Artista, Estrellas, PP) |
| **X** | Menú de Ajustes / Calibración de Audio Offset |
| **Y** | Activar servidor de transferencia Wi-Fi |
| **ZL / ZR** | Abrir teclado táctil interactivo de búsqueda |

---

## 🛠️ Compilación desde el Código Fuente

El proyecto utiliza **devkitARM** y **libctru**.

```bash
# Clonar el repositorio
git clone https://github.com/tu-usuario/osu-3ds.git
cd osu-3ds

# Compilar binarios (.3dsx, .elf, .cia)
make -j4
```

---

## 💖 Créditos y Agradecimientos
- **osu!** es propiedad de *ppy Pty Ltd* y Dean Herbert (peppy).
- Librerías homebrew: *devkitPro*, *libctru*, *Citro2D*, *Citro3D*, *bannertool*, *makerom*.
=======
# osu! 3DS (v2.0)

Port personalizado del popular juego de ritmo **osu!** para la familia de consolas **Nintendo 3DS / 2DS**, diseñado con experiencia de juego tipo tableta digitalizadora y compatibilidad nativa con beatmaps oficiales.

---

### 📲 Instalación rápida (.CIA)

Escanea el siguiente código QR desde la aplicación **FBI** en tu consola (`Remote Install` ➔ `Scan QR Code`) para descargar e instalar el archivo `.cia` directamente vía Wi-Fi:

<div align="center">
 <img width="1113" height="1113" alt="Osu V3 0" src="https://github.com/user-attachments/assets/dbb37071-697e-4ec8-aa1e-6b1535820442" />


  <p><i>Requiere conexión Wi-Fi activa y Custom Firmware (Luma3DS).</i></p>
</div>

---

## 🌟 Características Principales (v2.0)

* **Modo Tableta (Stylus Digitizer):** Mapeo de coordenadas absolutas en relación 4:3. La pantalla táctil inferior actúa como el área activa de una tableta gráfica, mientras que el campo de juego principal se proyecta en la pantalla superior.
* **Descarga y Gestión de Canciones Integrada:** Sistema para transferir y almacenar beatmaps (`.osu` / `.osz`) directamente en la tarjeta SD (`sdmc:/3ds/osu/Songs/`) sin necesidad de recompilar el juego.
* **Sincronización por Hardware (DSP):** Reloj de juego anclado al conteo de muestras de audio mediante el procesador DSP (`ndsp`) y decodificación nativa con `minimp3`.
* **Física y Matemáticas Oficiales:** Implementación de fórmulas originales de osu! para:
  * Radio de notas y seguimiento según *Circle Size* (CS).
  * Ventana de aparición y reacción según *Approach Rate* (AR).
  * Ventanas de impacto y precisión (`300`, `100`, `50`) según *Overall Difficulty* (OD).
* **Sliders Interactivos:** Renderizado de curvas Bézier suaves, cálculo de repeticiones y seguimiento en tiempo real con *Follow Circle* y *Slider Ball*.
* **Feedback Visual Completo:** Colores dinámicos de combo, numeración en cada nota, popups de puntuación flotantes sobre los círculos y *Key Overlay* en pantalla.
* **Control Híbrido:** Posibilidad de golpear notas tanto presionando botones físicos (`L`, `ZL`, Cruceta, `A`, `B`) como haciendo *tap* directo con el stylus en la pantalla táctil.

---

## 🎮 Controles

| Entrada | Acción |
| :--- | :--- |
| **Stylus (Pantalla Táctil)** | Posicionar el cursor en la pantalla superior / Golpear notas (*Tap*) |
| **Gatillo L / D-Pad Izquierda / Botón A** | Tecla de impacto 1 (K1) |
| **Gatillo ZL / D-Pad Abajo / Botón B** | Tecla de impacto 2 (K2) |
| **Cruceta Arriba / Abajo** | Navegar en el menú selector de dificultad |
| **Botón A** | Seleccionar canción / Iniciar partida |
| **Botón SELECT** | Pausar y volver al menú principal de canciones |
| **Botón START** | Salir de la aplicación al menú HOME / Homebrew Launcher |

---

🛠️ Tecnologías Utilizadas
Toolchain: devkitPro (devkitARM, libctru).

Gráficos 2D: Citro2D / Citro3D.

Audio y Sincronización: DSP (ndsp) de Nintendo 3DS junto a minimp3.

Formato de Empaquetado: .3dsx (Homebrew Launcher) y .cia (Menú HOME nativo).

📜 Créditos y Reconocimientos
Concepto y diseño original: ppy (Dean Herbert) y el equipo de osu!.

Desarrollo del port para 3DS: Acira345.

Toolchain y librerías base: Comunidad de devkitPro.

## 📂 Estructura de Canciones en la Tarjeta SD

Si prefieres transferir canciones manualmente a tu tarjeta SD, descomprime tus archivos `.osz` (formato ZIP estándar) en carpetas individuales dentro de la siguiente ruta:

```text
sdmc:/
└── 3ds/
    └── osu/
        └── Songs/
            └── NombreDeLaCancion/
                ├── audio.mp3
                └── cancion.osu

