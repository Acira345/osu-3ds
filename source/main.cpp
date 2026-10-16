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
#include "SkinManager.hpp"
#include "ScoreManager.hpp"
#include "WifiServer.hpp"
#include "TextureLoader.hpp"
#include "UiKit.hpp"

// Aumentar pila del hilo principal en la 3DS a 256 KB (por defecto libctru asigna solo 32 KB)
extern "C" {
    u32 __stacksize__ = 0x40000;
}

constexpr float SCALE_FACTOR = 0.625f;
constexpr float TOP_OFFSET_X = 40.0f;

inline float osuToTopX(float osuX) { return TOP_OFFSET_X + (osuX * SCALE_FACTOR); }
inline float osuToTopY(float osuY) { return osuY * SCALE_FACTOR; }

inline u32 getStarRatingColor(float stars) {
    if (stars < 2.0f) return C2D_Color32(100, 240, 120, 255); // Easy (Verde)
    if (stars < 2.7f) return C2D_Color32(70, 220, 255, 255);  // Normal (Cian)
    if (stars < 4.0f) return C2D_Color32(255, 224, 85, 255);  // Hard (Oro)
    if (stars < 5.3f) return C2D_Color32(255, 100, 170, 255); // Insane (Rosa)
    return C2D_Color32(205, 110, 255, 255);                   // Expert+ (Violeta)
}

enum AppState {
    STATE_MAIN_MENU,
    STATE_WIFI_TRANSFER,
    STATE_SETTINGS,
    STATE_SONG_SELECT,
    STATE_PLAYING,
    STATE_RESULTS
};

struct HitPopup {
    float x;
    float y;
    HitState state;
    float burstRadius;
    u32 burstColor;
    int timer;
    HitPopup(float _x = 0.0f, float _y = 0.0f, HitState _s = HIT_300, float _r = 16.0f, u32 _c = 0, int _t = 0)
        : x(_x), y(_y), state(_s), burstRadius(_r), burstColor(_c), timer(_t) {}
};

struct TrailPoint {
    float x, y;
    int life;
};

void drawRingZ(float x, float y, float z, float radius, float thickness, u32 color, int segments = 24) {
    const float step = (2.0f * static_cast<float>(M_PI)) / segments;
    for (int i = 0; i < segments; ++i) {
        float a0 = i * step;
        float a1 = (i + 1) * step;
        C2D_DrawLine(
            x + std::cos(a0) * radius, y + std::sin(a0) * radius, color,
            x + std::cos(a1) * radius, y + std::sin(a1) * radius, color,
            thickness, z
        );
    }
}

void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24) {
    drawRingZ(x, y, 0.5f, radius, thickness, color, segments);
}

void drawRectOutline(float x, float y, float w, float h, float thickness, u32 color, float z = 0.5f) {
    C2D_DrawLine(x, y, color, x + w, y, color, thickness, z);
    C2D_DrawLine(x + w, y, color, x + w, y + h, color, thickness, z);
    C2D_DrawLine(x + w, y + h, color, x, y + h, color, thickness, z);
    C2D_DrawLine(x, y + h, color, x, y, color, thickness, z);
}

void drawText(C2D_TextBuf buf, float x, float y, float scale, u32 color, const char* str, float z = 0.85f) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, z, scale, scale, color);
}

void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &w, &h);
    C2D_DrawText(&text, C2D_WithColor, cx - (w * 0.5f), cy - (h * 0.5f), z, scale, scale, color);
}

// Dibuja una flecha de rebote (Reverse Arrow) en (cx, cy) apuntando hacia (targetX, targetY)
void drawReverseArrow(float cx, float cy, float targetX, float targetY, float size, u32 color, float z = 0.42f) {
    float angle = std::atan2(targetY - cy, targetX - cx);
    float tipX = cx + std::cos(angle) * (size * 0.45f);
    float tipY = cy + std::sin(angle) * (size * 0.45f);
    float wingAngle = 2.45f; // ~140 grados
    float w1X = tipX + std::cos(angle + wingAngle) * (size * 0.75f);
    float w1Y = tipY + std::sin(angle + wingAngle) * (size * 0.75f);
    float w2X = tipX + std::cos(angle - wingAngle) * (size * 0.75f);
    float w2Y = tipY + std::sin(angle - wingAngle) * (size * 0.75f);

    C2D_DrawLine(w1X, w1Y, color, tipX, tipY, color, 2.0f, z);
    C2D_DrawLine(w2X, w2Y, color, tipX, tipY, color, 2.0f, z);
}

std::string truncateString(const std::string& str, size_t maxLen) {
    if (str.size() <= maxLen) return str;
    return str.substr(0, maxLen - 3) + "...";
}

void drawStatBar(C2D_TextBuf buf, float x, float y, const char* label, float val, u32 fillClr, u32 bgClr, u32 txtClr) {
    char str[32];
    std::snprintf(str, sizeof(str), "%s: %.1f", label, val);
    drawText(buf, x, y, 0.40f, txtClr, str);

    float barX = x + 58.0f;
    float barY = y + 3.0f;
    float barW = 92.0f;
    float barH = 6.5f;
    float fillRatio = std::max(0.0f, std::min(1.0f, val / 10.0f));

    Ui::statBar(barX, barY, barW, barH, 0.2f, fillRatio, fillClr);
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    romfsInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topScreen    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf textBuf = C2D_TextBufNew(4096);

    u32 clrBgDark     = C2D_Color32(18, 16, 26, 255);
    u32 clrPanelBg    = C2D_Color32(30, 27, 44, 255);
    u32 clrPlayfield  = C2D_Color32(26, 24, 38, 255);
    u32 clrTabletBg   = C2D_Color32(22, 20, 30, 255);
    u32 clrWhite      = C2D_Color32(255, 255, 255, 255);
    u32 clrMuted      = C2D_Color32(170, 165, 195, 255);
    u32 clrKeyIdle    = C2D_Color32(45, 42, 60, 255);
    u32 clrKeyActive  = C2D_Color32(245, 110, 165, 255);
    u32 clr300        = C2D_Color32(80, 220, 255, 255);
    u32 clr100        = C2D_Color32(100, 240, 120, 255);
    u32 clr50         = C2D_Color32(230, 190, 80, 255);
    u32 clrMiss       = C2D_Color32(240, 70, 70, 255);

    GameSettings settings;
    settings.load();

    SkinManager skinMgr;
    skinMgr.initAndScan();
    if (settings.skinIndex >= static_cast<int>(skinMgr.skins.size())) {
        settings.skinIndex = 0;
    }

    SongManager songMgr;
    songMgr.initAndScan();

    ScoreManager scoreMgr;
    scoreMgr.load();

    Beatmap beatmap;
    AudioPlayer audio;
    audio.initDSPAndHitsounds();
    audio.setVolumes(settings.musicVolume, settings.sfxVolume);

    WifiServer wifiServer;

    // Texturas GPU dinámicas (Fondo de canción y Sprites PNG de Skin personalizado)
    LoadedTexture bgTex;
    LoadedTexture texSkinCursor;
    LoadedTexture texSkinCircle;
    LoadedTexture texSkinOverlay;
    LoadedTexture texSkinApproach;

    auto applyActiveSkinAudio = [&]() {
        if (skinMgr.skins.empty()) return;
        const auto& sk = skinMgr.skins[settings.skinIndex % skinMgr.skins.size()];
        audio.loadSkinHitsounds(sk.hitNormalWav, sk.hitWhistleWav, sk.hitClapWav);

        if (!sk.cursorPng.empty())           texSkinCursor.loadFromFile(sk.cursorPng, 64, 64);
        else                                 texSkinCursor.unload();

        if (!sk.hitCirclePng.empty())        texSkinCircle.loadFromFile(sk.hitCirclePng, 128, 128);
        else                                 texSkinCircle.unload();

        if (!sk.hitCircleOverlayPng.empty()) texSkinOverlay.loadFromFile(sk.hitCircleOverlayPng, 128, 128);
        else                                 texSkinOverlay.unload();

        if (!sk.approachCirclePng.empty())   texSkinApproach.loadFromFile(sk.approachCirclePng, 128, 128);
        else                                 texSkinApproach.unload();
    };
    applyActiveSkinAudio();

    AppState appState = STATE_MAIN_MENU;
    AppState settingsReturnState = STATE_MAIN_MENU;
    AppState wifiReturnState = STATE_MAIN_MENU;

    int mainMenuOption = 0; // 0: Jugar, 1: Descargar Wi-Fi, 2: Configuraciones, 3: Salir
    int settingsOption = 0; // 0..9
    int selectedSet = 0;
    int selectedDiff = 0;

    // Selector de Mods en el menú de canciones
    bool modMenuOpen = false;
    int modCursor = 0; // 0..6 (EZ, NF, HT, HR, DT, HD, AUTO)

    // Asistente de calibración en Configuraciones
    bool calibrationMode = false;
    u64 calibStartTick = 0;
    int calibLastBeatIndex = -1;
    std::vector<int> calibSamples;

    // Estado de juego activo
    bool isPaused = false;
    int pauseOption = 0; // 0: Continuar, 1: Reintentar, 2: Volver a Canciones
    bool stageFailed = false;
    bool newRecordAchieved = false;
    float lastEarnedPP = 0.0f;
    float hp = 1.0f;

    float cursorOsuX = 256.0f;
    float cursorOsuY = 192.0f;
    std::vector<TrailPoint> cursorTrail;
    int combo = 0;
    int maxCombo = 0;
    int score = 0;
    int count300 = 0, count100 = 0, count50 = 0, countMiss = 0;
    std::vector<HitPopup> popups;
    int animTick = 0;
    float carouselPos = 0.0f;

    auto updateSongPreview = [&]() {
        if (songMgr.sets.empty()) {
            audio.stopMusic();
            bgTex.unload();
            return;
        }
        const auto& curSet = songMgr.sets[selectedSet];
        if (curSet.difficulties.empty()) return;
        const auto& curDiff = curSet.difficulties[selectedDiff];

        std::string bgPath = !curDiff.backgroundPath.empty() ? curDiff.backgroundPath : curSet.backgroundPath;
        if (!bgPath.empty()) {
            bgTex.loadFromFile(bgPath, 256, 192);
        } else {
            bgTex.unload();
        }

        if (!settings.songPreview) {
            audio.stopMusic();
            return;
        }
        const std::string& audioPath = curDiff.audioPath;
        float speedRate = getModsSpeedMultiplier(settings.activeMods);
        if (!audio.getIsPlaying() || audio.getLoadedMusicPath() != audioPath) {
            audio.startMusic(audioPath, 0, speedRate);
        }
    };

    auto startSelectedBeatmap = [&]() -> bool {
        if (songMgr.sets.empty()) return false;
        const auto& curSet = songMgr.sets[selectedSet];
        if (curSet.difficulties.empty()) return false;
        const auto& diffEntry = curSet.difficulties[selectedDiff];

        if (!beatmap.loadFromFile(diffEntry.osuPath, false)) return false;
        beatmap.applyMods(settings.activeMods);

        if (!beatmap.fullBackgroundPath.empty()) {
            bgTex.loadFromFile(beatmap.fullBackgroundPath, 256, 192);
        }

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

        float speedRate = getModsSpeedMultiplier(settings.activeMods);
        audio.startMusic(beatmap.fullAudioPath, leadIn, speedRate);

        combo = 0;
        maxCombo = 0;
        score = 0;
        count300 = count100 = count50 = countMiss = 0;
        hp = 1.0f;
        stageFailed = false;
        newRecordAchieved = false;
        lastEarnedPP = 0.0f;
        isPaused = false;
        pauseOption = 0;
        cursorOsuX = 256.0f;
        cursorOsuY = 192.0f;
        cursorTrail.clear();
        popups.clear();
        appState = STATE_PLAYING;
        return true;
    };

    auto toggleModByIndex = [&](int idx) {
        static const uint32_t modBits[7] = {
            MOD_EZ, MOD_NF, MOD_HT, MOD_HR, MOD_DT, MOD_HD, MOD_AUTO
        };
        uint32_t bit = modBits[idx];
        if (settings.activeMods & bit) {
            settings.activeMods &= ~bit;
        } else {
            settings.activeMods |= bit;
            // Incompatibilidades clásicas de osu!
            if (bit == MOD_EZ) settings.activeMods &= ~MOD_HR;
            if (bit == MOD_HR) settings.activeMods &= ~MOD_EZ;
            if (bit == MOD_HT) settings.activeMods &= ~MOD_DT;
            if (bit == MOD_DT) settings.activeMods &= ~MOD_HT;
        }
        settings.save();
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

        const SkinProfile& activeSkin = skinMgr.skins[settings.skinIndex % skinMgr.skins.size()];
        u32 clrCursor = C2D_Color32(activeSkin.cursorColor.r, activeSkin.cursorColor.g, activeSkin.cursorColor.b, 255);
        u32 clrPrimary = C2D_Color32(
            activeSkin.comboColors[0].r,
            activeSkin.comboColors[0].g,
            activeSkin.comboColors[0].b,
            235
        );

        // =========================================================
        // 1. MENÚ PRINCIPAL (Jugar / Descargar Wi-Fi / Config / Salir)
        // =========================================================
        if (appState == STATE_MAIN_MENU) {
            if (kDown & KEY_START) break;

            if (kDown & KEY_DUP) {
                mainMenuOption = (mainMenuOption + 3) % 4;
                audio.playHitSound(0);
            }
            if (kDown & KEY_DDOWN) {
                mainMenuOption = (mainMenuOption + 1) % 4;
                audio.playHitSound(0);
            }

            bool activateOption = (kDown & KEY_A) != 0;

            if (touchJustDown) {
                if (touch.px >= 40 && touch.px <= 280) {
                    for (int i = 0; i < 4; ++i) {
                        float by = 16.0f + i * 42.0f;
                        if (touch.py >= by && touch.py <= by + 36.0f) {
                            mainMenuOption = i;
                            activateOption = true;
                            break;
                        }
                    }
                }
            }

            if (activateOption) {
                audio.playHitSound(2);
                if (mainMenuOption == 0) {
                    appState = STATE_SONG_SELECT;
                    updateSongPreview();
                } else if (mainMenuOption == 1) {
                    audio.stopMusic();
                    wifiReturnState = STATE_MAIN_MENU;
                    wifiServer.start();
                    appState = STATE_WIFI_TRANSFER;
                } else if (mainMenuOption == 2) {
                    settingsReturnState = STATE_MAIN_MENU;
                    calibrationMode = false;
                    appState = STATE_SETTINGS;
                } else if (mainMenuOption == 3) {
                    break;
                }
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(34, 14, 56), Ui::rgba(10, 7, 22), clrPrimary, 0.02f);

            // Logo que late al ritmo, con ondas y resplandor
            float beat = std::pow(std::max(0.0f, std::sin(animTick * 0.105f)), 8.0f);
            float logoX = 118.0f;
            float logoY = 118.0f;
            float logoR = 56.0f + beat * 7.0f;

            for (int k = 0; k < 3; ++k) {
                float phase = std::fmod(animTick * 0.012f + k / 3.0f, 1.0f);
                Ui::ring(logoX, logoY, 0.15f, logoR + phase * 60.0f, 2.0f, Ui::withAlpha(clrPrimary, static_cast<u8>((1.0f - phase) * 95.0f)), 40);
            }
            Ui::glow(logoX, logoY, 0.16f, logoR * 1.9f, Ui::rgba(255, 102, 170, 130), 6);
            C2D_DrawCircleSolid(logoX + 3.0f, logoY + 5.0f, 0.18f, logoR, Ui::rgba(0, 0, 0, 120));
            C2D_DrawCircleSolid(logoX, logoY, 0.2f, logoR, Ui::rgba(255, 98, 168, 255));
            C2D_DrawCircleSolid(logoX - logoR * 0.22f, logoY - logoR * 0.28f, 0.21f, logoR * 0.6f, Ui::rgba(255, 255, 255, 30));
            Ui::ring(logoX, logoY, 0.25f, logoR, 3.5f, clrWhite, 40);
            drawCenteredText(textBuf, logoX, logoY - 6.0f, 1.10f, clrWhite, "osu!");
            drawCenteredText(textBuf, logoX, logoY + 24.0f, 0.46f, Ui::rgba(255, 235, 245), "3DS Edition");

            const char* menuLabels[4] = { "JUGAR", "DESCARGAR WI-FI", "CONFIGURACION", "SALIR" };
            for (int i = 0; i < 4; ++i) {
                bool sel = (mainMenuOption == i);
                float bx = sel ? 206.0f : 224.0f;
                float by = 32.0f + i * 46.0f;
                float bw = 210.0f;
                float bh = 36.0f;

                const RGBColor& c = activeSkin.comboColors[i % activeSkin.comboColors.size()];
                u32 base = C2D_Color32(c.r, c.g, c.b, 255);
                Ui::slantRect(bx + 2.0f, by + 3.0f, bw, bh, 12.0f, 0.18f, Ui::rgba(0, 0, 0, 90));
                if (sel) {
                    Ui::slantRectGrad(bx, by, bw, bh, 12.0f, 0.2f, Ui::brighten(base, 1.0f), Ui::lerpColor(base, Ui::rgba(40, 30, 70), 0.6f));
                    C2D_DrawLine(bx + 12.0f, by, clrWhite, bx + bw, by, clrWhite, 1.5f, 0.22f);
                } else {
                    Ui::slantRectGrad(bx, by, bw, bh, 12.0f, 0.2f, Ui::rgba(40, 35, 62, 225), Ui::rgba(22, 19, 36, 225));
                    C2D_DrawLine(bx + 12.0f, by, base, bx, by + bh, base, 3.0f, 0.22f);
                }
                drawCenteredText(textBuf, bx + bw * 0.5f + 6.0f, by + bh * 0.5f, 0.50f, clrWhite, menuLabels[i]);
            }

            Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
            drawText(textBuf, 10.0f, 221.0f, 0.38f, clrMuted, "Arr/Abj + A o toca la pantalla inferior");
            drawText(textBuf, 330.0f, 221.0f, 0.38f, Ui::rgba(150, 215, 255), "v2.0");

            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), clrPrimary, 0.02f);

            for (int i = 0; i < 4; ++i) {
                bool sel = (mainMenuOption == i);
                float bx = 40.0f;
                float by = 16.0f + i * 42.0f;
                float bw = 240.0f;
                float bh = 36.0f;

                const RGBColor& c = activeSkin.comboColors[i % activeSkin.comboColors.size()];
                u32 base = C2D_Color32(c.r, c.g, c.b, 255);
                Ui::slantRect(bx + 2.0f, by + 3.0f, bw, bh, 10.0f, 0.18f, Ui::rgba(0, 0, 0, 90));
                if (sel) {
                    Ui::slantRectGrad(bx, by, bw, bh, 10.0f, 0.2f, base, Ui::lerpColor(base, Ui::rgba(40, 30, 70), 0.6f));
                    C2D_DrawLine(bx + 10.0f, by, clrWhite, bx + bw, by, clrWhite, 1.5f, 0.22f);
                } else {
                    Ui::slantRectGrad(bx, by, bw, bh, 10.0f, 0.2f, Ui::rgba(40, 35, 62, 225), Ui::rgba(22, 19, 36, 225));
                    C2D_DrawLine(bx + 10.0f, by, base, bx, by + bh, base, 3.0f, 0.22f);
                }
                drawCenteredText(textBuf, bx + bw * 0.5f + 5.0f, by + bh * 0.5f, 0.50f, clrWhite, menuLabels[i]);
            }

            Ui::panel(10.0f, 188.0f, 300.0f, 44.0f, 0.2f, Ui::rgba(30, 27, 44, 235), clr300);
            char summary1[96];
            std::snprintf(summary1, sizeof(summary1), "Area Tablet: %d%% | Modo: %s | Offset: %+dms",
                static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                settings.tapWithStylus ? "Touch+Btn" : "Hibrido PC",
                settings.audioOffsetMs);
            drawCenteredText(textBuf, 160.0f, 201.0f, 0.37f, clr300, summary1);

            std::string skinLine = "Skin: " + truncateString(activeSkin.name, 24) + " | Mods: " + formatModsString(settings.activeMods);
            drawCenteredText(textBuf, 160.0f, 219.0f, 0.37f, clrCursor, skinLine.c_str());

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 1B. SERVIDOR WEB WI-FI Y DESCARGADOR DE CANCIONES / SKINS
        // =========================================================
        if (appState == STATE_WIFI_TRANSFER) {
            wifiServer.update();

            if (wifiServer.needsRescan) {
                wifiServer.needsRescan = false;
                songMgr.initAndScan();
                skinMgr.initAndScan();
                selectedSet = 0;
                selectedDiff = 0;
                audio.playHitSound(2);
            }

            bool exitWifi = (kDown & KEY_B) != 0;
            if (touchJustDown && touch.px >= 40 && touch.px <= 280 && touch.py >= 165 && touch.py <= 215) {
                exitWifi = true;
            }

            if (exitWifi) {
                wifiServer.stop();
                songMgr.initAndScan();
                skinMgr.initAndScan();
                audio.playHitSound(0);
                appState = wifiReturnState;
                if (appState == STATE_SONG_SELECT) updateSongPreview();
                continue;
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: URL DEL SERVIDOR Y PROGRESO EN VIVO ---
            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 34.0f, clrPanelBg);
            drawText(textBuf, 12.0f, 8.0f, 0.52f, clr300, "Modo Descargar / Enviar Canciones por Wi-Fi");

            C2D_DrawRectSolid(16.0f, 44.0f, 0.15f, 368.0f, 64.0f, clrPanelBg);
            drawRectOutline(16.0f, 44.0f, 368.0f, 64.0f, 1.5f, clrPrimary);
            drawCenteredText(textBuf, 200.0f, 58.0f, 0.43f, clrWhite,
                "Abre esta direccion en el navegador de tu Celular o PC:");
            drawCenteredText(textBuf, 200.0f, 84.0f, 0.72f, clrCursor,
                wifiServer.serverUrl.empty() ? "Sin conexion Wi-Fi" : wifiServer.serverUrl.c_str());

            drawText(textBuf, 22.0f, 118.0f, 0.41f, clrWhite,
                "1. Sube tus propios archivos .osz (Mapas) o .osk (Skins)");
            drawText(textBuf, 22.0f, 138.0f, 0.41f, clr100,
                "2. O usa el Buscador Online integrado en esa misma web!");

            // Barra de progreso de recepción en tiempo real
            C2D_DrawRectSolid(22.0f, 162.0f, 0.15f, 356.0f, 40.0f, clrPanelBg);
            drawCenteredText(textBuf, 200.0f, 173.0f, 0.42f, clr300,
                truncateString(wifiServer.statusText, 50).c_str());

            float prog = wifiServer.getProgressRatio();
            C2D_DrawRectSolid(34.0f, 186.0f, 0.2f, 332.0f, 10.0f, clrBgDark);
            if (prog > 0.0f) {
                C2D_DrawRectSolid(34.0f, 186.0f, 0.3f, 332.0f * prog, 10.0f, clr100);
            }

            C2D_DrawRectSolid(0.0f, 212.0f, 0.1f, 400.0f, 28.0f, clrPanelBg);
            char footerWifi[96];
            std::snprintf(footerWifi, sizeof(footerWifi),
                "Paquetes instalados en esta sesion: %d | Total canciones: %d",
                wifiServer.uploadedCount, static_cast<int>(songMgr.sets.size()));
            drawCenteredText(textBuf, 200.0f, 226.0f, 0.40f, clrWhite, footerWifi);

            // --- PANTALLA INFERIOR: INSTRUCCIONES Y BOTÓN SALIR ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            C2D_DrawRectSolid(16.0f, 16.0f, 0.1f, 288.0f, 135.0f, clrPanelBg);
            drawCenteredText(textBuf, 160.0f, 34.0f, 0.48f, clrCursor, "Como funciona?");
            drawCenteredText(textBuf, 160.0f, 60.0f, 0.39f, clrWhite, "Tu 3DS ahora aloja una pagina web local.");
            drawCenteredText(textBuf, 160.0f, 80.0f, 0.39f, clrWhite, "Desde tu telefono o PC puedes buscar mapas");
            drawCenteredText(textBuf, 160.0f, 100.0f, 0.39f, clrWhite, "online o arrastrar archivos .osz / .osk.");
            drawCenteredText(textBuf, 160.0f, 124.0f, 0.39f, clr100, "Se extraen solos sin reiniciar la 3DS!");

            C2D_DrawRectSolid(40.0f, 165.0f, 0.2f, 240.0f, 48.0f, clrPrimary);
            drawRectOutline(40.0f, 165.0f, 240.0f, 48.0f, 2.0f, clrWhite);
            drawCenteredText(textBuf, 160.0f, 189.0f, 0.50f, clrWhite, "VOLVER (B)");

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 2. MENÚ DE CONFIGURACIONES (Tablet, Audio, Calibración, Skins)
        // =========================================================
        if (appState == STATE_SETTINGS) {
            if (kDown & KEY_B) {
                calibrationMode = false;
                settings.save();
                audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                audio.playHitSound(0);
                appState = settingsReturnState;
                if (appState == STATE_SONG_SELECT) updateSongPreview();
                continue;
            }

            if (kDown & KEY_X) {
                calibrationMode = !calibrationMode;
                calibSamples.clear();
                calibStartTick = osGetTime();
                calibLastBeatIndex = -1;
                if (calibrationMode) audio.stopMusic();
                audio.playHitSound(2);
            }

            if (kDown & KEY_Y) {
                settings.resetTabletArea();
                audio.playHitSound(0);
            }

            if (calibrationMode) {
                u64 elapsed = osGetTime() - calibStartTick;
                int beatIdx = static_cast<int>(elapsed / 500);
                if (beatIdx > calibLastBeatIndex) {
                    calibLastBeatIndex = beatIdx;
                    audio.playHitSound((beatIdx % 4 == 0) ? 2 : 0);
                }

                u32 tapKeys = KEY_A | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
                if (kDown & tapKeys) {
                    int mod = static_cast<int>(elapsed % 500);
                    int err = (mod > 250) ? (mod - 500) : mod;
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
                const int numSettings = 10;
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
                    int numSkins = std::max(1, static_cast<int>(skinMgr.skins.size()));
                    switch (settingsOption) {
                        case 0: // Tamaño Área Tablet
                            settings.tabletScale += stepDir * 0.05f;
                            break;
                        case 1: // Posición Horizontal X
                            settings.tabletCenterX += stepDir * 8.0f;
                            break;
                        case 2: // Posición Vertical Y
                            settings.tabletCenterY += stepDir * 8.0f;
                            break;
                        case 3: // Modo de Entrada
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
                        case 6: // Offset de Audio
                            settings.audioOffsetMs += stepDir * ((kHeld & (KEY_L | KEY_R)) ? 5 : 1);
                            break;
                        case 7: // Skin Activo (.osk / Presets)
                            settings.skinIndex = (settings.skinIndex + stepDir + numSkins) % numSkins;
                            applyActiveSkinAudio();
                            audio.playHitSound(2);
                            break;
                        case 8: // Tamaño del Cursor / Preview Musical
                            settings.cursorScale += stepDir * 0.1f;
                            break;
                        case 9: // Atenuación del Fondo (Background Dim %)
                            settings.bgDim += stepDir * 5;
                            break;
                    }
                    settings.clampValues();
                }

                if (kDown & KEY_A) {
                    if (settingsOption == 3) settings.tapWithStylus = !settings.tapWithStylus;
                    else if (settingsOption == 8) settings.songPreview = !settings.songPreview;
                    audio.playHitSound(0);
                }
            }

            if (std::abs(cpad.dx) > 20 || std::abs(cpad.dy) > 20) {
                settings.tabletCenterX += (cpad.dx / 156.0f) * 3.0f;
                settings.tabletCenterY -= (cpad.dy / 156.0f) * 3.0f;
                settings.clampValues();
            }

            if (isTouching) {
                if (settingsOption == 1 || settingsOption == 2 || (kHeld & KEY_L)) {
                    settings.tabletCenterX = touch.px;
                    settings.tabletCenterY = touch.py;
                    settings.clampValues();
                }
                settings.mapTouchToOsu(touch.px, touch.py, cursorOsuX, cursorOsuY);
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            drawRectOutline(TOP_OFFSET_X, 0.0f, 320.0f, 240.0f, 1.0f, C2D_Color32(55, 50, 75, 160));

            C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 26.0f, clrPanelBg);
            drawText(textBuf, 12.0f, 4.0f, 0.48f, clrPrimary, "Configuraciones (Tablet, Audio, Calibracion, Skins y Fondo)");

            float rx, ry, rw, rh;
            settings.getTabletRect(rx, ry, rw, rh);

            char valStrings[10][64];
            std::snprintf(valStrings[0], 64, "%d%% (%dx%d px)",
                static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                static_cast<int>(std::round(rw)),
                static_cast<int>(std::round(rh)));
            std::snprintf(valStrings[1], 64, "X = %.0f px (Stylus / C-Pad)", settings.tabletCenterX);
            std::snprintf(valStrings[2], 64, "Y = %.0f px (Stylus / C-Pad)", settings.tabletCenterY);
            std::snprintf(valStrings[3], 64, "%s",
                settings.tapWithStylus ? "Touch + Botones" : "Hibrido PC (Stylus solo apunta)");
            std::snprintf(valStrings[4], 64, "%d%%", settings.musicVolume);
            std::snprintf(valStrings[5], 64, "%d%%", settings.sfxVolume);
            std::snprintf(valStrings[6], 64, "%+d ms (Pulsa X para Asistente)", settings.audioOffsetMs);
            std::snprintf(valStrings[7], 64, "%s (%d/%d)",
                truncateString(activeSkin.name, 22).c_str(),
                settings.skinIndex + 1,
                static_cast<int>(skinMgr.skins.size()));
            std::snprintf(valStrings[8], 64, "Cursor %.1fx | Preview(A): %s",
                settings.cursorScale, settings.songPreview ? "SI" : "NO");
            std::snprintf(valStrings[9], 64, "%d%% Oscuridad (Background Dim)", settings.bgDim);

            const char* labels[10] = {
                "1. Tamano Area Tablet",
                "2. Posicion Tablet X",
                "3. Posicion Tablet Y",
                "4. Modo de Entrada",
                "5. Volumen Musica",
                "6. Volumen Hitsounds",
                "7. Offset Calibracion",
                "8. Skin (.osk / Preset)",
                "9. Cursor y Preview",
                "10. Fondo Cancion (Dim)"
            };

            for (int i = 0; i < 10; ++i) {
                float yPos = 29.0f + i * 18.0f;
                bool sel = (!calibrationMode && settingsOption == i);
                if (sel) {
                    C2D_DrawRectSolid(8.0f, yPos - 1.0f, 0.15f, 384.0f, 17.0f, C2D_Color32(55, 45, 78, 220));
                }
                u32 lblClr = sel ? clrCursor : clrWhite;
                u32 valClr = sel ? clr300 : clrMuted;
                drawText(textBuf, 14.0f, yPos, 0.39f, lblClr, labels[i]);
                drawText(textBuf, 170.0f, yPos, 0.39f, valClr, valStrings[i]);
            }

            if (calibrationMode) {
                // Z-depth 0.90 para el cuadro y 0.98 para el texto encima
                C2D_DrawRectSolid(30.0f, 58.0f, 0.90f, 340.0f, 115.0f, C2D_Color32(18, 16, 30, 248));
                drawRectOutline(30.0f, 58.0f, 340.0f, 115.0f, 2.0f, clr300, 0.94f);
                drawCenteredText(textBuf, 200.0f, 76.0f, 0.50f, clrCursor, "ASISTENTE DE CALIBRACION (120 BPM)", 0.98f);
                drawCenteredText(textBuf, 200.0f, 100.0f, 0.41f, clrWhite, "Pulsa A o L al ritmo del sonido de metronomo", 0.98f);

                char calStr[96];
                std::snprintf(calStr, sizeof(calStr), "Muestras: %d/12 | Offset Calculado: %+d ms",
                    static_cast<int>(calibSamples.size()), settings.audioOffsetMs);
                drawCenteredText(textBuf, 200.0f, 128.0f, 0.45f, clr100, calStr, 0.98f);
                drawCenteredText(textBuf, 200.0f, 154.0f, 0.40f, clr300, "Pulsa X para terminar y aplicar", 0.98f);
            }

            C2D_DrawRectSolid(0.0f, 212.0f, 0.1f, 400.0f, 28.0f, clrPanelBg);
            drawText(textBuf, 8.0f, 218.0f, 0.38f, clr300,
                "Izq/Der: Ajustar | C-Pad: Mover Area | Y: Reset Area | B: Guardar y Salir");

            float testR = 6.0f * settings.cursorScale;
            C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.95f, testR, clrCursor);
            C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.97f, 2.0f, clrWhite);

            // --- PANTALLA INFERIOR: EDITOR INTERACTIVO DE ÁREA DE TABLET ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            C2D_DrawRectSolid(rx, ry, 0.15f, rw, rh, C2D_Color32(42, 36, 66, 230));

            u32 gridClr = C2D_Color32(80, 72, 115, 160);
            C2D_DrawLine(rx + rw * 0.5f, ry, gridClr, rx + rw * 0.5f, ry + rh, gridClr, 1.0f, 0.2f);
            C2D_DrawLine(rx, ry + rh * 0.5f, gridClr, rx + rw, ry + rh * 0.5f, gridClr, 1.0f, 0.2f);

            drawRectOutline(rx, ry, rw, rh, 2.0f, clrPrimary);
            drawRing(settings.tabletCenterX, settings.tabletCenterY, 6.0f, 1.5f, clrCursor, 16);

            char areaCaption[64];
            std::snprintf(areaCaption, sizeof(areaCaption), "AREA ACTIVA %d%% (%dx%d)",
                static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                static_cast<int>(std::round(rw)),
                static_cast<int>(std::round(rh)));
            drawCenteredText(textBuf, settings.tabletCenterX, std::max(12.0f, ry + 12.0f), 0.40f, clrWhite, areaCaption);

            drawCenteredText(textBuf, 160.0f, 228.0f, 0.36f, clrMuted,
                "Skins personalizados (.osk / carpetas): sdmc:/3ds/osu-3ds/skins/");

            if (isTouching) {
                C2D_DrawCircleSolid(touch.px, touch.py, 0.8f, 4.5f, clr300);
            }

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 3. SELECTOR DE CANCIONES, RECORDS Y SELECTOR DE MODS
        // =========================================================
        if (appState == STATE_SONG_SELECT) {
            audio.update();
            if (settings.songPreview && audio.isSongFinished()) {
                updateSongPreview();
            }

            // Abrir/Cerrar menú de Mods con SELECT o botón táctil en pantalla inferior
            if (kDown & KEY_SELECT) {
                modMenuOpen = !modMenuOpen;
                audio.playHitSound(0);
            }
            if (touchJustDown && !modMenuOpen) {
                if (touch.px >= 200 && touch.px <= 305 && touch.py >= 172 && touch.py <= 202) {
                    modMenuOpen = true;
                    audio.playHitSound(0);
                }
            }

            if (modMenuOpen) {
                if (kDown & KEY_B) {
                    modMenuOpen = false;
                    audio.playHitSound(0);
                    updateSongPreview();
                }
                if (kDown & KEY_DLEFT)  { modCursor = (modCursor + 6) % 7; audio.playHitSound(0); }
                if (kDown & KEY_DRIGHT) { modCursor = (modCursor + 1) % 7; audio.playHitSound(0); }
                if (kDown & KEY_DUP)    { modCursor = (modCursor + 4) % 7; audio.playHitSound(0); }
                if (kDown & KEY_DDOWN)  { modCursor = (modCursor + 3) % 7; audio.playHitSound(0); }

                if (kDown & KEY_A) {
                    toggleModByIndex(modCursor);
                    audio.playHitSound(2);
                    updateSongPreview();
                }
                if (kDown & KEY_Y) {
                    settings.activeMods = MOD_NONE;
                    settings.save();
                    audio.playHitSound(0);
                    updateSongPreview();
                }

                // Soporte táctil para las 7 tarjetas de Mods en la pantalla inferior
                if (touchJustDown) {
                    for (int m = 0; m < 7; ++m) {
                        int col = m % 4;
                        int row = m / 4;
                        float mx = 16.0f + col * 73.0f;
                        float my = 56.0f + row * 56.0f;
                        if (touch.px >= mx && touch.px <= mx + 66.0f &&
                            touch.py >= my && touch.py <= my + 46.0f) {
                            modCursor = m;
                            toggleModByIndex(m);
                            audio.playHitSound(2);
                            updateSongPreview();
                            break;
                        }
                    }
                }
            } else {
                if (kDown & KEY_B) {
                    audio.stopMusic();
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
                    audio.stopMusic();
                    audio.playHitSound(2);
                    wifiReturnState = STATE_SONG_SELECT;
                    wifiServer.start();
                    appState = STATE_WIFI_TRANSFER;
                    continue;
                }

                if (!songMgr.sets.empty()) {
                    int numSets = static_cast<int>(songMgr.sets.size());
                    if (kDown & (KEY_DLEFT | KEY_L)) {
                        selectedSet = (selectedSet - 1 + numSets) % numSets;
                        selectedDiff = 0;
                        audio.playHitSound(0);
                        updateSongPreview();
                    }
                    if (kDown & (KEY_DRIGHT | KEY_R)) {
                        selectedSet = (selectedSet + 1) % numSets;
                        selectedDiff = 0;
                        audio.playHitSound(0);
                        updateSongPreview();
                    }

                    const auto& curSet = songMgr.sets[selectedSet];
                    int numDiffs = static_cast<int>(curSet.difficulties.size());
                    if (numDiffs > 0) {
                        if (kDown & KEY_DUP) {
                            selectedDiff = (selectedDiff - 1 + numDiffs) % numDiffs;
                            audio.playHitSound(0);
                            updateSongPreview();
                        }
                        if (kDown & KEY_DDOWN) {
                            selectedDiff = (selectedDiff + 1) % numDiffs;
                            audio.playHitSound(0);
                            updateSongPreview();
                        }

                        if (kDown & KEY_A) {
                            startSelectedBeatmap();
                            continue;
                        }
                    }
                }
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: FONDO DEL BEATMAP, LISTA DE CANCIONES Y ESTRELLAS (★) ---
            C2D_TargetClear(topScreen, clrBgDark);
            C2D_SceneBegin(topScreen);

            if (bgTex.loaded) {
                bgTex.drawRect(0.0f, 0.0f, 400.0f, 240.0f, 0.02f, 255);
                u8 dimAlpha = static_cast<u8>((settings.bgDim * 200) / 100);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 12, 22, dimAlpha));
            } else {
                Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), clrPrimary, 0.02f);
            }
            // Vineta izquierda para dar legibilidad al panel de informacion
            Ui::gradH(0.0f, 0.0f, 240.0f, 240.0f, 0.05f, Ui::rgba(8, 6, 16, 220), Ui::rgba(8, 6, 16, 0));

            // Cabecera
            Ui::gradV(0.0f, 0.0f, 400.0f, 30.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 150));
            C2D_DrawRectSolid(0.0f, 29.0f, 0.31f, 400.0f, 1.5f, Ui::withAlpha(clrPrimary, 200));
            drawText(textBuf, 12.0f, 6.0f, 0.55f, clrWhite, "Seleccion de Cancion");
            char headerInfo[96];
            std::snprintf(headerInfo, sizeof(headerInfo), "%d / %d",
                songMgr.sets.empty() ? 0 : (selectedSet + 1),
                static_cast<int>(songMgr.sets.size()));
            Ui::pill(334.0f, 6.0f, 56.0f, 18.0f, 0.32f, Ui::rgba(60, 50, 90, 230));
            drawCenteredText(textBuf, 362.0f, 15.0f, 0.40f, clrWhite, headerInfo, 0.9f);

            carouselPos = Ui::approach(carouselPos, static_cast<float>(selectedSet), 0.22f);

            if (songMgr.sets.empty()) {
                Ui::panel(40.0f, 70.0f, 320.0f, 100.0f, 0.3f, Ui::rgba(30, 27, 44, 235), clrMiss);
                drawText(textBuf, 56.0f, 82.0f, 0.5f, clrMiss, "No se encontraron beatmaps (.osu / .osz)");
                drawText(textBuf, 56.0f, 110.0f, 0.43f, clrWhite, "Coloca carpetas o archivos .osz en:");
                drawText(textBuf, 56.0f, 130.0f, 0.45f, clr300, "sdmc:/3ds/osu-3ds/songs/");
            } else {
                const int nSets = static_cast<int>(songMgr.sets.size());

                // ---- Carrusel de canciones (derecha), la seleccionada sale hacia la izquierda ----
                for (int off = -4; off <= 4; ++off) {
                    int idx = selectedSet + off;
                    if (idx < 0 || idx >= nSets) continue;
                    float rel = idx - carouselPos;
                    float dist = std::fabs(rel);
                    if (dist > 4.2f) continue;
                    const auto& cs = songMgr.sets[idx];
                    if (cs.difficulties.empty()) continue;

                    bool sel = (idx == selectedSet);
                    float focus = std::max(0.0f, 1.0f - dist);
                    float cardH = 36.0f + focus * 12.0f;
                    float cy = 122.0f + rel * 44.0f;
                    float cx = 214.0f - focus * 40.0f + std::max(0.0f, dist - 1.0f) * 6.0f;
                    float cardY = cy - cardH * 0.5f;
                    float cardW = 400.0f - cx + 14.0f;
                    if (cardY > 240.0f || cardY + cardH < 30.0f) continue;

                    float maxStars = cs.difficulties.back().starRating;
                    u32 starClr = getStarRatingColor(getModAdjustedStars(maxStars, settings.activeMods));
                    u8 fade = static_cast<u8>(255.0f * std::max(0.35f, 1.0f - dist * 0.18f));

                    u32 leftC  = sel ? Ui::withAlpha(Ui::brighten(starClr, 0.85f), 245) : Ui::rgba(36, 32, 56, fade);
                    u32 rightC = sel ? Ui::rgba(52, 40, 86, 245) : Ui::rgba(22, 19, 36, fade);
                    C2D_DrawTriangle(cx + 14.0f + 2.0f, cardY + 3.0f, Ui::rgba(0, 0, 0, 80), cx + cardW + 2.0f, cardY + 3.0f, Ui::rgba(0, 0, 0, 80), cx + 2.0f, cardY + cardH + 3.0f, Ui::rgba(0, 0, 0, 80), 0.09f);
                    Ui::slantRectGrad(cx, cardY, cardW, cardH, 14.0f, 0.1f, leftC, rightC);
                    // borde de acento
                    C2D_DrawLine(cx + 14.0f, cardY, starClr, cx, cardY + cardH, starClr, sel ? 3.0f : 2.0f, 0.12f);
                    if (sel) {
                        C2D_DrawLine(cx + 14.0f, cardY, Ui::rgba(255, 255, 255, 220), cx + cardW, cardY, Ui::rgba(255, 255, 255, 220), 1.5f, 0.12f);
                        C2D_DrawLine(cx, cardY + cardH, Ui::rgba(255, 255, 255, 120), cx + cardW - 14.0f, cardY + cardH, Ui::rgba(255, 255, 255, 120), 1.5f, 0.12f);
                    }

                    u32 titleClr = sel ? clrWhite : Ui::rgba(215, 210, 235, fade);
                    drawText(textBuf, cx + 22.0f, cardY + 4.0f, sel ? 0.50f : 0.43f, titleClr, truncateString(cs.title, sel ? 26 : 24).c_str(), 0.6f);
                    drawText(textBuf, cx + 22.0f, cardY + (sel ? 22.0f : 19.0f), 0.34f, Ui::rgba(190, 185, 215, fade), truncateString(cs.artist, 26).c_str(), 0.6f);
                    if (sel) {
                        drawText(textBuf, cx + 22.0f, cardY + 34.0f, 0.30f, Ui::rgba(255, 224, 120, 255),
                                 truncateString("mapeado por " + cs.difficulties[std::min(selectedDiff, (int)cs.difficulties.size() - 1)].creator, 30).c_str(), 0.6f);
                    }

                    char sTag[16];
                    std::snprintf(sTag, sizeof(sTag), "*%.2f", getModAdjustedStars(maxStars, settings.activeMods));
                    float pw = 40.0f;
                    Ui::pill(400.0f - pw - 6.0f, cardY + cardH * 0.5f - 7.0f, pw, 14.0f, 0.5f, Ui::withAlpha(starClr, fade));
                    drawCenteredText(textBuf, 400.0f - pw * 0.5f - 6.0f, cardY + cardH * 0.5f, 0.32f, Ui::rgba(20, 16, 30), sTag, 0.7f);
                }

                // ---- Panel de informacion (izquierda) ----
                const auto& curSet = songMgr.sets[selectedSet];
                const int totalDiffs = static_cast<int>(curSet.difficulties.size());
                const int dIdx = std::max(0, std::min(selectedDiff, totalDiffs - 1));

                Ui::panel(8.0f, 36.0f, 166.0f, 74.0f, 0.3f, Ui::rgba(24, 21, 40, 225), clrPrimary);
                drawText(textBuf, 16.0f, 40.0f, 0.50f, clrWhite, truncateString(curSet.title, 22).c_str(), 0.6f);
                drawText(textBuf, 16.0f, 58.0f, 0.40f, clrCursor, truncateString(curSet.artist, 26).c_str(), 0.6f);
                if (totalDiffs > 0) {
                    const auto& dd = curSet.difficulties[dIdx];
                    drawText(textBuf, 16.0f, 73.0f, 0.34f, clrMuted, truncateString("Mapa por " + dd.creator, 28).c_str(), 0.6f);
                    char cnt[64];
                    std::snprintf(cnt, sizeof(cnt), "%d circ  %d sld  %d spn", dd.circleCount, dd.sliderCount, dd.spinnerCount);
                    drawText(textBuf, 16.0f, 88.0f, 0.34f, Ui::rgba(150, 215, 255), cnt, 0.6f);
                }

                // ---- Burbujas de dificultad (estilo lazer) ----
                if (totalDiffs > 0) {
                    Ui::panel(8.0f, 118.0f, 166.0f, 86.0f, 0.3f, Ui::rgba(24, 21, 40, 225));
                    const int maxB = 8;
                    int bStart = std::max(0, std::min(dIdx - maxB / 2, totalDiffs - maxB));
                    int bEnd = std::min(totalDiffs, bStart + maxB);
                    for (int i = bStart; i < bEnd; ++i) {
                        float bx = 22.0f + (i - bStart) * 19.0f;
                        float by = 136.0f;
                        bool isSel = (i == dIdx);
                        float st = getModAdjustedStars(curSet.difficulties[i].starRating, settings.activeMods);
                        u32 sc = getStarRatingColor(st);
                        if (isSel) {
                            Ui::glow(bx, by, 0.35f, 15.0f, Ui::withAlpha(sc, 140), 4);
                            C2D_DrawCircleSolid(bx, by, 0.4f, 9.5f, sc);
                            Ui::ring(bx, by, 0.45f, 11.5f, 1.5f, clrWhite, 18);
                        } else {
                            C2D_DrawCircleSolid(bx, by, 0.4f, 6.0f, Ui::withAlpha(sc, 190));
                        }
                    }
                    const auto& sd = curSet.difficulties[dIdx];
                    float selStars = getModAdjustedStars(sd.starRating, settings.activeMods);
                    u32 selClr = getStarRatingColor(selStars);
                    drawText(textBuf, 14.0f, 154.0f, 0.42f, clrWhite, truncateString(sd.version, 24).c_str(), 0.6f);
                    char sl[48];
                    std::snprintf(sl, sizeof(sl), "*%.2f", selStars);
                    Ui::pill(14.0f, 172.0f, 46.0f, 15.0f, 0.5f, selClr);
                    drawCenteredText(textBuf, 37.0f, 179.5f, 0.34f, Ui::rgba(20, 16, 30), sl, 0.7f);
                    const auto* rec = scoreMgr.getRecord(sd.osuPath);
                    if (rec) {
                        std::snprintf(sl, sizeof(sl), "%s  %.1f%%", rec->bestGrade.c_str(), rec->bestAcc);
                        drawText(textBuf, 68.0f, 172.0f, 0.38f, clr100, sl, 0.6f);
                        std::snprintf(sl, sizeof(sl), "%.0f pp", rec->bestPP);
                        drawText(textBuf, 68.0f, 186.0f, 0.34f, clr300, sl, 0.6f);
                    } else {
                        drawText(textBuf, 68.0f, 174.0f, 0.34f, clrMuted, "Sin record", 0.6f);
                    }
                }
            }

            // Barra inferior de atajos
            Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
            drawText(textBuf, 6.0f, 220.0f, 0.36f, clr300,
                "A: Jugar | SELECT: Mods | Y: Wi-Fi | X: Config | B: Menu");

            // --- PANTALLA INFERIOR: DETALLES, ESTRELLAS, PP Y RECORD O SELECTOR DE MODS ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), clrPrimary, 0.02f);
            Ui::panel(10.0f, 10.0f, 300.0f, 220.0f, 0.1f, Ui::rgba(30, 27, 44, 235), clrPrimary);

            if (modMenuOpen) {
                drawText(textBuf, 20.0f, 16.0f, 0.48f, clrCursor, "Selector de Modificadores (Mods)");
                char multStr[64];
                std::snprintf(multStr, sizeof(multStr), "Multiplicador Score: %.2fx", getModsScoreMultiplier(settings.activeMods));
                drawText(textBuf, 20.0f, 35.0f, 0.41f, clr300, multStr);

                const char* modCodes[7] = { "EZ", "NF", "HT", "HR", "DT", "HD", "AUTO" };
                const char* modNames[7] = { "Easy", "NoFail", "0.75x", "HardRock", "1.50x", "Hidden", "Bot" };
                static const uint32_t modBits[7] = {
                    MOD_EZ, MOD_NF, MOD_HT, MOD_HR, MOD_DT, MOD_HD, MOD_AUTO
                };

                for (int m = 0; m < 7; ++m) {
                    int col = m % 4;
                    int row = m / 4;
                    float mx = 16.0f + col * 73.0f;
                    float my = 56.0f + row * 56.0f;
                    bool active = (settings.activeMods & modBits[m]) != 0;
                    bool cursorOn = (modCursor == m);

                    u32 cardBg = active ? C2D_Color32(75, 165, 110, 235) : C2D_Color32(42, 38, 60, 255);
                    C2D_DrawRectSolid(mx, my, 0.2f, 66.0f, 46.0f, cardBg);
                    drawRectOutline(mx, my, 66.0f, 46.0f, cursorOn ? 2.0f : 1.0f, cursorOn ? clrCursor : clrMuted);
                    drawCenteredText(textBuf, mx + 33.0f, my + 16.0f, 0.50f, clrWhite, modCodes[m]);
                    drawCenteredText(textBuf, mx + 33.0f, my + 34.0f, 0.36f, clrWhite, modNames[m]);
                }

                drawText(textBuf, 18.0f, 176.0f, 0.39f, clrWhite, "A / Tocar: Activar Mod | Y: Limpiar Mods");
                drawText(textBuf, 18.0f, 196.0f, 0.39f, clr300, "SELECT o B: Volver a la cancion");
            } else if (!songMgr.sets.empty() && !songMgr.sets[selectedSet].difficulties.empty()) {
                const auto& curSet = songMgr.sets[selectedSet];
                const auto& d = curSet.difficulties[selectedDiff];

                float dispCS = d.cs, dispAR = d.ar, dispOD = d.od, dispHP = d.hp;
                if (settings.activeMods & MOD_EZ) {
                    dispCS *= 0.5f; dispAR *= 0.5f; dispOD *= 0.5f; dispHP *= 0.5f;
                }
                if (settings.activeMods & MOD_HR) {
                    dispCS = std::min(10.0f, dispCS * 1.3f);
                    dispAR = std::min(10.0f, dispAR * 1.4f);
                    dispOD = std::min(10.0f, dispOD * 1.4f);
                    dispHP = std::min(10.0f, dispHP * 1.4f);
                }

                float modStars = getModAdjustedStars(d.starRating, settings.activeMods);
                float maxSSPP  = calculatePP(d.starRating, d.ar, d.od, d.maxPossibleCombo, d.maxPossibleCombo,
                                             d.totalObjects(), 0, 0, 0, settings.activeMods & ~MOD_AUTO);

                char starHeader[96];
                std::snprintf(starHeader, sizeof(starHeader), "Dificultad: *%.2f Estrellas | SS: %.0f pp", modStars, maxSSPP);
                drawText(textBuf, 18.0f, 15.0f, 0.43f, getStarRatingColor(modStars), starHeader);

                char objSummary[96];
                std::snprintf(objSummary, sizeof(objSummary), "Circulos: %d | Sliders: %d | Spinners: %d",
                    d.circleCount, d.sliderCount, d.spinnerCount);
                drawText(textBuf, 18.0f, 34.0f, 0.38f, clrMuted, objSummary);

                u32 barBg = C2D_Color32(18, 16, 26, 255);
                drawStatBar(textBuf, 18.0f, 54.0f,  "CS", dispCS, clrPrimary, barBg, clrWhite);
                drawStatBar(textBuf, 18.0f, 72.0f,  "AR", dispAR, clr100,     barBg, clrWhite);
                drawStatBar(textBuf, 18.0f, 90.0f,  "OD", dispOD, clr300,     barBg, clrWhite);
                drawStatBar(textBuf, 18.0f, 108.0f, "HP", dispHP, clr50,      barBg, clrWhite);

                // Tarjeta de Récord Personal + Mejor PP a la derecha
                C2D_DrawRectSolid(176.0f, 52.0f, 0.2f, 126.0f, 78.0f, C2D_Color32(22, 20, 34, 255));
                drawRectOutline(176.0f, 52.0f, 126.0f, 78.0f, 1.0f, clrMuted);
                const auto* rec = scoreMgr.getRecord(d.osuPath);
                if (rec) {
                    char recLine[64];
                    std::snprintf(recLine, sizeof(recLine), "Rango: %s (%.1f%%)", rec->bestGrade.c_str(), rec->bestAcc);
                    drawText(textBuf, 182.0f, 56.0f, 0.39f, clrCursor, recLine);
                    std::snprintf(recLine, sizeof(recLine), "Best PP: %.1f pp", rec->bestPP);
                    drawText(textBuf, 182.0f, 74.0f, 0.39f, clr300, recLine);
                    std::snprintf(recLine, sizeof(recLine), "Score: %d", rec->bestScore);
                    drawText(textBuf, 182.0f, 92.0f, 0.38f, clrWhite, recLine);
                    std::snprintf(recLine, sizeof(recLine), "Combo: %dx", rec->bestCombo);
                    drawText(textBuf, 182.0f, 109.0f, 0.38f, clr100, recLine);
                } else {
                    drawCenteredText(textBuf, 239.0f, 90.0f, 0.40f, clrMuted, "Sin Record aun");
                }

                std::string modLine = "Mods: " + formatModsString(settings.activeMods);
                drawText(textBuf, 18.0f, 138.0f, 0.42f, clrCursor, modLine.c_str());

                std::string skinStr = "Skin: " + truncateString(activeSkin.name, 26);
                drawText(textBuf, 18.0f, 158.0f, 0.40f, clrWhite, skinStr.c_str());

                // Botón táctil para abrir el menú de Mods
                C2D_DrawRectSolid(200.0f, 172.0f, 0.2f, 102.0f, 30.0f, clrPrimary);
                drawRectOutline(200.0f, 172.0f, 102.0f, 30.0f, 1.5f, clrWhite);
                drawCenteredText(textBuf, 251.0f, 187.0f, 0.43f, clrWhite, "MODS (SELECT)");

                drawText(textBuf, 18.0f, 208.0f, 0.36f, clrMuted, "Audio MP3/OGG + Fondos JPG/PNG activos");
            }

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 4. PANTALLA DE RESULTADOS (Ranking Final + PP + Récords)
        // =========================================================
        if (appState == STATE_RESULTS) {
            if (kDown & KEY_A) {
                startSelectedBeatmap();
                continue;
            }
            if (kDown & (KEY_B | KEY_START | KEY_SELECT)) {
                audio.playHitSound(0);
                appState = STATE_SONG_SELECT;
                updateSongPreview();
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

            if (bgTex.loaded) {
                bgTex.drawRect(0.0f, 0.0f, 400.0f, 240.0f, 0.02f, 255);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 12, 22, 190));
            } else {
                Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), gradeClr, 0.02f);
            }

            Ui::gradV(0.0f, 0.0f, 400.0f, 30.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 150));
            C2D_DrawRectSolid(0.0f, 29.0f, 0.31f, 400.0f, 1.5f, Ui::withAlpha(gradeClr, 220));
            drawText(textBuf, 12.0f, 6.0f, 0.55f, clrWhite,
                stageFailed ? "PARTIDA FALLIDA" : (newRecordAchieved ? "RESULTADOS - NUEVO RECORD!" : "RESULTADOS"));

            // Rango gigante con resplandor pulsante
            float gPulse = 1.0f + std::sin(animTick * 0.08f) * 0.03f;
            Ui::glow(88.0f, 124.0f, 0.15f, 78.0f * gPulse, Ui::withAlpha(gradeClr, 120), 6);
            C2D_DrawCircleSolid(90.0f, 128.0f, 0.18f, 52.0f, Ui::rgba(0, 0, 0, 110));
            C2D_DrawCircleSolid(88.0f, 124.0f, 0.2f, 52.0f, Ui::rgba(24, 20, 40, 245));
            Ui::ring(88.0f, 124.0f, 0.25f, 52.0f * gPulse, 4.0f, gradeClr, 40);
            drawCenteredText(textBuf, 88.0f, 120.0f, 1.55f, gradeClr, grade);

            // Panel de resultados
            char buf[96];
            Ui::panel(158.0f, 38.0f, 234.0f, 62.0f, 0.3f, Ui::rgba(24, 21, 40, 230), gradeClr);
            drawText(textBuf, 168.0f, 42.0f, 0.36f, clrMuted, "PUNTUACION");
            std::snprintf(buf, sizeof(buf), "%07d", score);
            drawText(textBuf, 168.0f, 54.0f, 0.78f, clrWhite, buf);
            float modStars = getModAdjustedStars(beatmap.starRating, settings.activeMods);
            std::snprintf(buf, sizeof(buf), "%.1f pp", lastEarnedPP);
            Ui::pill(318.0f, 46.0f, 66.0f, 18.0f, 0.5f, Ui::rgba(70, 55, 110, 240));
            drawCenteredText(textBuf, 351.0f, 55.0f, 0.42f, clr300, buf, 0.9f);
            std::snprintf(buf, sizeof(buf), "*%.2f", modStars);
            Ui::pill(318.0f, 70.0f, 66.0f, 16.0f, 0.5f, getStarRatingColor(modStars));
            drawCenteredText(textBuf, 351.0f, 78.0f, 0.36f, Ui::rgba(20, 16, 30), buf, 0.9f);

            // Precision con barra
            Ui::panel(158.0f, 106.0f, 234.0f, 34.0f, 0.3f, Ui::rgba(24, 21, 40, 230));
            std::snprintf(buf, sizeof(buf), "Precision %.2f%%", acc);
            drawText(textBuf, 168.0f, 109.0f, 0.40f, clr300, buf);
            std::snprintf(buf, sizeof(buf), "Combo %dx", maxCombo);
            drawText(textBuf, 320.0f, 109.0f, 0.40f, clrCursor, buf);
            Ui::statBar(168.0f, 128.0f, 214.0f, 6.0f, 0.4f, acc / 100.0f, clr300);

            // Contadores 300 / 100 / 50 / Miss
            const char* hitLbl[4] = { "300", "100", "50", "X" };
            int hitCnt[4] = { count300, count100, count50, countMiss };
            u32 hitClr[4] = { clr300, clr100, clr50, clrMiss };
            for (int i = 0; i < 4; ++i) {
                float hx = 158.0f + i * 59.5f;
                Ui::panel(hx, 146.0f, 55.0f, 44.0f, 0.3f, Ui::rgba(24, 21, 40, 230));
                C2D_DrawRectSolid(hx, 146.0f, 0.4f, 55.0f, 3.0f, hitClr[i]);
                drawCenteredText(textBuf, hx + 27.5f, 160.0f, 0.42f, hitClr[i], hitLbl[i], 0.9f);
                std::snprintf(buf, sizeof(buf), "%d", hitCnt[i]);
                drawCenteredText(textBuf, hx + 27.5f, 177.0f, 0.50f, clrWhite, buf, 0.9f);
            }
            std::string modStr = "Mods: " + formatModsString(settings.activeMods);
            drawText(textBuf, 160.0f, 196.0f, 0.36f, clrMuted, modStr.c_str());

            Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
            drawCenteredText(textBuf, 200.0f, 227.0f, 0.42f, clr300, "A: Reintentar | B: Volver a Canciones");

            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), gradeClr, 0.02f);
            Ui::panel(20.0f, 70.0f, 280.0f, 80.0f, 0.2f, Ui::rgba(30, 27, 44, 240), gradeClr);
            drawCenteredText(textBuf, 160.0f, 95.0f, 0.52f, clrWhite, truncateString(beatmap.title, 32).c_str());
            std::snprintf(buf, sizeof(buf), "[%s]  %.1f pp", truncateString(beatmap.version, 22).c_str(), lastEarnedPP);
            drawCenteredText(textBuf, 160.0f, 122.0f, 0.44f, clr300, buf);
            if (newRecordAchieved) {
                float flash = 0.65f + 0.35f * std::sin(animTick * 0.2f);
                Ui::pill(40.0f, 168.0f, 240.0f, 24.0f, 0.3f, Ui::withAlpha(clrCursor, static_cast<u8>(255.0f * flash)));
                drawCenteredText(textBuf, 160.0f, 180.0f, 0.46f, Ui::rgba(20, 16, 30), "NUEVO RECORD PERSONAL!", 0.9f);
            }
            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 5. JUEGO ACTIVO + MENÚ DE PAUSA CORREGIDO
        // =========================================================
        if (kDown & (KEY_START | KEY_SELECT)) {
            if (!isPaused) {
                isPaused = true;
                pauseOption = 0;
                audio.pauseMusic();
                audio.playHitSound(0);
            } else {
                isPaused = false;
                audio.resumeMusic();
                audio.playHitSound(0);
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

            bool triggerPauseAction = (kDown & KEY_A) != 0;

            // Botones táctiles en la pantalla inferior durante la pausa
            if (touchJustDown && touch.px >= 40 && touch.px <= 280) {
                if (touch.py >= 40 && touch.py <= 84) {
                    pauseOption = 0;
                    triggerPauseAction = true;
                } else if (touch.py >= 98 && touch.py <= 142) {
                    pauseOption = 1;
                    triggerPauseAction = true;
                } else if (touch.py >= 156 && touch.py <= 200) {
                    pauseOption = 2;
                    triggerPauseAction = true;
                }
            }

            if (triggerPauseAction) {
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
                    updateSongPreview();
                    continue;
                }
            }
        } else {
            audio.update();
        }

        int songTime = audio.getTimeMs() - settings.audioOffsetMs;
        bool isAutoplay = (settings.activeMods & MOD_AUTO) != 0;
        bool isHidden   = (settings.activeMods & MOD_HD) != 0;
        bool isNoFail   = (settings.activeMods & MOD_NF) != 0;
        float scoreMult = getModsScoreMultiplier(settings.activeMods);

        u32 btnGroup1 = KEY_L | KEY_DLEFT | KEY_A | KEY_X | (settings.tapWithStylus ? KEY_TOUCH : 0);
        u32 btnGroup2 = KEY_R | KEY_ZL | KEY_ZR | KEY_DDOWN | KEY_DUP | KEY_DRIGHT | KEY_B | KEY_Y;
        bool k1Held = (kHeld & btnGroup1) != 0;
        bool k2Held = (kHeld & btnGroup2) != 0;
        bool anyHeld = (k1Held || k2Held);
        bool kActionDown = (kDown & (btnGroup1 | btnGroup2)) != 0;

        if (!isPaused) {
            if (isAutoplay) {
                // Bot Autoplay: calcula la posición ideal del cursor e inputs automáticos
                anyHeld = false;
                k1Held = false;
                kActionDown = false;
                for (size_t i = 0; i < beatmap.circles.size(); ++i) {
                    const auto& obj = beatmap.circles[i];
                    bool done = obj.isSlider ? obj.sliderDone :
                               (obj.isSpinner ? obj.sliderDone : (obj.state != NOT_HIT));
                    if (done) continue;

                    if (obj.isSpinner && songTime >= obj.time && songTime <= obj.endTime) {
                        float spinAng = songTime * 0.035f;
                        cursorOsuX = 256.0f + std::cos(spinAng) * 55.0f;
                        cursorOsuY = 192.0f + std::sin(spinAng) * 55.0f;
                        anyHeld = k1Held = true;
                        break;
                    } else if (obj.isSlider && songTime >= obj.time && songTime <= obj.endTime) {
                        Vec2 ball = obj.getSliderBallPos(songTime);
                        cursorOsuX = ball.x;
                        cursorOsuY = ball.y;
                        anyHeld = k1Held = true;
                        if (obj.state == NOT_HIT) kActionDown = true;
                        break;
                    } else {
                        // Interpolar suavemente hacia la siguiente nota
                        float dt = static_cast<float>(obj.time - songTime);
                        if (dt <= 0.0f) {
                            cursorOsuX = obj.x;
                            cursorOsuY = obj.y;
                            kActionDown = true;
                            k1Held = anyHeld = true;
                        } else if (dt < beatmap.diff.preempt) {
                            float lerpFactor = 0.28f;
                            cursorOsuX += (obj.x - cursorOsuX) * lerpFactor;
                            cursorOsuY += (obj.y - cursorOsuY) * lerpFactor;
                        }
                        break;
                    }
                }
            } else if (isTouching) {
                settings.mapTouchToOsu(touch.px, touch.py, cursorOsuX, cursorOsuY);
            }

            // Actualizar estela del cursor (Cursor Trail)
            if (activeSkin.cursorTrail) {
                if (cursorTrail.empty() ||
                    std::fabs(cursorTrail.back().x - cursorOsuX) > 1.5f ||
                    std::fabs(cursorTrail.back().y - cursorOsuY) > 1.5f) {
                    cursorTrail.push_back({cursorOsuX, cursorOsuY, 12});
                    if (cursorTrail.size() > 14) cursorTrail.erase(cursorTrail.begin());
                }
                for (size_t t = 0; t < cursorTrail.size(); ) {
                    cursorTrail[t].life--;
                    if (cursorTrail[t].life <= 0) cursorTrail.erase(cursorTrail.begin() + t);
                    else ++t;
                }
            } else {
                cursorTrail.clear();
            }

            float hitRadius = beatmap.diff.radiusOsu;
            float hit50     = beatmap.diff.hit50;
            float hit100    = beatmap.diff.hit100;
            float hit300    = beatmap.diff.hit300;

            if (!beatmap.circles.empty() && songTime >= beatmap.circles.front().time &&
                songTime <= beatmap.circles.back().endTime) {
                hp -= (0.00018f * beatmap.diff.hp);
                if (hp <= 0.0f) {
                    if (isNoFail || isAutoplay) {
                        hp = 0.05f;
                    } else {
                        hp = 0.0f;
                        stageFailed = true;
                        lastEarnedPP = 0.0f;
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

                u32 objBurstClr = C2D_Color32(
                    activeSkin.comboColors[obj.colorIndex % activeSkin.comboColors.size()].r,
                    activeSkin.comboColors[obj.colorIndex % activeSkin.comboColors.size()].g,
                    activeSkin.comboColors[obj.colorIndex % activeSkin.comboColors.size()].b,
                    200
                );

                // 1. Evaluar Spinners activos
                if (obj.isSpinner && !obj.sliderDone && songTime >= obj.time) {
                    if (((anyHeld || !settings.tapWithStylus) && isTouching) || isAutoplay) {
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
                        if (obj.spinnerProgress >= obj.spinnerRequiredRads * 0.65f || isAutoplay) {
                            obj.state = HIT_300;
                            audio.playHitSound(obj.hitSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                            count300++;
                            hp = std::min(1.0f, hp + 0.08f);
                        } else if (obj.spinnerProgress >= obj.spinnerRequiredRads * 0.35f) {
                            obj.state = HIT_100;
                            audio.playHitSound(obj.hitSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>(100 * scoreMult);
                            count100++;
                            hp = std::min(1.0f, hp + 0.04f);
                        } else {
                            obj.state = HIT_MISS;
                            combo = 0;
                            countMiss++;
                            hp = std::max(0.0f, hp - 0.12f);
                        }
                        popups.push_back(HitPopup(256.0f, 192.0f, obj.state, hitRadius * SCALE_FACTOR, objBurstClr, 20));
                    }
                    continue;
                }

                // 2. Evaluar seguimiento, Slider Ticks, rebotes y finalización de Sliders activos
                if (obj.isSlider && !obj.sliderDone && songTime >= obj.time) {
                    Vec2 ballPos = obj.getSliderBallPos(songTime);
                    float dx = cursorOsuX - ballPos.x;
                    float dy = cursorOsuY - ballPos.y;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    obj.sliderTracking = isAutoplay || (anyHeld && dist <= hitRadius * 2.4f);

                    // Evaluar Slider Ticks intermedios
                    for (auto& tk : obj.sliderTicks) {
                        if (!tk.hit && songTime >= tk.time) {
                            tk.hit = true;
                            if (obj.sliderTracking && obj.state != HIT_MISS) {
                                combo++;
                                if (combo > maxCombo) maxCombo = combo;
                                score += static_cast<int>(10 * scoreMult);
                                hp = std::min(1.0f, hp + 0.015f);
                            }
                        }
                    }

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
                                score += static_cast<int>(30 * scoreMult);
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
                            score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                            count300++;
                            hp = std::min(1.0f, hp + 0.05f);
                            popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_300, hitRadius * SCALE_FACTOR, objBurstClr, 18));
                        } else if (obj.state == NOT_HIT) {
                            obj.state = HIT_MISS;
                            combo = 0;
                            countMiss++;
                            hp = std::max(0.0f, hp - 0.10f);
                            popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_MISS, hitRadius * SCALE_FACTOR, clrMiss, 22));
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
                    popups.push_back(HitPopup(obj.x, obj.y, HIT_MISS, hitRadius * SCALE_FACTOR, clrMiss, 22));
                    continue;
                }

                // 4. Evaluar golpe (Tap) en la nota
                if (kActionDown) {
                    int timeDelta = isAutoplay ? 0 : std::abs(songTime - obj.time);
                    if (timeDelta <= hit50) {
                        float dx = cursorOsuX - obj.x;
                        float dy = cursorOsuY - obj.y;
                        float dist = isAutoplay ? 0.0f : std::sqrt(dx * dx + dy * dy);

                        if (dist <= hitRadius * 1.2f) {
                            if (timeDelta <= hit300) {
                                obj.state = HIT_300;
                                score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                                count300++;
                                hp = std::min(1.0f, hp + 0.06f);
                            } else if (timeDelta <= hit100) {
                                obj.state = HIT_100;
                                score += static_cast<int>((100 + 100 * combo * 0.05f) * scoreMult);
                                count100++;
                                hp = std::min(1.0f, hp + 0.025f);
                            } else {
                                obj.state = HIT_50;
                                score += static_cast<int>((50 + 50 * combo * 0.05f) * scoreMult);
                                count50++;
                            }
                            int headSound = obj.isSlider ? obj.getEdgeHitSound(0) : obj.hitSound;
                            audio.playHitSound(headSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            if (obj.isSlider) obj.sliderTracking = true;
                            popups.push_back(HitPopup(obj.x, obj.y, obj.state, hitRadius * SCALE_FACTOR, objBurstClr, 18));
                            kActionDown = false;
                            break;
                        }
                    }
                }
            }

            if ((allObjectsDone && !beatmap.circles.empty() &&
                 songTime > beatmap.circles.back().endTime + 1000) ||
                audio.isSongFinished()) {
                audio.stopMusic();

                int totalJudged = count300 + count100 + count50 + countMiss;
                float acc = (totalJudged > 0)
                    ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                    : 100.0f;
                const char* grade = "D";
                if (acc >= 99.9f && countMiss == 0) grade = "SS";
                else if (acc >= 93.0f && countMiss == 0) grade = "S";
                else if (acc >= 85.0f) grade = "A";
                else if (acc >= 75.0f) grade = "B";
                else if (acc >= 65.0f) grade = "C";

                lastEarnedPP = calculatePP(
                    beatmap.starRating,
                    beatmap.diff.ar,
                    beatmap.diff.od,
                    maxCombo,
                    beatmap.maxPossibleCombo,
                    count300, count100, count50, countMiss,
                    settings.activeMods
                );

                newRecordAchieved = scoreMgr.submitScore(
                    beatmap.filePath, score, maxCombo, acc, grade, settings.activeMods, lastEarnedPP
                );
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

        if (bgTex.loaded) {
            bgTex.drawRect(TOP_OFFSET_X, 0.0f, 320.0f, 240.0f, 0.01f, 255);
            u8 dimAlpha = static_cast<u8>((settings.bgDim * 255) / 100);
            C2D_DrawRectSolid(TOP_OFFSET_X, 0.0f, 0.02f, 320.0f, 240.0f, C2D_Color32(14, 12, 22, dimAlpha));
        } else {
            C2D_DrawRectSolid(TOP_OFFSET_X, 0.0f, 0.0f, 320.0f, 240.0f, clrPlayfield);
        }

        float hitRadius = beatmap.diff.radiusOsu;
        float preempt   = beatmap.diff.preempt;
        float fadeIn    = beatmap.diff.fadeIn;
        float hit50     = beatmap.diff.hit50;
        float rScreen   = hitRadius * SCALE_FACTOR;

        const std::vector<RGBColor>& activePalette =
            !beatmap.customComboColors.empty() ? beatmap.customComboColors : activeSkin.comboColors;

        // 0. Follow Points entre notas consecutivas del mismo combo
        for (size_t i = 0; i + 1 < beatmap.circles.size(); ++i) {
            const auto& a = beatmap.circles[i];
            const auto& b = beatmap.circles[i + 1];
            if (a.isSpinner || b.isSpinner || b.comboNumber == 1) continue;
            if (songTime < b.time - preempt || songTime > b.time) continue;

            Vec2 startPos = a.getEndPosition();
            float x0 = osuToTopX(startPos.x);
            float y0 = osuToTopY(startPos.y);
            float x1 = osuToTopX(b.x);
            float y1 = osuToTopY(b.y);
            float dx = x1 - x0;
            float dy = y1 - y0;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > rScreen * 2.4f) {
                u32 fpColor = C2D_Color32(255, 255, 255, 70);
                float tStart = (rScreen * 0.9f) / dist;
                float tEnd   = 1.0f - (rScreen * 0.9f) / dist;
                C2D_DrawLine(
                    x0 + dx * tStart, y0 + dy * tStart, fpColor,
                    x0 + dx * tEnd,   y0 + dy * tEnd,   fpColor,
                    1.0f, 0.05f
                );
            }
        }

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

            // Calcular opacidad (Fade-In normal y Fade-Out en modo Hidden HD)
            float alphaMult = 1.0f;
            float timeSinceAppear = static_cast<float>(songTime - (obj.time - preempt));
            if (timeSinceAppear < fadeIn && fadeIn > 1.0f) {
                alphaMult = std::max(0.0f, std::min(1.0f, timeSinceAppear / fadeIn));
            }
            if (isHidden && !obj.isSpinner) {
                float fadeOutStart = preempt * 0.4f;
                float fadeOutEnd   = preempt * 0.72f;
                if (timeSinceAppear > fadeOutStart) {
                    float t = (timeSinceAppear - fadeOutStart) / std::max(1.0f, fadeOutEnd - fadeOutStart);
                    alphaMult = std::max(0.0f, 1.0f - t);
                }
            }

            const auto& rgb = activePalette[obj.colorIndex % activePalette.size()];
            u8 cAlpha = static_cast<u8>(235.0f * alphaMult);
            u8 sAlpha = static_cast<u8>(165.0f * (isHidden ? std::max(0.25f, alphaMult) : 1.0f));
            u32 cColor = C2D_Color32(rgb.r, rgb.g, rgb.b, cAlpha);
            u32 sColor = activeSkin.hasSliderTrackOverride
                ? C2D_Color32(activeSkin.sliderTrackColor.r, activeSkin.sliderTrackColor.g, activeSkin.sliderTrackColor.b, sAlpha)
                : C2D_Color32(static_cast<u8>(rgb.r * 0.62f), static_cast<u8>(rgb.g * 0.62f), static_cast<u8>(rgb.b * 0.62f), sAlpha);
            u32 ringClr = C2D_Color32(activeSkin.sliderBorderColor.r, activeSkin.sliderBorderColor.g, activeSkin.sliderBorderColor.b, cAlpha);

            if (obj.isSpinner) {
                float spX = osuToTopX(256.0f);
                float spY = osuToTopY(192.0f);
                float ratio = std::min(1.0f, obj.spinnerProgress / obj.spinnerRequiredRads);
                drawRing(spX, spY, 75.0f * (1.0f - ratio * 0.7f), 2.5f, clr300, 28);
                C2D_DrawCircleSolid(spX, spY, 0.25f, 6.0f, clrWhite);
                continue;
            }

            // 1. Cuerpo del Slider, Ticks, Flechas de Rebote y Bola
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
                drawRing(tailX, tailY, rScreen * 0.85f, 1.5f, ringClr, 16);

                // Dibujar Slider Ticks pendientes
                int curSpan = 0;
                if (songTime > obj.time && obj.endTime > obj.time) {
                    float slideDur = static_cast<float>(obj.endTime - obj.time) / obj.repeats;
                    curSpan = static_cast<int>((songTime - obj.time) / slideDur);
                }
                for (const auto& tk : obj.sliderTicks) {
                    if (!tk.hit && tk.time >= songTime - 50) {
                        C2D_DrawCircleSolid(osuToTopX(tk.x), osuToTopY(tk.y), 0.18f, 2.2f, clrWhite);
                    }
                }

                // Flecha de rebote (Reverse Arrow) si quedan repeticiones
                if (obj.repeats > 1 && curSpan + 1 < obj.repeats) {
                    bool arrowAtTail = (curSpan % 2 == 0);
                    if (arrowAtTail) {
                        const Vec2& pTip = obj.sliderPath.back();
                        const Vec2& pPrev = obj.sliderPath[obj.sliderPath.size() - 2];
                        drawReverseArrow(
                            osuToTopX(pTip.x), osuToTopY(pTip.y),
                            osuToTopX(pPrev.x), osuToTopY(pPrev.y),
                            rScreen * 0.65f, clrWhite
                        );
                    } else {
                        const Vec2& pTip = obj.sliderPath.front();
                        const Vec2& pNext = obj.sliderPath[1];
                        drawReverseArrow(
                            osuToTopX(pTip.x), osuToTopY(pTip.y),
                            osuToTopX(pNext.x), osuToTopY(pNext.y),
                            rScreen * 0.65f, clrWhite
                        );
                    }
                }

                if (songTime >= obj.time && songTime <= obj.endTime) {
                    Vec2 ballOsu = obj.getSliderBallPos(songTime);
                    float bx = osuToTopX(ballOsu.x);
                    float by = osuToTopY(ballOsu.y);
                    C2D_DrawCircleSolid(bx, by, 0.35f, rScreen * 0.88f, C2D_Color32(rgb.r, rgb.g, rgb.b, 240));
                    float followR = obj.sliderTracking ? (rScreen * 1.9f) : (rScreen * 1.3f);
                    u32 followClr = obj.sliderTracking ? clrCursor : clrWhite;
                    drawRing(bx, by, followR, 1.5f, followClr, 20);
                }
            }

            // 2. Cabeza del círculo y número (con soporte para Sprites PNG de Skin o Vectorial 60 FPS)
            if (obj.state == NOT_HIT && songTime <= obj.time + hit50 && cAlpha > 8) {
                float cx = osuToTopX(obj.x);
                float cy = osuToTopY(obj.y);

                if (texSkinCircle.loaded) {
                    texSkinCircle.drawCenteredSprite(cx, cy, rScreen * 1.05f, 0.20f, cColor);
                    if (texSkinOverlay.loaded) {
                        texSkinOverlay.drawCenteredSprite(cx, cy, rScreen * 1.05f, 0.22f, C2D_Color32(255, 255, 255, cAlpha));
                    }
                } else if (activeSkin.darkCircleCenter) {
                    C2D_DrawCircleSolid(cx, cy, 0.2f, rScreen, C2D_Color32(20, 18, 30, static_cast<u8>(220 * alphaMult)));
                    drawRing(cx, cy, rScreen, activeSkin.ringThickness, cColor, 22);
                } else {
                    C2D_DrawCircleSolid(cx, cy, 0.2f, rScreen, cColor);
                    drawRing(cx, cy, rScreen, activeSkin.ringThickness, ringClr, 22);
                }

                char numStr[16];
                std::snprintf(numStr, sizeof(numStr), "%d", obj.comboNumber);
                drawCenteredText(textBuf, cx, cy, 0.55f, C2D_Color32(255, 255, 255, cAlpha), numStr, 0.30f);

                // Anillo de aproximación (oculto en modo Hidden HD)
                if (!isHidden) {
                    float timeLeft = static_cast<float>(obj.time - songTime);
                    if (timeLeft > 0.0f) {
                        float progress = timeLeft / preempt;
                        float approachRadius = rScreen * (1.0f + 2.5f * progress);
                        if (texSkinApproach.loaded) {
                            texSkinApproach.drawCenteredSprite(cx, cy, approachRadius, 0.24f, cColor);
                        } else {
                            drawRing(cx, cy, approachRadius, 1.5f, cColor, 20);
                        }
                    }
                }
            }
        }

        // 3. Efecto Hit Burst y calificaciones flotantes (300 / 100 / 50 / X)
        for (size_t i = 0; i < popups.size(); ) {
            float px = osuToTopX(popups[i].x);
            float py = osuToTopY(popups[i].y);
            float age = static_cast<float>(18 - popups[i].timer);

            if (popups[i].state != HIT_MISS && popups[i].timer > 6) {
                float burstR = popups[i].burstRadius * (1.0f + age * 0.045f);
                u8 bAlpha = static_cast<u8>(std::max(0, popups[i].timer * 10));
                u32 bClr = (popups[i].burstColor & 0x00FFFFFF) | (static_cast<u32>(bAlpha) << 24);
                drawRing(px, py, burstR, 1.8f, bClr, 18);
            }

            const char* txt = "300";
            u32 clr = clr300;
            if (popups[i].state == HIT_100)      { txt = "100"; clr = clr100; }
            else if (popups[i].state == HIT_50)  { txt = "50";  clr = clr50; }
            else if (popups[i].state == HIT_MISS){ txt = "X";   clr = clrMiss; }

            drawCenteredText(textBuf, px, py - age * 0.6f, 0.5f, clr, txt, 0.80f);

            if (!isPaused) popups[i].timer--;
            if (popups[i].timer <= 0) {
                popups.erase(popups.begin() + i);
            } else {
                ++i;
            }
        }

        // Barra de Vida (HP Bar) con degradado y borde
        u32 hpClr = (hp > 0.5f) ? clr100 : ((hp > 0.25f) ? clr50 : clrMiss);
        C2D_DrawRectSolid(5.0f, 5.0f, 0.29f, 148.0f, 11.0f, Ui::rgba(0, 0, 0, 170));
        C2D_DrawRectSolid(7.0f, 7.0f, 0.3f, 144.0f, 7.0f, clrKeyIdle);
        if (hp > 0.0f) Ui::gradH(7.0f, 7.0f, 144.0f * hp, 7.0f, 0.35f, Ui::brighten(hpClr, 0.6f), hpClr);
        if (hp < 0.25f) {
            float blink = 0.5f + 0.5f * std::sin(animTick * 0.35f);
            C2D_DrawRectSolid(5.0f, 5.0f, 0.4f, 148.0f, 11.0f, Ui::rgba(255, 60, 60, static_cast<int>(60 * blink)));
        }

        // Barra de progreso de la cancion
        if (!beatmap.circles.empty()) {
            float endT = static_cast<float>(beatmap.circles.back().endTime + 1000);
            float prog = std::max(0.0f, std::min(1.0f, static_cast<float>(songTime) / std::max(1.0f, endT)));
            C2D_DrawRectSolid(5.0f, 18.0f, 0.3f, 148.0f, 2.0f, Ui::rgba(255, 255, 255, 40));
            C2D_DrawRectSolid(5.0f, 18.0f, 0.35f, 148.0f * prog, 2.0f, clrPrimary);
        }

        // HUD: Score, Accuracy, Live PP, Mods y Combo
        char hudStr[64];
        Ui::gradH(300.0f, 2.0f, 100.0f, 52.0f, 0.28f, Ui::rgba(10, 8, 20, 0), Ui::rgba(10, 8, 20, 170));
        std::snprintf(hudStr, sizeof(hudStr), "%07d", score);
        drawText(textBuf, 322.0f, 3.0f, 0.62f, clrWhite, hudStr);

        int totalJudged = count300 + count100 + count50 + countMiss;
        float acc = (totalJudged > 0)
            ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
            : 100.0f;
        std::snprintf(hudStr, sizeof(hudStr), "%.1f%%", acc);
        drawText(textBuf, 340.0f, 24.0f, 0.40f, clr300, hudStr);

        float livePP = calculatePP(
            beatmap.starRating,
            beatmap.diff.ar,
            beatmap.diff.od,
            maxCombo,
            std::max(1, totalJudged),
            count300, count100, count50, countMiss,
            settings.activeMods
        );
        std::snprintf(hudStr, sizeof(hudStr), "%.0fpp", livePP);
        drawText(textBuf, 345.0f, 38.0f, 0.36f, clrPrimary, hudStr);

        if (settings.activeMods != MOD_NONE) {
            std::string mTag = formatModsString(settings.activeMods);
            drawText(textBuf, 330.0f, 52.0f, 0.34f, clrCursor, mTag.c_str());
        }

        std::snprintf(hudStr, sizeof(hudStr), "%dx", combo);
        drawText(textBuf, 5.0f, 215.0f, 0.6f, clrCursor, hudStr);

        // Key Overlay
        C2D_DrawRectSolid(368.0f, 95.0f, 0.3f, 24.0f, 22.0f, k1Held ? clrKeyActive : clrKeyIdle);
        C2D_DrawRectSolid(368.0f, 123.0f, 0.3f, 24.0f, 22.0f, k2Held ? clrKeyActive : clrKeyIdle);

        // Estela del Cursor (Cursor Trail) y Cursor
        for (size_t t = 0; t < cursorTrail.size(); ++t) {
            float ratio = static_cast<float>(cursorTrail[t].life) / 12.0f;
            u8 tAlpha = static_cast<u8>(115.0f * ratio);
            u32 tClr = C2D_Color32(
                activeSkin.cursorTrailColor.r,
                activeSkin.cursorTrailColor.g,
                activeSkin.cursorTrailColor.b,
                tAlpha
            );
            float tr = (3.8f * ratio) * settings.cursorScale;
            C2D_DrawCircleSolid(osuToTopX(cursorTrail[t].x), osuToTopY(cursorTrail[t].y), 0.86f, tr, tClr);
        }

        float expandFactor = (activeSkin.cursorExpand && anyHeld) ? 7.5f : 5.5f;
        float cursorRadius = expandFactor * settings.cursorScale;
        if (texSkinCursor.loaded) {
            texSkinCursor.drawCenteredSprite(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), cursorRadius * 1.4f, 0.88f, C2D_Color32(255, 255, 255, 255));
        } else {
            C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.88f, cursorRadius, clrCursor);
            C2D_DrawCircleSolid(osuToTopX(cursorOsuX), osuToTopY(cursorOsuY), 0.89f, 2.0f * settings.cursorScale, clrWhite);
        }

        // Overlay de Pausa en pantalla superior (Z-depth 0.92 para el fondo y 0.99 para el texto encima!)
        if (isPaused) {
            C2D_DrawRectSolid(75.0f, 40.0f, 0.92f, 250.0f, 160.0f, C2D_Color32(18, 16, 28, 245));
            drawRectOutline(75.0f, 40.0f, 250.0f, 160.0f, 2.0f, clrPrimary, 0.95f);
            drawCenteredText(textBuf, 200.0f, 62.0f, 0.62f, clrCursor, "JUEGO EN PAUSA", 0.99f);

            const char* pOpts[3] = { "Continuar", "Reintentar Beatmap", "Volver a Canciones" };
            for (int i = 0; i < 3; ++i) {
                bool sel = (pauseOption == i);
                float py = 98.0f + i * 28.0f;
                if (sel) {
                    C2D_DrawRectSolid(95.0f, py - 10.0f, 0.95f, 210.0f, 22.0f, C2D_Color32(55, 45, 80, 255));
                }
                u32 c = sel ? clr300 : clrWhite;
                drawCenteredText(textBuf, 200.0f, py, 0.50f, c, pOpts[i], 0.99f);
            }
        }

        // --- PANTALLA INFERIOR: SUPERFICIE DE TABLET ACTIVA O BOTONES TÁCTILES DE PAUSA ---
        C2D_TargetClear(bottomScreen, clrTabletBg);
        C2D_SceneBegin(bottomScreen);

        if (isPaused) {
            const char* pOpts[3] = { "CONTINUAR", "REINTENTAR", "VOLVER A CANCIONES" };
            for (int i = 0; i < 3; ++i) {
                bool sel = (pauseOption == i);
                float bx = 40.0f;
                float by = 40.0f + i * 58.0f;
                float bw = 240.0f;
                float bh = 44.0f;

                u32 bg = sel ? clrPrimary : clrPanelBg;
                C2D_DrawRectSolid(bx, by, 0.2f, bw, bh, bg);
                drawRectOutline(bx, by, bw, bh, sel ? 2.0f : 1.0f, sel ? clrWhite : clrMuted, 0.3f);
                drawCenteredText(textBuf, bx + bw * 0.5f, by + bh * 0.5f, 0.52f, clrWhite, pOpts[i], 0.85f);
            }
        } else {
            float rx, ry, rw, rh;
            settings.getTabletRect(rx, ry, rw, rh);

            C2D_DrawRectSolid(rx, ry, 0.1f, rw, rh, C2D_Color32(28, 25, 40, 255));
            drawRectOutline(rx, ry, rw, rh, 1.5f, C2D_Color32(85, 75, 120, 200));
            drawRing(settings.tabletCenterX, settings.tabletCenterY, 12.0f, 1.0f, C2D_Color32(55, 50, 75, 255), 16);

            if (isTouching) {
                C2D_DrawCircleSolid(touch.px, touch.py, 0.5f, 4.0f, clrKeyActive);
            }
        }

        C3D_FrameEnd(0);
    }

    bgTex.unload();
    texSkinCursor.unload();
    texSkinCircle.unload();
    texSkinOverlay.unload();
    texSkinApproach.unload();
    settings.save();
    audio.exitAll();
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
    return 0;
}
