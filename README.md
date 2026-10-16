# osu! 3DS (v2.0)

Port personalizado del popular juego de ritmo **osu!** para la familia de consolas **Nintendo 3DS / 2DS**, diseñado con experiencia de juego tipo tableta digitalizadora y compatibilidad nativa con beatmaps oficiales.

---

### 📲 Instalación rápida (.CIA)

Escanea el siguiente código QR desde la aplicación **FBI** en tu consola (`Remote Install` ➔ `Scan QR Code`) para descargar e instalar el archivo `.cia` directamente vía Wi-Fi:

<div align="center">
  <img width="1113" height="1113" alt="Osu V2 0" src="https://github.com/user-attachments/assets/b9bbb387-ed86-423d-9a82-f9fffcaf4e5a" />

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


