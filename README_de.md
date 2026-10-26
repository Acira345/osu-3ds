[Español](README.md) · [English](README_en.md) · [Polski](README_pl.md) · [Русский](README_ru.md) · **Deutsch**

# osu!3DS 🌸

**osu!3DS** ist ein vollständiger, native Port von **osu!** für die **3DS**-Konsolenfamilie (Nintendo 3DS, 2DS, New 3DS und New 2DS). Sie wurde von Grund auf in C++20 unter Verwendung von `libctru`, `Citro2D` und `Citro3D` entwickelt.

Das Spiel ist hochgradig optimiert, um selbst auf dem ursprünglichen Konsolenmodell (Old 3DS) mit **stabilen 60 FPS** zu laufen. Es nutzt dabei das Dual-Screen-Konzept, den Touch-Stylus sowie den **echten stereoskopischen 3D-Effekt** voll aus.

---

### 📲 Schnelle Installation (.CIA)

Scanne den untenstehenden QR-Code mit der Anwendung **FBI** auf deiner Konsole (`Remote Install` ➔ `Scan QR Code`), um die `.cia`-Datei direkt über WLAN herunterzuladen und zu installieren:

<div align="center">
<img width="1113" height="1113" alt="Osu V3 5" src="https://github.com/user-attachments/assets/3d7de3cd-86f1-4298-a2df-218b79590a10" />

<p><i>Erfordert eine aktive WLAN-Verbindung sowie eine Custom Firmware (Luma3DS).</i></p>
</div>

---
---

## ✨ Hauptmerkmale

### 🎮 Die 4 osu!-Spielmodi
1. **osu!standard (STD):**
   - **Direct-Touch-Modus:** Tippe mit dem Stylus direkt auf dem unteren Bildschirm auf Kreise, Slider und Spinner. 
   - **Virtueller Tablet-Modus:** Spiele, indem du den oberen Bildschirm beobachtest, während du den Touchscreen als virtuelles Grafiktablett nutzt. 
   - GPU-gerenderte, auf Bézier-Kurven basierende Slider, schlangenartige Animationen (*Snake-Slider*), präzise Approach-Circles und Spinner.
2. **osu!taiko:**
   - Dynamische Notenbahn mit Don (rot), Katsu (blau), Trommelwirbeln (gelb) und Denden (riesige Spinner). 
   - Animiertes Maskottchen **Don-chan**, das im Takt des Liedes springt.
   - Interaktives, in vier Zonen unterteiltes Touch-Trommelfeld auf dem unteren Bildschirm; vollständige Unterstützung für physische Bedienelemente (Steuerkreuz, ABXY-Tasten sowie L/R-Schultertasten).
   - Umschaltbarer Hintergrund: Wechsel zwischen einem japanischen Fest-Szenario – inklusive Lampions und Feuerwerk – und dem ursprünglichen Hintergrund oder Video der jeweiligen Beatmap.
3. **osu!catch (Catch the Beat):**
   - Fange Früchte, kleine Tropfen (*Droplets*) und Bananen mit einer Auffangvorrichtung.
   - **Hyperdash**-Effekt: Visuelle Darstellung durch Lichtblitze und eine rote Spur, die eine extreme Beschleunigung signalisiert.
   - Steuerung über das analoge Schiebepad, das Steuerkreuz oder per Touch-Eingabe möglich.
4. **osu!mania:**
   - 4K- und 7K-Modi mit beleuchteten Spalten und Partikeleffekten, die beim Treffen einer Note ausgelöst werden.
   - 3D-Modelle für gehaltene Noten (*Hold Notes*).
   - **Anpassung der Scroll-Geschwindigkeit** während des Spiels über das Steuerkreuz oder die Schultertasten.

---

### 👓 Echter stereoskopischer 3D-Effekt
- Nutzt den **3D-Regler** des Nintendo 3DS (für den oberen Bildschirm).
- **Dynamische Ebenentiefe:** Noten bewegen sich aus der Tiefe des 3D-Raums auf den Spieler zu, während die eingeblendeten Wertungen (`300`, `100`, `50`, `Miss`) so wirken, als würden sie aus dem Bildschirm herausragen.

### 🎬 Unterstützung für Hintergrundvideos in Beatmaps
- Flüssige Wiedergabe von Musikvideos in einem für den 3DS optimierten Format, millisekundengenau mit der Audiospur synchronisiert.
- Enthält das Skript `convert_videos_3ds.ps1`, mit dem sich jedes Video per Mausklick für den 3DS optimieren lässt.

### ⌨️ QWERTY-Touch-Tastatur und Echtzeit-Filter
- Ein interaktives Suchwerkzeug auf dem unteren Bildschirm, das deine Musikbibliothek **in Echtzeit während der Eingabe** (per Touchpen) filtert – ohne das Spiel zu unterbrechen oder den Bildschirm einfrieren zu lassen. - **Schnellfilter-Schaltflächen:** Filtere sofort nach Spielmodus (`[STD]`, `[TAIKO]`, `[CATCH]`, `[MANIA]`) oder Mindestschwierigkeitsgrad (`[*4+]`, `[*5+]`) – oder setze die Ansicht mit `[ALL]` zurück.

### ⚡ 15 Gameplay-Modifikatoren (Mods)
- **Schwierigkeit und Geschwindigkeit:** Easy (EZ), NoFail (NF), HalfTime (0,75x HT), HardRock (HR), SuddenDeath (SD), Perfect (PF), DoubleTime (1,5x DT), Nightcore (NC).
- **Visualisierung und Hilfen:** Hidden (HD), Flashlight (FL), SpunOut (SO), Relax (RX), AutoPilot (AP), Auto und Cinema.

### 📊 Lokale Rekorde und PP-System
- Echtzeit-Berechnung von **PP (Performance Points)** basierend auf Sternebewertung, Genauigkeit (Accuracy) und aktiven Mods.
- Automatische Speicherung der besten lokalen Ergebnisse auf der SD-Karte – getrennt nach Song und Spielmodus.
- Detaillierter Ergebnisbildschirm im *osu!lazer*-Stil: mit Aufschlüsselung der Treffer, maximaler Combo, Belastungsdiagramm (Strain Graph) und abschließender Bewertung (SS, S, A, B, C, D, F).

### 📡 WLAN-Server und kabellose Übertragung
- Integrierter Webserver: Übertrage `.osz`-Songs oder entpackte Ordner direkt über den Browser deines PCs oder Smartphones – ganz ohne die SD-Karte entnehmen zu müssen.

---

## 📥 Installation

### Voraussetzungen
- Eine Konsole der Nintendo 3DS-/2DS-Familie mit Custom Firmware (Luma3DS).
- Eine SD-Karte mit ausreichend Speicherplatz für deine Songs.

### Installationsmethoden:
1. **CIA-Format (Empfohlen):**
    - Kopiere die Datei `osu-3ds.cia` auf deine SD-Karte und installiere sie mithilfe von **FBI**. 
    - Die Anwendung erscheint anschließend direkt im HOME-Menü deiner Konsole – inklusive eigenem Symbol und musikalischem Banner.
2. **3DSX-Format (Homebrew Launcher):**
    - Kopiere die Dateien `osu-3ds.3dsx` und `osu-3ds.smdh` in den Ordner `sdmc:/3ds/osu-3ds/`. 
    - Starte die Anwendung über den **Homebrew Launcher**.

---

## 🎵 Songs (Beatmaps) hinzufügen

Du kannst deine Songs im `.osz`-Format oder als entpackte Ordner in einem der folgenden Verzeichnisse auf deiner SD-Karte ablegen:
- `sdmc:/3ds/osu-3ds/songs/`
- `sdmc:/osu-3ds/songs/`

> **Hinweis:** Beim Starten des Spiels werden alle `.osz`-Dateien, die sich in diesen Ordnern befinden, automatisch entpackt und sortiert. Alternativ kannst du im Song-Menü die Taste **[Y]** drücken, um den WLAN-Server zu aktivieren und Songs direkt über deinen Webbrowser hochzuladen.

---

## 🕹️ Grundlegende Steuerung

| Taste | Aktion |
| :--- | :--- |
| **Stylus / Touchscreen** | Kreise/Slider antippen, Suchtastatur bedienen, durch Menüs navigieren |
| **A / Touchscreen** | Song auswählen / Spiel starten |
| **B** | Zum vorherigen Menü zurückkehren / Suchleiste schließen |
| **Steuerkreuz (oben/unten)** | Durch die Songliste navigieren |
| **Steuerkreuz (links/rechts)** | Schwierigkeitsgrad der Map ändern |
| **SELECT** | Mods-Menü öffnen/schließen |
| **START** | Sortierkriterium ändern (Titel, Interpret, Sterne-Bewertung, PP) |
| **X** | Einstellungsmenü / Audio-Offset-Kalibrierung |
| **Y** | WLAN-Übertragungsserver aktivieren |
| **ZL / ZR** | Bildschirmtastatur für die Suche öffnen |

---

## 🛠️ Kompilieren aus dem Quellcode

Das Projekt verwendet **devkitARM** und **libctru**.

```bash
# Clone the repository
git clone https://github.com/Acira345/osu-3ds.git
cd osu-3ds

# Compile binaries (.3dsx, .elf, .cia)
make -j4
```

---

## 💖 Credits und Danksagungen
- **osu!** ist Eigentum von *ppy Pty Ltd* und Dean Herbert (peppy).
- Homebrew-Bibliotheken: *devkitPro*, *libctru*, *Citro2D*, *Citro3D*, *bannertool*, *makerom*.
=======
# osu! 3DS (v2.0)

Ein benutzerdefinierter Port des beliebten Rhythmusspiels **osu!** für die **Nintendo 3DS / 2DS**-Konsolenfamilie, der ein Gameplay im Stil eines Grafiktabletts bietet und nativ mit offiziellen Beatmaps kompatibel ist.


## 🌟 Hauptfunktionen (v2.0)

* **Tablet-Modus (Stylus-Eingabe):** Absolute Koordinatenzuordnung im 4:3-Seitenverhältnis. Der untere Touchscreen fungiert als aktive Fläche eines Grafiktabletts, während das eigentliche Spielgeschehen auf dem oberen Bildschirm angezeigt wird.
* **Integrierter Song-Download und -Verwaltung:** Ein System zum Übertragen und Speichern von Beatmaps (`.osu` / `.osz`) direkt auf der SD-Karte (`sdmc:/3ds/osu/Songs/`), ohne dass das Spiel neu kompiliert werden muss.
* **Hardware-Synchronisation (DSP):** Der Spieltakt wird über den DSP-Prozessor (`ndsp`) mit der Audio-Sample-Anzahl synchronisiert; die Dekodierung erfolgt nativ mittels `minimp3`.
* **Originalgetreue Physik und Mathematik:** Implementierung der ursprünglichen *osu!*-Formeln für:
* Notenradius und Tracking basierend auf der *Circle Size* (CS). * Erscheinen und Timing basieren auf der *Approach Rate* (AR). 
* Trefferfenster (`300`, `100`, `50`) basieren auf der *Overall Difficulty* (OD).
* **Interaktive Slider:** Darstellung flüssiger Bézier-Kurven, Berechnung von Wiederholungen sowie Echtzeit-Verfolgung mittels *Follow Circle* und *Slider Ball*.
* **Umfassendes visuelles Feedback:** Dynamische Combo-Farben, Nummerierung der Noten, aufsteigende Punktanzeigen über den Kreisen sowie eine *Key-Anzeige* (Tasten-Overlay) auf dem Bildschirm.
* **Hybrides Steuerungskonzept:** Noten können entweder durch Drücken physischer Tasten (`L`, `ZL`, Steuerkreuz, `A`, `B`) oder durch direktes Tippen auf den Touchscreen mit dem Eingabestift (Stylus) getroffen werden.

---

## 🎮 Steuerung

| Eingabe | Aktion |
| :--- | :--- |
| **Stylus (Touchscreen)** | Cursor auf dem oberen Bildschirm positionieren / Noten treffen (*Tippen*) |
| **L-Taste / Steuerkreuz links / A-Taste** | Taste 1 (K1) betätigen |
| **ZL-Taste / Steuerkreuz unten / B-Taste** | Taste 2 (K2) betätigen |
| **Steuerkreuz oben / unten** | Im Schwierigkeitsgrad-Menü navigieren |
| **A-Taste** | Lied auswählen / Spiel starten |
| **SELECT-Taste** | Spiel pausieren und zum Hauptmenü zurückkehren |
| **START-Taste** | Anwendung beenden und zum HOME-Menü / Homebrew Launcher zurückkehren |

---

🛠️ Verwendete Technologien
Toolchain: devkitPro (devkitARM, libctru).

2D-Grafik: Citro2D / Citro3D.

Audio und Synchronisation: Nintendo 3DS DSP (ndsp) in Kombination mit minimp3.

Paketformate: .3dsx (Homebrew Launcher) und .cia (natives HOME-Menü). 📜 Credits und Danksagungen
Ursprüngliches Konzept und Design: ppy (Dean Herbert) und das osu!-Team.

Entwicklung des 3DS-Ports: Acira345.

Toolchain und Basis-Bibliotheken: Die devkitPro-Community. 

## 📂 Song-Struktur auf der SD-Karte

Wenn du Songs lieber manuell auf deine SD-Karte übertragen möchtest, entpacke deine `.osz`-Dateien (ein Standard-ZIP-Format) in jeweils eigene Ordner innerhalb des folgenden Pfads:

```text
sdmc:/
└── 3ds/
    └── osu/
        └── Songs/
            └── SongName/
                ├── audio.mp3
                └── song.osu
```