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

constexpr float SCALE_FACTOR = 0.625f;
constexpr float TOP_OFFSET_X = 40.0f;

inline float osuToTopX(float osuX) { return TOP_OFFSET_X + (osuX * SCALE_FACTOR); }
inline float osuToTopY(float osuY) { return osuY * SCALE_FACTOR; }

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
    C2D_TextBuf textBuf = C2D_TextBufNew(2048);

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

    SongManager songMgr;
    songMgr.initAndScan();

    int selectedSet = 0;
    int selectedDiff = 0;
    bool inMenu = true;

    Beatmap beatmap;
    AudioPlayer audio;
    audio.initDSPAndHitsounds();

    float cursorOsuX = 256.0f;
    float cursorOsuY = 192.0f;
    int combo = 0;
    int maxCombo = 0;
    int score = 0;
    int count300 = 0, count100 = 0, count50 = 0, countMiss = 0;
    std::vector<HitPopup> popups;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        C2D_TextBufClear(textBuf);

        // =========================================================
        // MENÚ SELECTOR ESCALABLE DE CANCIONES Y DIFICULTADES
        // =========================================================
        if (inMenu) {
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
                        const auto& diffEntry = curSet.difficulties[selectedDiff];
                        if (beatmap.loadFromFile(diffEntry.osuPath, false)) {
                            audio.playHitSound(2);

                            // Calcular Lead-In para dar tiempo de reacción si la 1ra nota está al inicio
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
                            popups.clear();
                            inMenu = false;
                        }
                    }
                }
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: LISTA DE CANCIONES Y DIFICULTADES ---
            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 34.0f, clrPanelBg);
            drawText(textBuf, 12.0f, 8.0f, 0.58f, defaultComboColors[0], "osu! 3DS");

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

                // Tarjeta de la canción activa
                C2D_DrawRectSolid(12.0f, 42.0f, 0.1f, 376.0f, 46.0f, clrPanelBg);
                std::string songHeader = std::string(curSet.fromRomfs ? "[ROM] " : "[SD] ") + truncateString(curSet.title, 38);
                drawText(textBuf, 20.0f, 47.0f, 0.52f, clrCursor, songHeader.c_str());

                std::string artistLine = "Artista: " + truncateString(curSet.artist, 42);
                drawText(textBuf, 20.0f, 67.0f, 0.42f, clrMuted, artistLine.c_str());

                // Lista paginada de dificultades (máx 4 visibles a la vez)
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
            drawText(textBuf, 10.0f, 218.0f, 0.40f, clr300,
                "Izq/Der: Cancion | Arr/Abj: Dificultad | A: Jugar | Y: Recargar SD");

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

                drawText(textBuf, 20.0f, 176.0f, 0.38f, clrMuted, "Carpeta SD (soporta carpetas y .osz):");
                drawText(textBuf, 20.0f, 192.0f, 0.40f, clr300, "sdmc:/3ds/osu-3ds/songs/");

                if (songMgr.extractedOszCount > 0) {
                    char oszMsg[64];
                    std::snprintf(oszMsg, sizeof(oszMsg), "Extraidos %d paquete(s) .osz!", songMgr.extractedOszCount);
                    drawText(textBuf, 20.0f, 208.0f, 0.40f, clr100, oszMsg);
                }
            }

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // JUEGO ACTIVO
        // =========================================================
        audio.update();
        int songTime = audio.getTimeMs();

        if (kDown & KEY_SELECT) {
            audio.stopMusic();
            inMenu = true;
            continue;
        }

        touchPosition touch;
        bool isTouching = (kHeld & KEY_TOUCH) != 0;
        if (isTouching) {
            hidTouchRead(&touch);
            cursorOsuX = touch.px / SCALE_FACTOR;
            cursorOsuY = touch.py / SCALE_FACTOR;
        }

        bool k1Held = (kHeld & (KEY_L | KEY_DLEFT | KEY_A | KEY_TOUCH)) != 0;
        bool k2Held = (kHeld & (KEY_ZL | KEY_DDOWN | KEY_DUP | KEY_B | KEY_R | KEY_X | KEY_Y)) != 0;
        bool anyHeld = (k1Held || k2Held);
        bool kActionDown = (kDown & (KEY_L | KEY_DLEFT | KEY_A | KEY_ZL | KEY_DDOWN | KEY_DUP | KEY_B | KEY_R | KEY_X | KEY_Y | KEY_TOUCH)) != 0;

        float hitRadius = beatmap.diff.radiusOsu;
        float preempt   = beatmap.diff.preempt;
        float hit50     = beatmap.diff.hit50;
        float hit100    = beatmap.diff.hit100;
        float hit300    = beatmap.diff.hit300;

        for (auto& obj : beatmap.circles) {
            // 1. Evaluar Spinners activos
            if (obj.isSpinner && !obj.sliderDone && songTime >= obj.time) {
                if (anyHeld && isTouching) {
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
                    } else if (obj.spinnerProgress >= obj.spinnerRequiredRads * 0.35f) {
                        obj.state = HIT_100;
                        audio.playHitSound(obj.hitSound);
                        combo++;
                        if (combo > maxCombo) maxCombo = combo;
                        score += 100;
                        count100++;
                    } else {
                        obj.state = HIT_MISS;
                        combo = 0;
                        countMiss++;
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

                // Hitsounds en rebotes (repeats intermedios)
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
                        popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_300, 18));
                    } else if (obj.state == NOT_HIT) {
                        obj.state = HIT_MISS;
                        combo = 0;
                        countMiss++;
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
                        } else if (timeDelta <= hit100) {
                            obj.state = HIT_100;
                            score += 100 + static_cast<int>(100 * combo * 0.05f);
                            count100++;
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

        // =========================================================
        // RENDERIZADO
        // =========================================================
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // --- PANTALLA SUPERIOR ---
        C2D_TargetClear(topScreen, clrBgDark);
        C2D_SceneBegin(topScreen);

        C2D_DrawRectSolid(TOP_OFFSET_X, 0.0f, 0.0f, 320.0f, 240.0f, clrPlayfield);

        float rScreen = hitRadius * SCALE_FACTOR;

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

            // Seleccionar color del mapa o color por defecto
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

            // 0. Renderizar Spinner
            if (obj.isSpinner) {
                float spX = osuToTopX(256.0f);
                float spY = osuToTopY(192.0f);
                float ratio = std::min(1.0f, obj.spinnerProgress / obj.spinnerRequiredRads);
                drawRing(spX, spY, 75.0f * (1.0f - ratio * 0.7f), 2.5f, clr300, 28);
                C2D_DrawCircleSolid(spX, spY, 0.25f, 6.0f, clrWhite);
                continue;
            }

            // 1. Cuerpo del Slider y Bola
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

            // 2. Cabeza del círculo y número
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

        // 3. Dibujar calificaciones flotantes (300 / 100 / 50 / X)
        for (size_t i = 0; i < popups.size(); ) {
            float px = osuToTopX(popups[i].x);
            float py = osuToTopY(popups[i].y) - (18 - popups[i].timer) * 0.6f;

            const char* txt = "300";
            u32 clr = clr300;
            if (popups[i].state == HIT_100)      { txt = "100"; clr = clr100; }
            else if (popups[i].state == HIT_50)  { txt = "50";  clr = clr50; }
            else if (popups[i].state == HIT_MISS){ txt = "X";   clr = clrMiss; }

            drawCenteredText(textBuf, px, py, 0.5f, clr, txt);

            popups[i].timer--;
            if (popups[i].timer <= 0) {
                popups.erase(popups.begin() + i);
            } else {
                ++i;
            }
        }

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
        C2D_DrawRectSolid(368.0f, 95.0f, 0.3f, 24.0f, 22.0f, k1Held ? clrKeyActive : clrKeyIdle);
        C2D_DrawRectSolid(368.0f, 123.0f, 0.3f, 24.0f, 22.0f, k2Held ? clrKeyActive : clrKeyIdle);

        // Cursor
        float cursorRadius = anyHeld ? 7.5f : 5.5f;
        C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.9f, cursorRadius, clrCursor);
        C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.95f, 2.0f, clrWhite);

        // --- PANTALLA INFERIOR ---
        C2D_TargetClear(bottomScreen, clrTabletBg);
        C2D_SceneBegin(bottomScreen);

        drawRing(160.0f, 120.0f, 15.0f, 1.0f, C2D_Color32(50, 48, 65, 255));
        if (isTouching) {
            C2D_DrawCircleSolid(touch.px, touch.py, 0.5f, 4.0f, clrKeyActive);
        }

        C3D_FrameEnd(0);
    }

    audio.exitAll();
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
    return 0;
}