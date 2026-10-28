[Español](README.md) · **English** · [Polski](README_pl.md) · [Русский](README_ru.md) · [Deutsch](README_de.md)

# osu!3DS 🌸

**osu!3DS** is a full, native port of **osu!** for the **Nintendo 3DS, 2DS, New 3DS, and New 2DS** console family, programmed from scratch in C++20 using `libctru`, `Citro2D`, and `Citro3D`.

The game is highly optimized to run at a **stable 60 FPS**, even on the original console model (Old 3DS), making full use of the dual-screen setup, the touch stylus, and the **true stereoscopic 3D effect**.

---

### 📲 Quick Installation (.CIA)

Scan the QR code below using the **FBI** application on your console (`Remote Install` ➔ `Scan QR Code`) to download and install the `.cia` file directly via Wi-Fi:

<div align="center">
<img width="1113" height="1113" alt="Osu V3 5" src="https://github.com/user-attachments/assets/3d7de3cd-86f1-4298-a2df-218b79590a10" />



<p><i>Requires an active Wi-Fi connection and Custom Firmware (Luma3DS).</i></p>
</div>

---
---

## ✨ Key Features

### 🎮 The 4 osu! Game Modes
1. **osu!standard (STD):**
   - **Direct Touch Mode:** Tap circles, sliders, and spinners using the stylus directly on the bottom screen. 
   - **Virtual Tablet Mode:** Play by watching the top screen while using the touch screen as a virtual graphics tablet. 
   - GPU-rendered Bézier-curved sliders, snake-like animation (*snake sliders*), precise approach circles, and spinners.
2. **osu!taiko:**
   - Dynamic note lane featuring Don (red), Katsu (blue), drumrolls (yellow), and Denden (giant spinner). 
   - Animated mascot **Don-chan**, who jumps to the beat of the song. 
   - Interactive 4-zone touch drum on the bottom screen, with full support for physical controls (D-pad, ABXY buttons, and L/R shoulder buttons). 
   - Toggleable background: switch between a Japanese festival setting—complete with lanterns and fireworks—and the beatmap’s original background or video.
3. **osu!catch (Catch the Beat):**
   - Catch fruits, small drops (*droplets*), and bananas using a catcher and plate. 
   - **Hyperdash** visual effect, featuring flashes and a red trail indicating extreme acceleration. 
   - Controllable via the analog Circle Pad, D-pad, or touch-based inputs.
4. **osu!mania:**
   - 4K and 7K modes, featuring column lighting and impact particles triggered upon hitting each note. 
   - 3D meshes for long notes (*hold notes*). 
   - On-the-fly **scroll speed adjustment** using the D-pad or shoulder buttons.

---

### 👓 True Stereoscopic 3D Effect
- Utilizes the **3D Slider** on the Nintendo 3DS’s top screen.
- **Dynamic layer depth:** Notes travel from the back of the 3D space toward the player, while the score popups (`300`, `100`, `50`, `Miss`) appear to float out from the screen. 

### 🎬 Background Video Support for Beatmaps
- Smooth playback of music videos in a format optimized for the 3DS, synchronized to the millisecond with the audio track.
- Includes the `convert_videos_3ds.ps1` script to optimize any video with a single click.

### ⌨️ QWERTY Touch Keyboard and Live Filters
- An interactive search tool on the bottom screen that filters your song library **in real-time as you type** with the stylus, without pausing the game or freezing the screen.
- **Quick-filter chips:** Instantly filter by mode (`[STD]`, `[TAIKO]`, `[CATCH]`, `[MANIA]`) or minimum difficulty (`[*4+]`, `[*5+]`), or reset your view with `[ALL]`.

### ⚡ 15 Gameplay Modifiers (Mods)
- **Difficulty and Speed:** Easy (EZ), NoFail (NF), HalfTime (0.75x HT), HardRock (HR), SuddenDeath (SD), Perfect (PF), DoubleTime (1.5x DT), Nightcore (NC).
- **Visuals and Assistance:** Hidden (HD), Flashlight (FL), SpunOut (SO), Relax (RX), AutoPilot (AP), Auto, and Cinema.

### 📊 Local Records and PP System
- Real-time calculation of **PP (Performance Points)** based on star rating, accuracy, and mods.
- Automatic saving of top local scores to the SD card, tracked per song and per mode.
- Detailed *osu!lazer*-style results screen featuring a breakdown of hits, maximum combo, a strain graph, and a final grade (SS, S, A, B, C, D, F). 

### 📡 Wi-Fi Server and Wireless Loading
- Built-in web server: transfer `.osz` songs or unzipped folders directly from your PC or smartphone browser, without needing to remove the SD card.

---

## 📥 Installation

### Requirements
- A Nintendo 3DS / 2DS family console with Custom Firmware (Luma3DS).
- An SD card with enough space for your songs.

### Installation Methods:
1. **CIA Format (Recommended):**
   - Copy `osu-3ds.cia` to your SD card and install it using **FBI**. 
   - It will appear directly on your console's HOME Menu, complete with its own icon and musical banner.
2. **3DSX Format (Homebrew Launcher):**
   - Copy `osu-3ds.3dsx` and `osu-3ds.smdh` to the `sdmc:/3ds/osu-3ds/` folder. 
   - Launch it via the **Homebrew Launcher**.

---

## 🎵 How to Add Songs (Beatmaps)

You can place your songs in `.osz` format or as unzipped folders in any of the following paths on your SD card:
- `sdmc:/3ds/osu-3ds/songs/`
- `sdmc:/osu-3ds/songs/`

> **Note:** When you open the game, any `.osz` files placed in these folders will be automatically extracted and organized. You can also press **[Y]** in the song menu to activate the Wi-Fi server and upload songs from your web browser.

---

## 🕹️ Main Controls

| Button | Action |
| :--- | :--- |
| **Stylus / Touch** | Tap circles/sliders, use the search keyboard, navigate menus |
| **A / Touch** | Select song / Start game |
| **B** | Return to previous menu / Close search bar |
| **D-pad Up / Down** | Navigate song list |
| **D-pad Left / Right** | Change map difficulty |
| **SELECT** | Open/Close Mods menu |
| **START** | Change sorting criteria (Title, Artist, Star Rating, PP) |
| **X** | Settings menu / Audio Offset calibration |
| **Y** | Activate Wi-Fi transfer server |
| **ZL / ZR** | Open interactive on-screen search keyboard |

---

## 🛠️ Compiling from Source Code

The project uses **devkitARM** and **libctru**. 

```bash
# Clone the repository
git clone https://github.com/Acira345/osu-3ds.git
cd osu-3ds

# Compile binaries (.3dsx, .elf, .cia)
make -j4
```

---

## 💖 Credits and Acknowledgments
- **osu!** is the property of *ppy Pty Ltd* and Dean Herbert (peppy).
- Homebrew libraries: *devkitPro*, *libctru*, *Citro2D*, *Citro3D*, *bannertool*, *makerom*.
=======

# osu! 3DS (v2.0)

A custom port of the popular rhythm game **osu!** for the **Nintendo 3DS / 2DS** console familiy, designed to provide a digitizer - tablet-style gameplay experience with native compatibility for official beatmaps.


## 🌟 Key Features (v2.0)

* **Tablet Mode (Stylus Digitizer):** Absolute coordinate mapping in a 4:3 aspect ratio. The bottom touchscreen functions as the active area of ​​a graphics tablet, while the main gameplay area is displayed on the top screen.
* **Integrated Song Download and Management:** A system for transferring and storing beatmaps (`.osu` / `.osz`) directly onto the SD card (`sdmc:/3ds/osu/Songs/`), with no need to recompile the game.
* **Hardware Synchronization (DSP):** Game clock synchronized to audio sample counts via the DSP (`ndsp`) processor, with native decoding using `minimp3`.
* **Official Physics and Mathematics:** Implementation of original *osu!* formulas for:
    * Note radius and tracking based on *Circle Size* (CS). 
    * Appearance and reaction timing based on *Approach Rate* (AR). 
    * Hit windows (`300`, `100`, `50`) based on *Overall Difficulty* (OD).
* **Interactive Sliders:** Rendering of smooth Bézier curves, repeat calculations, and real-time tracking using a *Follow Circle* and *Slider Ball*.
* **Comprehensive Visual Feedback:** Dynamic combo colors, numbering on each note, floating score popups above circles, and an on-screen *Key Overlay*.
* **Hybrid Control Scheme:** Notes can be hit either by pressing physical buttons (`L`, `ZL`, D-Pad, `A`, `B`) or by tapping directly on the touchscreen with the stylus.

---

## 🎮 Controls

| Input | Action |
| :--- | :--- |
| **Stylus (Touchscreen)** | Position the cursor on the top screen / Hit notes (*Tap*) |
| **L Trigger / D-Pad Left / A Button** | Hit Key 1 (K1) |
| **ZL Trigger / D-Pad Down / B Button** | Hit Key 2 (K2) |
| **D-Pad Up / Down** | Navigate the difficulty selection menu |
| **A Button** | Select song / Start game |
| **SELECT Button** | Pause and return to the main song menu |
| **START Button** | Exit the application to the HOME Menu / Homebrew Launcher |

---

🛠️ Technologies Used
Toolchain: devkitPro (devkitARM, libctru).

2D Graphics: Citro2D / Citro3D.

Audio and Synchronization: Nintendo 3DS DSP (ndsp) combined with minimp3.

Packaging Formats: .3dsx (Homebrew Launcher) and .cia (Native HOME Menu). 📜 Credits and Acknowledgments
Original concept and design: ppy (Dean Herbert) and the osu! team.

3DS port development: Acira345.

Toolchain and base libraries: The devkitPro community.

## 📂 Song Structure on the SD Card

If you prefer to transfer songs manually to your SD card, extract your `.osz` files (standard ZIP format) into individual folders within the following path:

```text
sdmc:/
└── 3ds/
    └── osu/
        └── Songs/
            └── SongName/
                ├── audio.mp3
                └── song.osu
```