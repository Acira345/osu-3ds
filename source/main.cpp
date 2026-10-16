#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>
#include "Beatmap.hpp"
#include "AudioPlayer.hpp"
#include "SongManager.hpp"
#include "Settings.hpp"

constexpr float SCALE_FACTOR = 0.625f;
constexpr float TOP_OFFSET_X = 40.0f;

inline float osuToTopX(float osuX) { return TOP_OFFSET_X + (osuX * SCALE_FACTOR); }
inline float osuToTopY(float osuY) { return osuY * SCALE_FACTOR; }

enum AppState {
    STATE_MAIN_MENU,
    STATE_SETTINGS,
    STATE_SONG_SELECT,
    STATE_PLAYING,
    STATE_RESULTS
};

struct HitPopup {
    float x;
    float y;
    HitState state;
    int timer;
    HitPopup(float _x = 0.0f, float _y = 0.0f, HitState _s = HIT_300, int _t = 0)
        : x(_x), y(_y), state(_s), timer(_t) {}
};

void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24) {
    const float step = (2.0f * static_cast<float>(M_PI)) / segments;
    for (int i = 0; i < segments; ++i) {
        float a0 = i * step;
        float a1 = (i + 1) * step;
        C2D_DrawLine(
            x + std::cos(a0) * radius, y + std::sin(a0) * radius, color,
            x + std::cos(a1) * radius, y + std::sin(a1) * radius, color,
            thickness, 0.5f
        );
    }
}

void drawRectOutline(float x, float y, float w, float h, float thickness, u32 color) {
    C2D_DrawLine(x, y, color, x + w, y, color, thickness, 0.5f);
    C2D_DrawLine(x + w, y, color, x + w, y + h, color, thickness, 0.5f);
    C2D_DrawLine(x + w, y + h, color, x, y + h, color, thickness, 0.5f);
    C2D_DrawLine(x, y + h, color, x, y, color, thickness, 0.5f);
}

void drawText(C2D_TextBuf buf, float x, float y, float scale, u32 color, const char* str) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.85f, scale, scale, color);
}

void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &w, &h);
    C2D_DrawText(&text, C2D_WithColor, cx - (w * 0.5f), cy - (h * 0.5f), 0.85f, scale, scale, color);
}

std::string truncateString(const std::string& str, size_t maxLen) {
    if (str.size() <= maxLen) return str;
    return str.substr(0, maxLen - 3) + "...";
}

void drawStatBar(C2D_TextBuf buf, float x, float y, const char* label, float val, u32 fillClr, u32 bgClr, u32 txtClr) {
    char str[32];
    std::snprintf(str, sizeof(str), "%s: %.1f", label, val);
    drawText(buf, x, y, 0.42f, txtClr, str);

    float barX = x + 62.0f;
    float barY = y + 3.0f;
    float barW = 110.0f;
    float barH = 7.0f;
    float fillRatio = std::max(0.0f, std::min(1.0f, val / 10.0f));

    C2D_DrawRectSolid(barX, barY, 0.2f, barW, barH, bgClr);
    C2D_DrawRectSolid(barX, barY, 0.3f, barW * fillRatio, barH, fillClr);
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    romfsInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topScreen    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf textBuf = C2D_TextBufNew(3072);

    u32 clrBgDark     = C2D_Color32(18, 16, 26, 255);
    u32 clrPanelBg    = C2D_Color32(30, 27, 44, 255);
    u32 clrPlayfield  = C2D_Color32(26, 24, 38, 255);
    u32 clrTabletBg   = C2D_Color32(22, 20, 30, 255);
    u32 clrCursor     = C2D_Color32(255, 220, 80, 255);
    u32 clrWhite      = C2D_Color32(255, 255, 255, 255);
    u32 clrMuted      = C2D_Color32(170, 165, 195, 255);
    u32 clrKeyIdle    = C2D_Color32(45, 42, 60, 255);
    u32 clrKeyActive  = C2D_Color32(245, 110, 165, 255);
    u32 clr300        = C2D_Color32(80, 220, 255, 255);
    u32 clr100        = C2D_Color32(100, 240, 120, 255);
    u32 clr50         = C2D_Color32(230, 190, 80, 255);
    u32 clrMiss       = C2D_Color32(240, 70, 70, 255);

    u32 defaultComboColors[4] = {
        C2D_Color32(255, 115, 180, 235),
        C2D_Color32(90, 235, 140, 235),
        C2D_Color32(70, 180, 255, 235),
        C2D_Color32(255, 180, 70, 235)
    };
    u32 defaultSliderColors[4] = {
        C2D_Color32(170, 65, 115, 150),
        C2D_Color32(50, 150, 90, 150),
        C2D_Color32(45, 115, 175, 150),
        C2D_Color32(175, 115, 45, 150)
    };

    GameSettings settings;
    settings.load();

    SongManager songMgr;
    songMgr.initAndScan();

    Beatmap beatmap;
    AudioPlayer audio;
    audio.initDSPAndHitsounds();
    audio.setVolumes(settings.musicVolume, settings.sfxVolume);

    AppState appState = STATE_MAIN_MENU;
    AppState settingsReturnState = STATE_MAIN_MENU;

    int mainMenuOption = 0; // 0: Jugar, 1: Configuraciones, 2: Salir
    int settingsOption = 0; // 0..7
    int selectedSet = 0;
    int selectedDiff = 0;

    // Estado del asistente de calibración en Configuraciones
    bool calibrationMode = false;
    u64 calibStartTick = 0;
    int calibLastBeatIndex = -1;
    std::vector<int> calibSamples;

    // Estado de juego activo
    bool isPaused = false;
    int pauseOption = 0; // 0: Continuar, 1: Reintentar, 2: Salir al selector
    bool stageFailed = false;
    float hp = 1.0f;

    float cursorOsuX = 256.0f;
    float cursorOsuY = 192.0f;
    int combo = 0;
    int maxCombo = 0;
    int score = 0;
    int count300 = 0, count100 = 0, count50 = 0, countMiss = 0;
    std::vector<HitPopup> popups;
    int animTick = 0;

    auto startSelectedBeatmap = [&]() -> bool {
        if (songMgr.sets.empty()) return false;
        const auto& curSet = songMgr.sets[selectedSet];
        if (curSet.difficulties.empty()) return false;
        const auto& diffEntry = curSet.difficulties[selectedDiff];

        if (!beatmap.loadFromFile(diffEntry.osuPath, false)) return false;

        audio.setVolumes(settings.musicVolume, settings.sfxVolume);
        audio.playHitSound(2);

        int leadIn = std::max(0, beatmap.audioLeadIn);
        if (!beatmap.circles.empty()) {
            int firstObjTime = beatmap.circles.front().time;
            int minStartWait = static_cast<int>(beatmap.diff.preempt) + 500;
            if (firstObjTime < minStartWait) {
                leadIn = std::max(leadIn, minStartWait - firstObjTime);
            }
        }

        audio.startMusic(beatmap.fullAudioPath, leadIn);
        combo = 0;
        maxCombo = 0;
        score = 0;
        count300 = count100 = count50 = countMiss = 0;
        hp = 1.0f;
        stageFailed = false;
        isPaused = false;
        pauseOption = 0;
        cursorOsuX = 256.0f;
        cursorOsuY = 192.0f;
        popups.clear();
        appState = STATE_PLAYING;
        return true;
    };

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        circlePosition cpad;
        hidCircleRead(&cpad);

        touchPosition touch;
        bool isTouching = (kHeld & KEY_TOUCH) != 0;
        bool touchJustDown = (kDown & KEY_TOUCH) != 0;
        if (isTouching) {
            hidTouchRead(&touch);
        }

        animTick++;
        C2D_TextBufClear(textBuf);

        // =========================================================
        // 1. MENÚ PRINCIPAL (Jugar / Configuraciones / Salir)
        // =========================================================
        if (appState == STATE_MAIN_MENU) {
            if (kDown & KEY_START) break;

            if (kDown & KEY_DUP) {
                mainMenuOption = (mainMenuOption + 2) % 3;
                audio.playHitSound(0);
            }
            if (kDown & KEY_DDOWN) {
                mainMenuOption = (mainMenuOption + 1) % 3;
                audio.playHitSound(0);
            }

            bool activateOption = (kDown & KEY_A) != 0;

            // Soporte táctil para los botones del menú en la pantalla inferior
            if (touchJustDown) {
                if (touch.px >= 40 && touch.px <= 280) {
                    if (touch.py >= 30 && touch.py <= 74) {
                        mainMenuOption = 0;
                        activateOption = true;
                    } else if (touch.py >= 86 && touch.py <= 130) {
                        mainMenuOption = 1;
                        activateOption = true;
                    } else if (touch.py >= 142 && touch.py <= 186) {
                        mainMenuOption = 2;
                        activateOption = true;
                    }
                }
            }

            if (activateOption) {
                audio.playHitSound(2);
                if (mainMenuOption == 0) {
                    appState = STATE_SONG_SELECT;
                } else if (mainMenuOption == 1) {
                    settingsReturnState = STATE_MAIN_MENU;
                    calibrationMode = false;
                    appState = STATE_SETTINGS;
                } else if (mainMenuOption == 2) {
                    break;
                }
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: LOGO OSU! 3DS Y OPCIONES ---
            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            // Círculo principal estilo osu! con pulso suave
            float pulse = std::sin(animTick * 0.07f) * 4.5f;
            float logoX = 115.0f;
            float logoY = 120.0f;
            float logoR = 62.0f + pulse;

            drawRing(logoX, logoY, logoR + 9.0f, 2.0f, C2D_Color32(255, 115, 180, 90), 32);
            C2D_DrawCircleSolid(logoX, logoY, 0.2f, logoR, defaultComboColors[0]);
            drawRing(logoX, logoY, logoR, 3.5f, clrWhite, 32);
            drawCenteredText(textBuf, logoX, logoY - 8.0f, 0.95f, clrWhite, "osu!");
            drawCenteredText(textBuf, logoX, logoY + 20.0f, 0.50f, clrCursor, "3DS Edition");

            // Botones laterales en pantalla superior
            const char* menuLabels[3] = { "JUGAR", "CONFIGURACION", "SALIR" };
            for (int i = 0; i < 3; ++i) {
                bool sel = (mainMenuOption == i);
                float bx = sel ? 202.0f : 212.0f;
                float by = 52.0f + i * 48.0f;
                float bw = 175.0f;
                float bh = 36.0f;

                u32 bg = sel ? defaultComboColors[i % 3] : clrPanelBg;
                C2D_DrawRectSolid(bx, by, 0.2f, bw, bh, bg);
                if (sel) drawRectOutline(bx, by, bw, bh, 2.0f, clrWhite);
                drawCenteredText(textBuf, bx + bw * 0.5f, by + bh * 0.5f, 0.52f, clrWhite, menuLabels[i]);
            }

            drawText(textBuf, 12.0f, 218.0f, 0.40f, clrMuted, "Arr/Abj + A o Toca la pantalla inferior");

            // --- PANTALLA INFERIOR: BOTONES TÁCTILES Y RESUMEN ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            for (int i = 0; i < 3; ++i) {
                bool sel = (mainMenuOption == i);
                float bx = 40.0f;
                float by = 30.0f + i * 56.0f;
                float bw = 240.0f;
                float bh = 44.0f;

                u32 bg = sel ? defaultComboColors[i % 3] : clrPanelBg;
                C2D_DrawRectSolid(bx, by, 0.2f, bw, bh, bg);
                drawRectOutline(bx, by, bw, bh, sel ? 2.0f : 1.0f, sel ? clrWhite : clrMuted);
                drawCenteredText(textBuf, bx + bw * 0.5f, by + bh * 0.5f, 0.55f, clrWhite, menuLabels[i]);
            }

            // Resumen rápido abajo
            C2D_DrawRectSolid(12.0f, 196.0f, 0.2f, 296.0f, 34.0f, clrPanelBg);
            char summary1[96];
            std::snprintf(summary1, sizeof(summary1), "Area Tablet: %d%% | Modo: %s | Offset: %+dms",
                static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                settings.tapWithStylus ? "Touch+Btn" : "Hibrido PC",
                settings.audioOffsetMs);
            drawCenteredText(textBuf, 160.0f, 213.0f, 0.38f, clr300, summary1);

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 2. MENÚ DE CONFIGURACIONES (Tablet Area, Audio, Calibración)
        // =========================================================
        if (appState == STATE_SETTINGS) {
            // Salir y guardar con B
            if (kDown & KEY_B) {
                calibrationMode = false;
                settings.save();
                audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                audio.playHitSound(0);
                appState = settingsReturnState;
                continue;
            }

            // Alternar asistente de calibración con X
            if (kDown & KEY_X) {
                calibrationMode = !calibrationMode;
                calibSamples.clear();
                calibStartTick = osGetTime();
                calibLastBeatIndex = -1;
                audio.playHitSound(2);
            }

            // Centrar área de tablet con Y
            if (kDown & KEY_Y) {
                settings.resetTabletArea();
                audio.playHitSound(0);
            }

            if (calibrationMode) {
                // Metrónomo a 120 BPM (500 ms por pulso)
                u64 elapsed = osGetTime() - calibStartTick;
                int beatIdx = static_cast<int>(elapsed / 500);
                if (beatIdx > calibLastBeatIndex) {
                    calibLastBeatIndex = beatIdx;
                    audio.playHitSound((beatIdx % 4 == 0) ? 2 : 0);
                }

                // Medir toque del usuario en cualquier botón de acción
                u32 tapKeys = KEY_A | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
                if (kDown & tapKeys) {
                    int mod = static_cast<int>(elapsed % 500);
                    int err = (mod > 250) ? (mod - 500) : mod; // -250 .. +250 ms
                    if (std::abs(err) <= 150) {
                        calibSamples.push_back(err);
                        if (calibSamples.size() > 12) calibSamples.erase(calibSamples.begin());
                        long sum = 0;
                        for (int s : calibSamples) sum += s;
                        settings.audioOffsetMs = static_cast<int>(sum / static_cast<long>(calibSamples.size()));
                        settings.clampValues();
                    }
                }
            } else {
                const int numSettings = 8;
                if (kDown & KEY_DUP) {
                    settingsOption = (settingsOption - 1 + numSettings) % numSettings;
                    audio.playHitSound(0);
                }
                if (kDown & KEY_DDOWN) {
                    settingsOption = (settingsOption + 1) % numSettings;
                    audio.playHitSound(0);
                }

                int stepDir = 0;
                if (kDown & (KEY_DLEFT | KEY_L))  stepDir = -1;
                if (kDown & (KEY_DRIGHT | KEY_R)) stepDir = +1;

                if (stepDir != 0) {
                    switch (settingsOption) {
                        case 0: // Tamaño Área Tablet
                            settings.tabletScale += stepDir * 0.05f;
                            break;
                        case 1: // Posición Horizontal X del Área Tablet
                            settings.tabletCenterX += stepDir * 8.0f;
                            break;
                        case 2: // Posición Vertical Y del Área Tablet
                            settings.tabletCenterY += stepDir * 8.0f;
                            break;
                        case 3: // Modo de Entrada (Stylus vs Híbrido PC)
                            settings.tapWithStylus = !settings.tapWithStylus;
                            break;
                        case 4: // Volumen Música
                            settings.musicVolume += stepDir * 5;
                            settings.clampValues();
                            audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                            break;
                        case 5: // Volumen Hitsounds
                            settings.sfxVolume += stepDir * 5;
                            settings.clampValues();
                            audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                            audio.playHitSound(2);
                            break;
                        case 6: // Offset de Audio (ms)
                            settings.audioOffsetMs += stepDir * ((kHeld & (KEY_L | KEY_R)) ? 5 : 1);
                            break;
                        case 7: // Tamaño del Cursor / No-Fail
                            if (kHeld & (KEY_L | KEY_R)) {
                                settings.noFail = !settings.noFail;
                            } else {
                                settings.cursorScale += stepDir * 0.1f;
                            }
                            break;
                    }
                    settings.clampValues();
                }

                if (kDown & KEY_A) {
                    if (settingsOption == 3) settings.tapWithStylus = !settings.tapWithStylus;
                    else if (settingsOption == 7) settings.noFail = !settings.noFail;
                    audio.playHitSound(0);
                }
            }

            // Permitir mover el centro del Área de Tablet con el Circle Pad o arrastrando con Stylus
            if (std::abs(cpad.dx) > 20 || std::abs(cpad.dy) > 20) {
                settings.tabletCenterX += (cpad.dx / 156.0f) * 3.0f;
                settings.tabletCenterY -= (cpad.dy / 156.0f) * 3.0f;
                settings.clampValues();
            }

            if (isTouching) {
                // Si está en las opciones de posición (1 o 2) o mantiene L, arrastrar con el stylus reposiciona la caja
                if (settingsOption == 1 || settingsOption == 2 || (kHeld & KEY_L)) {
                    settings.tabletCenterX = touch.px;
                    settings.tabletCenterY = touch.py;
                    settings.clampValues();
                }
                // Actualizar el cursor de prueba en la pantalla superior usando el mapeo del área de tablet
                settings.mapTouchToOsu(touch.px, touch.py, cursorOsuX, cursorOsuY);
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: LISTA DE CONFIGURACIONES + CURSOR DE PRUEBA ---
            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            // Marco tenue del playfield 320x240 en el fondo para probar alcance del cursor
            drawRectOutline(TOP_OFFSET_X, 0.0f, 320.0f, 240.0f, 1.0f, C2D_Color32(55, 50, 75, 160));

            C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 30.0f, clrPanelBg);
            drawText(textBuf, 12.0f, 6.0f, 0.54f, defaultComboColors[0], "Configuraciones (Tablet, Audio y Juego)");

            float rx, ry, rw, rh;
            settings.getTabletRect(rx, ry, rw, rh);

            char valStrings[8][64];
            std::snprintf(valStrings[0], 64, "%d%% (%dx%d px)",
                static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                static_cast<int>(std::round(rw)),
                static_cast<int>(std::round(rh)));
            std::snprintf(valStrings[1], 64, "X = %.0f px (Arrastra con Stylus)", settings.tabletCenterX);
            std::snprintf(valStrings[2], 64, "Y = %.0f px (Arrastra con Stylus)", settings.tabletCenterY);
            std::snprintf(valStrings[3], 64, "%s",
                settings.tapWithStylus ? "Touch + Botones (Tap en pantalla)" : "Hibrido PC (Stylus solo apunta)");
            std::snprintf(valStrings[4], 64, "%d%%", settings.musicVolume);
            std::snprintf(valStrings[5], 64, "%d%%", settings.sfxVolume);
            std::snprintf(valStrings[6], 64, "%+d ms (Pulsa X para Asistente)", settings.audioOffsetMs);
            std::snprintf(valStrings[7], 64, "Cursor %.1fx | No-Fail: %s (A)",
                settings.cursorScale, settings.noFail ? "SI" : "NO");

            const char* labels[8] = {
                "1. Tamano Area Tablet",
                "2. Posicion Tablet X",
                "3. Posicion Tablet Y",
                "4. Modo de Entrada",
                "5. Volumen Musica",
                "6. Volumen Hitsounds",
                "7. Offset Calibracion",
                "8. Cursor y No-Fail"
            };

            for (int i = 0; i < 8; ++i) {
                float yPos = 35.0f + i * 21.5f;
                bool sel = (!calibrationMode && settingsOption == i);
                if (sel) {
                    C2D_DrawRectSolid(10.0f, yPos - 1.0f, 0.15f, 380.0f, 20.0f, C2D_Color32(55, 45, 78, 220));
                }
                u32 lblClr = sel ? clrCursor : clrWhite;
                u32 valClr = sel ? clr300 : clrMuted;
                drawText(textBuf, 16.0f, yPos, 0.42f, lblClr, labels[i]);
                drawText(textBuf, 175.0f, yPos, 0.42f, valClr, valStrings[i]);
            }

            if (calibrationMode) {
                C2D_DrawRectSolid(30.0f, 65.0f, 0.7f, 340.0f, 110.0f, C2D_Color32(20, 18, 32, 245));
                drawRectOutline(30.0f, 65.0f, 340.0f, 110.0f, 2.0f, clr300);
                drawCenteredText(textBuf, 200.0f, 82.0f, 0.52f, clrCursor, "ASISTENTE DE CALIBRACION (120 BPM)");
                drawCenteredText(textBuf, 200.0f, 105.0f, 0.42f, clrWhite, "Pulsa A o L al ritmo del sonido de metrónomo");

                char calStr[96];
                std::snprintf(calStr, sizeof(calStr), "Muestras: %d/12 | Offset Calculado: %+d ms",
                    static_cast<int>(calibSamples.size()), settings.audioOffsetMs);
                drawCenteredText(textBuf, 200.0f, 132.0f, 0.46f, clr100, calStr);
                drawCenteredText(textBuf, 200.0f, 156.0f, 0.40f, clr300, "Pulsa X para terminar y aplicar");
            }

            C2D_DrawRectSolid(0.0f, 212.0f, 0.1f, 400.0f, 28.0f, clrPanelBg);
            drawText(textBuf, 10.0f, 218.0f, 0.39f, clr300,
                "Izq/Der: Ajustar | C-Pad: Mover Area | Y: Reset Area | B: Guardar y Salir");

            // Cursor de prueba en vivo en la pantalla superior
            float testR = 6.0f * settings.cursorScale;
            C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.9f, testR, clrCursor);
            C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.95f, 2.0f, clrWhite);

            // --- PANTALLA INFERIOR: EDITOR INTERACTIVO DE ÁREA DE TABLET ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            // Zona activa de la tablet
            C2D_DrawRectSolid(rx, ry, 0.15f, rw, rh, C2D_Color32(42, 36, 66, 230));

            // Líneas de rejilla dentro del área activa
            u32 gridClr = C2D_Color32(80, 72, 115, 160);
            C2D_DrawLine(rx + rw * 0.5f, ry, gridClr, rx + rw * 0.5f, ry + rh, gridClr, 1.0f, 0.2f);
            C2D_DrawLine(rx, ry + rh * 0.5f, gridClr, rx + rw, ry + rh * 0.5f, gridClr, 1.0f, 0.2f);

            // Borde resaltado del área de la tablet
            drawRectOutline(rx, ry, rw, rh, 2.0f, defaultComboColors[0]);
            drawRing(settings.tabletCenterX, settings.tabletCenterY, 6.0f, 1.5f, clrCursor, 16);

            char areaCaption[64];
            std::snprintf(areaCaption, sizeof(areaCaption), "AREA ACTIVA %d%% (%dx%d)",
                static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                static_cast<int>(std::round(rw)),
                static_cast<int>(std::round(rh)));
            drawCenteredText(textBuf, settings.tabletCenterX, std::max(12.0f, ry + 12.0f), 0.40f, clrWhite, areaCaption);

            drawCenteredText(textBuf, 160.0f, 228.0f, 0.37f, clrMuted,
                "Mueve el stylus dentro del recuadro para probar sensibilidad arriba");

            if (isTouching) {
                C2D_DrawCircleSolid(touch.px, touch.py, 0.8f, 4.5f, clr300);
            }

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 3. SELECTOR DE CANCIONES Y DIFICULTADES
        // =========================================================
        if (appState == STATE_SONG_SELECT) {
            if (kDown & KEY_B) {
                audio.playHitSound(0);
                appState = STATE_MAIN_MENU;
                continue;
            }

            if (kDown & KEY_X) {
                audio.playHitSound(0);
                settingsReturnState = STATE_SONG_SELECT;
                calibrationMode = false;
                appState = STATE_SETTINGS;
                continue;
            }

            if (kDown & KEY_Y) {
                audio.playHitSound(2);
                songMgr.initAndScan();
                selectedSet = 0;
                selectedDiff = 0;
            }

            if (!songMgr.sets.empty()) {
                int numSets = static_cast<int>(songMgr.sets.size());
                if (kDown & (KEY_DLEFT | KEY_L)) {
                    selectedSet = (selectedSet - 1 + numSets) % numSets;
                    selectedDiff = 0;
                    audio.playHitSound(0);
                }
                if (kDown & (KEY_DRIGHT | KEY_R)) {
                    selectedSet = (selectedSet + 1) % numSets;
                    selectedDiff = 0;
                    audio.playHitSound(0);
                }

                const auto& curSet = songMgr.sets[selectedSet];
                int numDiffs = static_cast<int>(curSet.difficulties.size());
                if (numDiffs > 0) {
                    if (kDown & KEY_DUP) {
                        selectedDiff = (selectedDiff - 1 + numDiffs) % numDiffs;
                        audio.playHitSound(0);
                    }
                    if (kDown & KEY_DDOWN) {
                        selectedDiff = (selectedDiff + 1) % numDiffs;
                        audio.playHitSound(0);
                    }

                    if (kDown & KEY_A) {
                        startSelectedBeatmap();
                        continue;
                    }
                }
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: LISTA DE CANCIONES Y DIFICULTADES ---
            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 34.0f, clrPanelBg);
            drawText(textBuf, 12.0f, 8.0f, 0.55f, defaultComboColors[0], "Selecciona un Beatmap");

            char headerInfo[96];
            std::snprintf(headerInfo, sizeof(headerInfo), "Cancion %d/%d",
                songMgr.sets.empty() ? 0 : (selectedSet + 1),
                static_cast<int>(songMgr.sets.size()));
            drawText(textBuf, 290.0f, 10.0f, 0.45f, clrMuted, headerInfo);

            if (songMgr.sets.empty()) {
                drawText(textBuf, 20.0f, 95.0f, 0.5f, clrMiss, "No se encontraron beatmaps (.osu / .osz)");
                drawText(textBuf, 20.0f, 120.0f, 0.43f, clrWhite, "Coloca carpetas o archivos .osz en:");
                drawText(textBuf, 20.0f, 140.0f, 0.45f, clr300, "sdmc:/3ds/osu-3ds/songs/");
            } else {
                const auto& curSet = songMgr.sets[selectedSet];

                C2D_DrawRectSolid(12.0f, 42.0f, 0.1f, 376.0f, 46.0f, clrPanelBg);
                std::string songHeader = std::string(curSet.fromRomfs ? "[ROM] " : "[SD] ") + truncateString(curSet.title, 38);
                drawText(textBuf, 20.0f, 47.0f, 0.52f, clrCursor, songHeader.c_str());

                std::string artistLine = "Artista: " + truncateString(curSet.artist, 42);
                drawText(textBuf, 20.0f, 67.0f, 0.42f, clrMuted, artistLine.c_str());

                int totalDiffs = static_cast<int>(curSet.difficulties.size());
                int maxVisible = 4;
                int startIdx = std::max(0, std::min(selectedDiff - 1, totalDiffs - maxVisible));
                int endIdx   = std::min(totalDiffs, startIdx + maxVisible);

                for (int i = startIdx; i < endIdx; ++i) {
                    int row = i - startIdx;
                    float yPos = 98.0f + row * 26.0f;
                    bool isSel = (i == selectedDiff);

                    if (isSel) {
                        C2D_DrawRectSolid(16.0f, yPos - 2.0f, 0.1f, 368.0f, 22.0f, C2D_Color32(55, 45, 78, 255));
                    }

                    u32 color = isSel ? clrCursor : clrWhite;
                    std::string verLabel = truncateString(curSet.difficulties[i].version, 32);
                    char lineStr[128];
                    std::snprintf(lineStr, sizeof(lineStr), "%s [%s] (%d obj)",
                        isSel ? "->" : "  ",
                        verLabel.c_str(),
                        curSet.difficulties[i].totalObjects());
                    drawText(textBuf, 22.0f, yPos, 0.48f, color, lineStr);
                }
            }

            C2D_DrawRectSolid(0.0f, 212.0f, 0.1f, 400.0f, 28.0f, clrPanelBg);
            drawText(textBuf, 8.0f, 218.0f, 0.39f, clr300,
                "A: Jugar | B: Menu | X: Config | Y: Recargar SD | Izq/Der: Cancion");

            // --- PANTALLA INFERIOR: DETALLES DEL BEATMAP ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            C2D_DrawRectSolid(10.0f, 10.0f, 0.1f, 300.0f, 220.0f, clrPanelBg);
            if (!songMgr.sets.empty() && !songMgr.sets[selectedSet].difficulties.empty()) {
                const auto& curSet = songMgr.sets[selectedSet];
                const auto& d = curSet.difficulties[selectedDiff];

                drawText(textBuf, 20.0f, 18.0f, 0.48f, defaultComboColors[2], "Informacion del Beatmap");

                std::string mapperStr = "Mapper: " + (d.creator.empty() ? std::string("Desconocido") : truncateString(d.creator, 28));
                drawText(textBuf, 20.0f, 40.0f, 0.42f, clrWhite, mapperStr.c_str());

                char objSummary[96];
                std::snprintf(objSummary, sizeof(objSummary), "Circulos: %d | Sliders: %d | Spinners: %d",
                    d.circleCount, d.sliderCount, d.spinnerCount);
                drawText(textBuf, 20.0f, 60.0f, 0.40f, clrMuted, objSummary);

                u32 barBg = C2D_Color32(18, 16, 26, 255);
                drawStatBar(textBuf, 20.0f, 86.0f,  "CS", d.cs, defaultComboColors[0], barBg, clrWhite);
                drawStatBar(textBuf, 20.0f, 106.0f, "AR", d.ar, defaultComboColors[1], barBg, clrWhite);
                drawStatBar(textBuf, 20.0f, 126.0f, "OD", d.od, defaultComboColors[2], barBg, clrWhite);
                drawStatBar(textBuf, 20.0f, 146.0f, "HP", d.hp, defaultComboColors[3], barBg, clrWhite);

                char cfgLine[96];
                std::snprintf(cfgLine, sizeof(cfgLine), "Area Tablet: %d%% | Modo: %s",
                    static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                    settings.tapWithStylus ? "Touch+Botones" : "Hibrido PC");
                drawText(textBuf, 20.0f, 176.0f, 0.40f, clrCursor, cfgLine);

                drawText(textBuf, 20.0f, 195.0f, 0.38f, clr300, "Carpeta SD: sdmc:/3ds/osu-3ds/songs/");
            }

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 4. PANTALLA DE RESULTADOS (Ranking Final)
        // =========================================================
        if (appState == STATE_RESULTS) {
            if (kDown & KEY_A) {
                startSelectedBeatmap();
                continue;
            }
            if (kDown & (KEY_B | KEY_START | KEY_SELECT)) {
                audio.playHitSound(0);
                appState = STATE_SONG_SELECT;
                continue;
            }

            int totalJudged = count300 + count100 + count50 + countMiss;
            float acc = (totalJudged > 0)
                ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                : 100.0f;

            const char* grade = "D";
            u32 gradeClr = clrMiss;
            if (stageFailed) {
                grade = "F";
                gradeClr = clrMiss;
            } else if (acc >= 99.9f && countMiss == 0) {
                grade = "SS";
                gradeClr = clrCursor;
            } else if (acc >= 93.0f && countMiss == 0) {
                grade = "S";
                gradeClr = clrCursor;
            } else if (acc >= 85.0f) {
                grade = "A";
                gradeClr = clr100;
            } else if (acc >= 75.0f) {
                grade = "B";
                gradeClr = clr300;
            } else if (acc >= 65.0f) {
                grade = "C";
                gradeClr = clr50;
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 34.0f, clrPanelBg);
            drawText(textBuf, 12.0f, 8.0f, 0.52f, clrWhite,
                stageFailed ? "PARTIDA FALLIDA" : "RESULTADOS DEL BEATMAP");

            // Círculo gigante de calificación a la izquierda
            C2D_DrawCircleSolid(95.0f, 125.0f, 0.2f, 52.0f, clrPanelBg);
            drawRing(95.0f, 125.0f, 52.0f, 3.0f, gradeClr, 28);
            drawCenteredText(textBuf, 95.0f, 122.0f, 1.3f, gradeClr, grade);

            // Desglose numérico a la derecha
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Puntuacion: %07d", score);
            drawText(textBuf, 175.0f, 50.0f, 0.52f, clrWhite, buf);

            std::snprintf(buf, sizeof(buf), "Precision:  %.2f%%", acc);
            drawText(textBuf, 175.0f, 75.0f, 0.48f, clr300, buf);

            std::snprintf(buf, sizeof(buf), "Max Combo:  %dx", maxCombo);
            drawText(textBuf, 175.0f, 98.0f, 0.48f, clrCursor, buf);

            std::snprintf(buf, sizeof(buf), "300: %-4d   100: %-4d", count300, count100);
            drawText(textBuf, 175.0f, 130.0f, 0.46f, clr100, buf);

            std::snprintf(buf, sizeof(buf), "50:  %-4d   Miss: %-4d", count50, countMiss);
            drawText(textBuf, 175.0f, 154.0f, 0.46f, clrMiss, buf);

            C2D_DrawRectSolid(0.0f, 212.0f, 0.1f, 400.0f, 28.0f, clrPanelBg);
            drawCenteredText(textBuf, 200.0f, 226.0f, 0.44f, clr300, "A: Reintentar | B: Volver a Canciones");

            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);
            drawCenteredText(textBuf, 160.0f, 120.0f, 0.50f, clrMuted, truncateString(beatmap.title, 34).c_str());
            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 5. JUEGO ACTIVO + MENÚ DE PAUSA
        // =========================================================
        if (kDown & (KEY_START | KEY_SELECT)) {
            if (!isPaused) {
                isPaused = true;
                pauseOption = 0;
                audio.pauseMusic();
            } else {
                isPaused = false;
                audio.resumeMusic();
            }
        }

        if (isPaused) {
            if (kDown & KEY_DUP) {
                pauseOption = (pauseOption + 2) % 3;
                audio.playHitSound(0);
            }
            if (kDown & KEY_DDOWN) {
                pauseOption = (pauseOption + 1) % 3;
                audio.playHitSound(0);
            }
            if (kDown & KEY_A) {
                audio.playHitSound(2);
                if (pauseOption == 0) {
                    isPaused = false;
                    audio.resumeMusic();
                } else if (pauseOption == 1) {
                    startSelectedBeatmap();
                    continue;
                } else if (pauseOption == 2) {
                    audio.stopMusic();
                    appState = STATE_SONG_SELECT;
                    continue;
                }
            }
        } else {
            audio.update();
        }

        int songTime = audio.getTimeMs() - settings.audioOffsetMs;

        if (!isPaused) {
            if (isTouching) {
                // Mapear el toque de la pantalla inferior según el Área de Tablet configurada
                settings.mapTouchToOsu(touch.px, touch.py, cursorOsuX, cursorOsuY);
            }

            u32 btnGroup1 = KEY_L | KEY_DLEFT | KEY_A | KEY_X;
            u32 btnGroup2 = KEY_R | KEY_ZL | KEY_ZR | KEY_DDOWN | KEY_DUP | KEY_DRIGHT | KEY_B | KEY_Y;
            if (settings.tapWithStylus) {
                btnGroup1 |= KEY_TOUCH;
            }

            bool k1Held = (kHeld & btnGroup1) != 0;
            bool k2Held = (kHeld & btnGroup2) != 0;
            bool anyHeld = (k1Held || k2Held);
            bool kActionDown = (kDown & (btnGroup1 | btnGroup2)) != 0;

            float hitRadius = beatmap.diff.radiusOsu;
            float hit50     = beatmap.diff.hit50;
            float hit100    = beatmap.diff.hit100;
            float hit300    = beatmap.diff.hit300;

            // Drenaje pasivo suave de HP mientras hay notas activas
            if (!beatmap.circles.empty() && songTime >= beatmap.circles.front().time &&
                songTime <= beatmap.circles.back().endTime) {
                hp -= (0.00018f * beatmap.diff.hp);
                if (hp <= 0.0f) {
                    if (settings.noFail) {
                        hp = 0.05f;
                    } else {
                        hp = 0.0f;
                        stageFailed = true;
                        audio.stopMusic();
                        appState = STATE_RESULTS;
                        continue;
                    }
                }
            }

            bool allObjectsDone = true;

            for (auto& obj : beatmap.circles) {
                bool thisObjDone = obj.isSlider ? obj.sliderDone :
                                  (obj.isSpinner ? obj.sliderDone : (obj.state != NOT_HIT));
                if (!thisObjDone) allObjectsDone = false;

                // 1. Evaluar Spinners activos
                if (obj.isSpinner && !obj.sliderDone && songTime >= obj.time) {
                    if ((anyHeld || !settings.tapWithStylus) && isTouching) {
                        float ang = std::atan2(cursorOsuY - 192.0f, cursorOsuX - 256.0f);
                        if (obj.spinnerAngleInit) {
                            float dAng = ang - obj.lastSpinnerAngle;
                            while (dAng >  static_cast<float>(M_PI)) dAng -= 2.0f * static_cast<float>(M_PI);
                            while (dAng < -static_cast<float>(M_PI)) dAng += 2.0f * static_cast<float>(M_PI);
                            obj.spinnerProgress += std::fabs(dAng);
                        }
                        obj.lastSpinnerAngle = ang;
                        obj.spinnerAngleInit = true;
                    } else {
                        obj.spinnerAngleInit = false;
                    }

                    if (songTime >= obj.endTime) {
                        obj.sliderDone = true;
                        if (obj.spinnerProgress >= obj.spinnerRequiredRads * 0.65f) {
                            obj.state = HIT_300;
                            audio.playHitSound(obj.hitSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += 300 + static_cast<int>(300 * combo * 0.05f);
                            count300++;
                            hp = std::min(1.0f, hp + 0.08f);
                        } else if (obj.spinnerProgress >= obj.spinnerRequiredRads * 0.35f) {
                            obj.state = HIT_100;
                            audio.playHitSound(obj.hitSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += 100;
                            count100++;
                            hp = std::min(1.0f, hp + 0.04f);
                        } else {
                            obj.state = HIT_MISS;
                            combo = 0;
                            countMiss++;
                            hp = std::max(0.0f, hp - 0.12f);
                        }
                        popups.push_back(HitPopup(256.0f, 192.0f, obj.state, 20));
                    }
                    continue;
                }

                // 2. Evaluar seguimiento, rebotes y finalización de Sliders activos
                if (obj.isSlider && !obj.sliderDone && songTime >= obj.time) {
                    Vec2 ballPos = obj.getSliderBallPos(songTime);
                    float dx = cursorOsuX - ballPos.x;
                    float dy = cursorOsuY - ballPos.y;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    obj.sliderTracking = (anyHeld && dist <= hitRadius * 2.4f);

                    if (obj.repeats > 1 && obj.endTime > obj.time) {
                        float slideDur = static_cast<float>(obj.endTime - obj.time) / obj.repeats;
                        int curRep = static_cast<int>((songTime - obj.time) / slideDur);
                        if (curRep > obj.lastRepeatProcessed && curRep < obj.repeats) {
                            obj.lastRepeatProcessed = curRep;
                            if (obj.sliderTracking) {
                                audio.playHitSound(obj.getEdgeHitSound(curRep));
                                combo++;
                                if (combo > maxCombo) maxCombo = combo;
                                score += 30;
                                hp = std::min(1.0f, hp + 0.02f);
                            }
                        }
                    }

                    if (songTime >= obj.endTime) {
                        obj.sliderDone = true;
                        if (obj.state != HIT_MISS && obj.sliderTracking) {
                            audio.playHitSound(obj.getEdgeHitSound(obj.repeats));
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += 300 + static_cast<int>(300 * combo * 0.05f);
                            count300++;
                            hp = std::min(1.0f, hp + 0.05f);
                            popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_300, 18));
                        } else if (obj.state == NOT_HIT) {
                            obj.state = HIT_MISS;
                            combo = 0;
                            countMiss++;
                            hp = std::max(0.0f, hp - 0.10f);
                            popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_MISS, 22));
                        }
                    }
                }

                if (obj.state != NOT_HIT || obj.isSpinner) continue;

                // 3. Evaluar Miss de la cabeza de la nota
                if (songTime > (obj.time + hit50)) {
                    obj.state = HIT_MISS;
                    combo = 0;
                    countMiss++;
                    hp = std::max(0.0f, hp - 0.12f);
                    popups.push_back(HitPopup(obj.x, obj.y, HIT_MISS, 22));
                    continue;
                }

                // 4. Evaluar golpe (Tap) en la nota
                if (kActionDown) {
                    int timeDelta = std::abs(songTime - obj.time);
                    if (timeDelta <= hit50) {
                        float dx = cursorOsuX - obj.x;
                        float dy = cursorOsuY - obj.y;
                        float dist = std::sqrt(dx * dx + dy * dy);

                        if (dist <= hitRadius * 1.2f) {
                            if (timeDelta <= hit300) {
                                obj.state = HIT_300;
                                score += 300 + static_cast<int>(300 * combo * 0.05f);
                                count300++;
                                hp = std::min(1.0f, hp + 0.06f);
                            } else if (timeDelta <= hit100) {
                                obj.state = HIT_100;
                                score += 100 + static_cast<int>(100 * combo * 0.05f);
                                count100++;
                                hp = std::min(1.0f, hp + 0.025f);
                            } else {
                                obj.state = HIT_50;
                                score += 50 + static_cast<int>(50 * combo * 0.05f);
                                count50++;
                            }
                            int headSound = obj.isSlider ? obj.getEdgeHitSound(0) : obj.hitSound;
                            audio.playHitSound(headSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            if (obj.isSlider) obj.sliderTracking = true;
                            popups.push_back(HitPopup(obj.x, obj.y, obj.state, 18));
                            kActionDown = false;
                            break;
                        }
                    }
                }
            }

            // Si ya terminaron todos los objetos o terminó la canción, mostrar Resultados
            if ((allObjectsDone && !beatmap.circles.empty() &&
                 songTime > beatmap.circles.back().endTime + 1200) ||
                audio.isSongFinished()) {
                audio.stopMusic();
                appState = STATE_RESULTS;
                continue;
            }
        }

        // =========================================================
        // RENDERIZADO DE LA PARTIDA
        // =========================================================
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // --- PANTALLA SUPERIOR ---
        C2D_TargetClear(topScreen, clrBgDark);
        C2D_SceneBegin(topScreen);

        C2D_DrawRectSolid(TOP_OFFSET_X, 0.0f, 0.0f, 320.0f, 240.0f, clrPlayfield);

        float hitRadius = beatmap.diff.radiusOsu;
        float preempt   = beatmap.diff.preempt;
        float hit50     = beatmap.diff.hit50;
        float rScreen   = hitRadius * SCALE_FACTOR;

        for (int i = static_cast<int>(beatmap.circles.size()) - 1; i >= 0; --i) {
            const auto& obj = beatmap.circles[i];

            bool visible = false;
            if (obj.isSpinner) {
                visible = (songTime >= (obj.time - 400) && !obj.sliderDone);
            } else if (obj.isSlider) {
                visible = (songTime >= (obj.time - preempt) && !obj.sliderDone);
            } else {
                visible = (songTime >= (obj.time - preempt) && obj.state == NOT_HIT);
            }
            if (!visible) continue;

            u32 cColor = defaultComboColors[obj.colorIndex % 4];
            u32 sColor = defaultSliderColors[obj.colorIndex % 4];
            if (!beatmap.customComboColors.empty()) {
                const auto& rgb = beatmap.customComboColors[obj.colorIndex % beatmap.customComboColors.size()];
                cColor = C2D_Color32(rgb.r, rgb.g, rgb.b, 235);
                sColor = C2D_Color32(
                    static_cast<u8>(rgb.r * 0.65f),
                    static_cast<u8>(rgb.g * 0.65f),
                    static_cast<u8>(rgb.b * 0.65f),
                    155
                );
            }

            if (obj.isSpinner) {
                float spX = osuToTopX(256.0f);
                float spY = osuToTopY(192.0f);
                float ratio = std::min(1.0f, obj.spinnerProgress / obj.spinnerRequiredRads);
                drawRing(spX, spY, 75.0f * (1.0f - ratio * 0.7f), 2.5f, clr300, 28);
                C2D_DrawCircleSolid(spX, spY, 0.25f, 6.0f, clrWhite);
                continue;
            }

            if (obj.isSlider && obj.sliderPath.size() >= 2) {
                for (size_t p = 0; p + 1 < obj.sliderPath.size(); ++p) {
                    float x0 = osuToTopX(obj.sliderPath[p].x);
                    float y0 = osuToTopY(obj.sliderPath[p].y);
                    float x1 = osuToTopX(obj.sliderPath[p + 1].x);
                    float y1 = osuToTopY(obj.sliderPath[p + 1].y);
                    C2D_DrawLine(x0, y0, sColor, x1, y1, sColor, rScreen * 1.7f, 0.1f);
                    C2D_DrawCircleSolid(x1, y1, 0.1f, rScreen * 0.85f, sColor);
                }

                float tailX = osuToTopX(obj.sliderPath.back().x);
                float tailY = osuToTopY(obj.sliderPath.back().y);
                drawRing(tailX, tailY, rScreen * 0.85f, 1.5f, clrWhite, 16);

                if (songTime >= obj.time && songTime <= obj.endTime) {
                    Vec2 ballOsu = obj.getSliderBallPos(songTime);
                    float bx = osuToTopX(ballOsu.x);
                    float by = osuToTopY(ballOsu.y);
                    C2D_DrawCircleSolid(bx, by, 0.35f, rScreen * 0.9f, cColor);
                    float followR = obj.sliderTracking ? (rScreen * 1.9f) : (rScreen * 1.3f);
                    u32 followClr = obj.sliderTracking ? clrCursor : clrWhite;
                    drawRing(bx, by, followR, 1.5f, followClr, 20);
                }
            }

            if (obj.state == NOT_HIT && songTime <= obj.time + hit50) {
                float cx = osuToTopX(obj.x);
                float cy = osuToTopY(obj.y);

                C2D_DrawCircleSolid(cx, cy, 0.2f, rScreen, cColor);
                drawRing(cx, cy, rScreen, 2.0f, clrWhite, 20);

                char numStr[16];
                std::snprintf(numStr, sizeof(numStr), "%d", obj.comboNumber);
                drawCenteredText(textBuf, cx, cy, 0.55f, clrWhite, numStr);

                float timeLeft = static_cast<float>(obj.time - songTime);
                if (timeLeft > 0.0f) {
                    float progress = timeLeft / preempt;
                    float approachRadius = rScreen * (1.0f + 2.5f * progress);
                    drawRing(cx, cy, approachRadius, 1.5f, cColor, 20);
                }
            }
        }

        // Calificaciones flotantes
        for (size_t i = 0; i < popups.size(); ) {
            float px = osuToTopX(popups[i].x);
            float py = osuToTopY(popups[i].y) - (18 - popups[i].timer) * 0.6f;

            const char* txt = "300";
            u32 clr = clr300;
            if (popups[i].state == HIT_100)      { txt = "100"; clr = clr100; }
            else if (popups[i].state == HIT_50)  { txt = "50";  clr = clr50; }
            else if (popups[i].state == HIT_MISS){ txt = "X";   clr = clrMiss; }

            drawCenteredText(textBuf, px, py, 0.5f, clr, txt);

            if (!isPaused) popups[i].timer--;
            if (popups[i].timer <= 0) {
                popups.erase(popups.begin() + i);
            } else {
                ++i;
            }
        }

        // Barra de Vida (HP Bar) arriba a la izquierda
        C2D_DrawRectSolid(6.0f, 8.0f, 0.3f, 140.0f, 7.0f, clrKeyIdle);
        u32 hpClr = (hp > 0.5f) ? clr100 : ((hp > 0.25f) ? clr50 : clrMiss);
        C2D_DrawRectSolid(6.0f, 8.0f, 0.35f, 140.0f * hp, 7.0f, hpClr);

        // HUD: Score, Accuracy y Combo
        char hudStr[64];
        std::snprintf(hudStr, sizeof(hudStr), "%07d", score);
        drawText(textBuf, 325.0f, 5.0f, 0.45f, clrWhite, hudStr);

        int totalJudged = count300 + count100 + count50 + countMiss;
        float acc = (totalJudged > 0)
            ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
            : 100.0f;
        std::snprintf(hudStr, sizeof(hudStr), "%.1f%%", acc);
        drawText(textBuf, 340.0f, 22.0f, 0.40f, clr300, hudStr);

        std::snprintf(hudStr, sizeof(hudStr), "%dx", combo);
        drawText(textBuf, 5.0f, 215.0f, 0.6f, clrCursor, hudStr);

        // Key Overlay
        u32 btnGroup1 = KEY_L | KEY_DLEFT | KEY_A | KEY_X | (settings.tapWithStylus ? KEY_TOUCH : 0);
        u32 btnGroup2 = KEY_R | KEY_ZL | KEY_ZR | KEY_DDOWN | KEY_DUP | KEY_DRIGHT | KEY_B | KEY_Y;
        bool k1Held = (kHeld & btnGroup1) != 0;
        bool k2Held = (kHeld & btnGroup2) != 0;
        bool anyHeld = (k1Held || k2Held);

        C2D_DrawRectSolid(368.0f, 95.0f, 0.3f, 24.0f, 22.0f, k1Held ? clrKeyActive : clrKeyIdle);
        C2D_DrawRectSolid(368.0f, 123.0f, 0.3f, 24.0f, 22.0f, k2Held ? clrKeyActive : clrKeyIdle);

        // Cursor escalado según configuración
        float cursorRadius = (anyHeld ? 7.5f : 5.5f) * settings.cursorScale;
        C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.9f, cursorRadius, clrCursor);
        C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.95f, 2.0f * settings.cursorScale, clrWhite);

        // Overlay de Pausa en pantalla superior
        if (isPaused) {
            C2D_DrawRectSolid(80.0f, 45.0f, 0.96f, 240.0f, 150.0f, C2D_Color32(18, 16, 28, 240));
            drawRectOutline(80.0f, 45.0f, 240.0f, 150.0f, 2.0f, defaultComboColors[0]);
            drawCenteredText(textBuf, 200.0f, 65.0f, 0.60f, clrCursor, "JUEGO EN PAUSA");

            const char* pOpts[3] = { "Continuar", "Reintentar", "Volver a Canciones" };
            for (int i = 0; i < 3; ++i) {
                u32 c = (pauseOption == i) ? clr300 : clrWhite;
                drawCenteredText(textBuf, 200.0f, 102.0f + i * 26.0f, 0.48f, c, pOpts[i]);
            }
        }

        // --- PANTALLA INFERIOR: SUPERFICIE DE TABLET ACTIVA ---
        C2D_TargetClear(bottomScreen, clrTabletBg);
        C2D_SceneBegin(bottomScreen);

        float rx, ry, rw, rh;
        settings.getTabletRect(rx, ry, rw, rh);

        // Dibujar el recuadro del área activa configurada en la pantalla inferior
        C2D_DrawRectSolid(rx, ry, 0.1f, rw, rh, C2D_Color32(28, 25, 40, 255));
        drawRectOutline(rx, ry, rw, rh, 1.5f, C2D_Color32(85, 75, 120, 200));
        drawRing(settings.tabletCenterX, settings.tabletCenterY, 12.0f, 1.0f, C2D_Color32(55, 50, 75, 255), 16);

        if (isTouching) {
            C2D_DrawCircleSolid(touch.px, touch.py, 0.5f, 4.0f, clrKeyActive);
        }

        C3D_FrameEnd(0);
    }

    settings.save();
    audio.exitAll();
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
    return 0;
}