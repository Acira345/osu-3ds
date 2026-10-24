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
#include "VideoPlayer.hpp"
#include "UiKit.hpp"
#include "qrcodegen.h"
#include "DirectDownloader.hpp"
#include "ModeTaiko.hpp"
#include "ModeMania.hpp"
#include "ModeCatch.hpp"
#include "NetCommon.hpp"
#include "MultiplayerManager.hpp"

// Aumentar pila del hilo principal en la 3DS a 256 KB (por defecto libctru asigna solo 32 KB)
extern "C" {
    u32 __stacksize__ = 0x40000;
}

// Playfield seguro centrado con márgenes para que ningún círculo ni slider se salga de la pantalla (Safe Area)
constexpr float SCALE_FACTOR = 0.5208333f; // 200.0f / 384.0f
constexpr float PLAYFIELD_W  = 266.6667f;  // 512.0f * SCALE_FACTOR
constexpr float PLAYFIELD_H  = 200.0f;     // 384.0f * SCALE_FACTOR

constexpr float PLAYFIELD_BOT_OFFSET_X = 26.6667f; // (320.0f - PLAYFIELD_W) * 0.5f
constexpr float PLAYFIELD_BOT_OFFSET_Y = 20.0f;    // (240.0f - PLAYFIELD_H) * 0.5f

constexpr float PLAYFIELD_TOP_OFFSET_X = 66.6667f; // (400.0f - PLAYFIELD_W) * 0.5f
constexpr float PLAYFIELD_TOP_OFFSET_Y = 20.0f;    // (240.0f - PLAYFIELD_H) * 0.5f

inline float osuToTopX(float osuX) { return PLAYFIELD_TOP_OFFSET_X + (osuX * SCALE_FACTOR); }
inline float osuToTopY(float osuY) { return PLAYFIELD_TOP_OFFSET_Y + (osuY * SCALE_FACTOR); }

inline u32 getStarRatingColor(float stars) {
    if (stars < 2.0f) return C2D_Color32(100, 240, 120, 255); // Easy (Verde)
    if (stars < 2.7f) return C2D_Color32(70, 220, 255, 255);  // Normal (Cian)
    if (stars < 4.0f) return C2D_Color32(255, 224, 85, 255);  // Hard (Oro)
    if (stars < 5.3f) return C2D_Color32(255, 100, 170, 255); // Insane (Rosa)
    return C2D_Color32(205, 110, 255, 255);                   // Expert+ (Violeta)
}

static void renderQrCode(const char* text, float startX, float startY, float maxBoxSize) {
    if (!text || text[0] == '\0') return;
    uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if (!qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_LOW, 1, 10, qrcodegen_Mask_AUTO, true)) {
        return;
    }
    int qrSize = qrcodegen_getSize(qrcode);
    int border = 2;
    int totalModules = qrSize + border * 2;
    float modSize = std::floor(maxBoxSize / static_cast<float>(totalModules));
    if (modSize < 1.0f) modSize = 1.0f;
    float actualSize = modSize * static_cast<float>(totalModules);
    float ox = startX + std::floor((maxBoxSize - actualSize) * 0.5f);
    float oy = startY + std::floor((maxBoxSize - actualSize) * 0.5f);

    // Fondo blanco del QR
    C2D_DrawRectSolid(ox, oy, 0.28f, actualSize, actualSize, C2D_Color32(255, 255, 255, 255));

    u32 clrBlack = C2D_Color32(18, 16, 26, 255);
    for (int y = 0; y < qrSize; ++y) {
        for (int x = 0; x < qrSize; ++x) {
            if (qrcodegen_getModule(qrcode, x, y)) {
                C2D_DrawRectSolid(ox + (x + border) * modSize,
                                  oy + (y + border) * modSize,
                                  0.30f, modSize, modSize, clrBlack);
            }
        }
    }
}

static void renderWifiBars(float x, float y, u8 strength, u32 clrActive, u32 clrInactive) {
    for (int i = 0; i < 3; ++i) {
        float barH = 5.0f + i * 4.0f;
        float barY = y + (13.0f - barH);
        u32 c = (strength > i) ? clrActive : clrInactive;
        C2D_DrawRectSolid(x + i * 6.0f, barY, 0.35f, 4.0f, barH, c);
    }
}

enum AppState {
    STATE_MAIN_MENU,
    STATE_WIFI_TRANSFER,
    STATE_SETTINGS,
    STATE_SONG_SELECT,
    STATE_PLAYING,
    STATE_RESULTS,
    STATE_MULTI_BROWSER,
    STATE_MULTI_LOBBY,
    STATE_MODS_SELECT,
    STATE_MULTI_RESULTS,
    STATE_MULTI_SYNC
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
    int maxLife;
};

struct SmokeParticle {
    float x, y;
    float vx, vy;
    float size;
    int life;
    int maxLife;
};

struct HitErrorTick {
    float errMs;
    HitState state;
    int life;
};

struct GameplayParticle {
    float x, y;
    float vx, vy;
    float size;
    u32 color;
    int life, maxLife;
};

void drawRingZ(float x, float y, float z, float radius, float thickness, u32 color, int segments = 24) {
    if (radius < 0.5f) return;
    int segs = std::max(12, std::min(segments, 24));
    const float step = (2.0f * static_cast<float>(M_PI)) / segs;
    float prevX = x + radius;
    float prevY = y;
    for (int i = 1; i <= segs; ++i) {
        float a = i * step;
        float curX = x + std::cos(a) * radius;
        float curY = y + std::sin(a) * radius;
        C2D_DrawLine(prevX, prevY, color, curX, curY, color, thickness, z);
        prevX = curX;
        prevY = curY;
    }
}

void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24, float z = 0.5f) {
    drawRingZ(x, y, z, radius, thickness, color, segments);
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
    float rx = std::round(x);
    float ry = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, rx, ry, z, scale, scale, color);
}

void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &w, &h);
    float rx = std::round(cx - (w * 0.5f));
    float ry = std::round(cy - (h * 0.5f));
    C2D_DrawText(&text, C2D_WithColor, rx, ry, z, scale, scale, color);
}

void drawTextWithShadow(C2D_TextBuf buf, float x, float y, float scale, u32 color, const char* str, float z = 0.85f, u32 shadowColor = Ui::rgba(0, 0, 0, 220)) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    float rx = std::round(x);
    float ry = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, rx + 1.0f, ry + 1.0f, z - 0.001f, scale, scale, shadowColor);
    C2D_DrawText(&text, C2D_WithColor, rx, ry, z, scale, scale, color);
}

void drawCenteredTextWithShadow(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f, u32 shadowColor = Ui::rgba(0, 0, 0, 220)) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &w, &h);
    float rx = std::round(cx - (w * 0.5f));
    float ry = std::round(cy - (h * 0.5f));
    C2D_DrawText(&text, C2D_WithColor, rx + 1.0f, ry + 1.0f, z - 0.001f, scale, scale, shadowColor);
    C2D_DrawText(&text, C2D_WithColor, rx, ry, z, scale, scale, color);
}

// Dibuja una insignia de control estilo consola (Pastilla de color con letra + texto en alto contraste)
void drawKeyBadge(C2D_TextBuf buf, float x, float y, const char* key, const char* label, u32 keyClr, u32 textClr = Ui::rgba(255, 255, 255), float z = 0.85f) {
    float rx = std::round(x);
    float ry = std::round(y);
    int len = key ? static_cast<int>(std::strlen(key)) : 1;
    float kw = (len <= 1) ? 15.0f : std::round(8.0f + len * 6.0f);
    Ui::pill(rx, ry - 1.0f, kw, 15.0f, z - 0.005f, keyClr);
    drawCenteredText(buf, rx + kw * 0.5f, ry + 6.5f, (len > 3) ? 0.25f : 0.30f, Ui::rgba(0, 0, 0, 245), key, z);
    drawTextWithShadow(buf, rx + kw + 4.0f, ry, 0.34f, textClr, label, z);
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

inline std::string formatScoreThousands(int score) {
    std::string s = std::to_string(score);
    int n = static_cast<int>(s.length()) - 3;
    while (n > 0) {
        s.insert(n, ",");
        n -= 3;
    }
    return s;
}

inline u32 getScoreGradeColor(const std::string& grade, u32 cursorClr) {
    if (grade == "SS" || grade == "SSH") return Ui::rgba(255, 102, 171);
    if (grade == "S" || grade == "SH") return cursorClr;
    if (grade == "A") return Ui::rgba(110, 235, 110);
    if (grade == "B") return Ui::rgba(100, 180, 255);
    if (grade == "C") return Ui::rgba(195, 120, 255);
    return Ui::rgba(255, 80, 80);
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

struct ModDef {
    const char* code;
    const char* shortName;
    const char* fullName;
    const char* desc;
    uint32_t bit;
    u32 color;
};

static const ModDef modTable[15] = {
    // Fila 1: Reducción y Dificultad Clásica
    { "EZ", "Facil",     "Easy",         "Circulos mas grandes y velocidad reducida.",               MOD_EZ,   Ui::rgba(60, 185, 105) },
    { "NF", "NoFail",    "No Fail",      "No puedes perder aunque se agote la barra de vida.",       MOD_NF,   Ui::rgba(50, 165, 120) },
    { "HT", "Lento",     "Half Time",    "Velocidad de musica y notas reducida al 75% (0.75x).",     MOD_HT,   Ui::rgba(45, 150, 140) },
    { "HR", "Dificil",   "Hard Rock",    "Circulos mas pequenos, AR/OD alto y mapa invertido.",      MOD_HR,   Ui::rgba(240, 70, 70)  },
    { "SD", "Muerte",    "Sudden Death", "Falla una sola nota y perderas la partida al instante.",   MOD_SD,   Ui::rgba(255, 90, 50)  },

    // Fila 2: Velocidad y Desafío Extremo
    { "PF", "Perfect",   "Perfect",      "Solo calificaciones 300 permitidas. Un 100 y mueres.",     MOD_PF,   Ui::rgba(255, 180, 40) },
    { "DT", "DobleVel",  "Double Time",  "Velocidad de musica y notas aumentada al 150% (1.5x).",    MOD_DT,   Ui::rgba(180, 70, 240) },
    { "NC", "Nightcore", "Nightcore",    "Velocidad 1.5x con ritmo dance en los compases.",          MOD_NC,   Ui::rgba(220, 80, 255) },
    { "HD", "Oculto",    "Hidden",       "Las notas se desvanecen antes de tocar y no hay anillos.", MOD_HD,   Ui::rgba(255, 102, 171)},
    { "FL", "Linterna",  "Flashlight",   "Toda la pantalla a oscuras salvo un foco sobre el stylus.",MOD_FL,   Ui::rgba(255, 215, 60) },

    // Fila 3: Automatización, Estilo y Asistencia
    { "RX", "Relax",     "Relax",        "Solo apunta el stylus; el juego toca las notas por ti.",   MOD_RX,   Ui::rgba(0, 216, 240)  },
    { "AP", "AutoPilot", "Auto Pilot",   "Solo sigue el ritmo; el cursor apunta automaticamente.",   MOD_AP,   Ui::rgba(80, 180, 255) },
    { "SO", "SinSpin",   "Spun Out",     "Los spinners se giran automaticamente para no cansar.",    MOD_SO,   Ui::rgba(130, 220, 110)},
    { "MR", "Espejo",    "Mirror",       "Refleja todas las notas y sliders horizontalmente (X).",   MOD_MR,   Ui::rgba(255, 145, 185)},
    { "AT", "AutoBot",   "Autoplay",     "Demostracion perfecta por inteligencia artificial.",       MOD_AUTO, Ui::rgba(100, 110, 255)}
};

int main(int argc, char* argv[]) {
    osSetSpeedupEnable(true); // Activar 804 MHz + Cache L2 en consolas New Nintendo 3DS / New 2DS XL
    gfxInitDefault();
    gfxSet3D(false);          // Se activa dinamicamente solo cuando el slider 3D fisico > 0
    romfsInit();
    acInit();                 // Iniciar servicio de acceso a red para mantener activa la conexion Wi-Fi
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 4);
    C2D_Prepare();

    C3D_RenderTarget* topScreen      = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* topScreenRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    C3D_RenderTarget* bottomScreen   = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf textBuf = C2D_TextBufNew(4096);

    u32 clrBgDark     = C2D_Color32(18, 16, 26, 255);
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

    ScoreManager scoreMgr;
    scoreMgr.load();

    SongManager songMgr;
    songMgr.initAndScan(&scoreMgr);

    Beatmap beatmap;
    ModeTaiko taikoEngine;
    ModeMania maniaEngine;
    ModeCatch catchEngine;
    AudioPlayer audio;
    audio.initDSPAndHitsounds();
    audio.setVolumes(settings.musicVolume, settings.sfxVolume);

    WifiServer wifiServer;
    MultiplayerManager multiMgr;
    multiMgr.enableBgDownload = (settings.multiDlOption != 0);

    // Detectar nombre de usuario configurado en la consola 3DS
    std::string sysUser = "Player3DS";
    u16 utf16_user[16] = {0};
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_GetConfigInfoBlk2(0x1C, 0x000A0000, utf16_user);
        cfguExit();
        std::string tmp;
        for (int i = 0; i < 10 && utf16_user[i] != 0; ++i) {
            if (utf16_user[i] >= 32 && utf16_user[i] < 127) tmp.push_back(static_cast<char>(utf16_user[i]));
        }
        if (!tmp.empty()) sysUser = tmp;
    }
    multiMgr.localPlayerName = sysUser;
    bool multiQueuePickerOpen = false;
    int multiQueuePickerIndex = 0;
    int multiQueuePickerDiff = 0;
    enum MultiPickerAction {
        PICKER_ACTION_ACTIVE = 0,
        PICKER_ACTION_QUEUE  = 1
    };
    MultiPickerAction multiPickerAction = PICKER_ACTION_ACTIVE;
    int multiResultsTab = 0;
    bool multiSyncLoaded = false;
    int multiSyncLeadIn = 0;

    // Texturas GPU dinámicas (Fondo de canción y Sprites PNG de Skin personalizado)
    LoadedTexture bgTex;
    LoadedTexture texSkinCursor;
    LoadedTexture texSkinCursorTrail;
    LoadedTexture texSkinCircle;
    LoadedTexture texSkinOverlay;
    LoadedTexture texSkinApproach;
    LoadedTexture texSkinComboBurst;

    // Detectar si la consola es New Nintendo 3DS / New 2DS XL (804 MHz Quad-Core + 256 MB RAM)
    bool isNew3DS = false;
    APT_CheckNew3DS(&isNew3DS);
    const int bgMaxW = isNew3DS ? 400 : 256;
    const int bgMaxH = isNew3DS ? 240 : 192;

    // Decodificador de fondos en hilo secundario (usa nucleo dedicado en New 3DS)
    AsyncTextureLoader bgLoader;
    bgLoader.start(isNew3DS);
    audio.startAsyncLoader(isNew3DS);
    VideoPlayer videoPlayer;
    videoPlayer.init();
    int previewDelay = 0;      // frames de espera antes de cargar audio/fondo (debounce)
    float bgFade = 255.0f;     // opacidad del fondo para fundido suave

    auto applyActiveSkinAudio = [&]() {
        if (skinMgr.skins.empty()) return;
        const auto& sk = skinMgr.skins[settings.skinIndex % skinMgr.skins.size()];
        audio.loadSkinHitsounds(
            sk.hitNormalWav,
            sk.hitWhistleWav,
            sk.hitFinishWav,
            sk.hitClapWav,
            sk.comboBreakWav,
            sk.synthPreset,
            sk.nightcoreKickWav,
            sk.nightcoreClapWav,
            sk.nightcoreHatWav
        );

        if (!sk.cursorPng.empty())           texSkinCursor.loadFromFile(sk.cursorPng, 64, 64);
        else                                 texSkinCursor.unload();

        if (!sk.cursorTrailPng.empty())      texSkinCursorTrail.loadFromFile(sk.cursorTrailPng, 64, 64);
        else                                 texSkinCursorTrail.unload();

        if (!sk.hitCirclePng.empty())        texSkinCircle.loadFromFile(sk.hitCirclePng, 128, 128);
        else                                 texSkinCircle.unload();

        if (!sk.hitCircleOverlayPng.empty()) texSkinOverlay.loadFromFile(sk.hitCircleOverlayPng, 128, 128);
        else                                 texSkinOverlay.unload();

        if (!sk.approachCirclePng.empty())   texSkinApproach.loadFromFile(sk.approachCirclePng, 128, 128);
        else                                 texSkinApproach.unload();

        if (!sk.comboBurstPng.empty())       texSkinComboBurst.loadFromFile(sk.comboBurstPng, 160, 240);
        else                                 texSkinComboBurst.unload();
    };
    applyActiveSkinAudio();

    AppState appState = STATE_MAIN_MENU;
    AppState settingsReturnState = STATE_MAIN_MENU;
    AppState wifiReturnState = STATE_MAIN_MENU;

    int mainMenuOption = 0; // 0: Jugar, 1: Descargar Wi-Fi, 2: Configuraciones, 3: Salir
    int settingsOption = 0; // 0..20 (5 pestañas)
    int capturingKeyIndex = -1; // -1: inactivo, 4..7: capturando tecla física
    int settingsControlsMode = MODE_STD; // Modo activo en la pestaña de controles (0: STD, 1: TAIKO, 2: CATCH, 3: MANIA)
    float taikoGuideTimer = 0.0f; // Temporizador para mostrar guía de controles en pantalla (4 segundos)
    int selectedSet = 0;
    int selectedDiff = 0;

    // Selector de Mods en el menú de canciones
    bool modMenuOpen = false;
    int modCursor = 0; // 0..6 (EZ, NF, HT, HR, DT, HD, AUTO)
    bool deleteConfirmOpen = false;
    // Teclado interactivo táctil QWERTY para búsqueda en vivo
    bool searchKeyboardOpen = false;
    int searchLastKeyIndex = -1;
    int searchKeyFlashTimer = 0;
    int searchBackspaceHoldTimer = 0;
    float footerCardLift[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int footerCardHover = -1;

    // Pestañas y Ranking Local en el menú de canciones
    int songSelectTab = 0; // 0: Resumen (Personal Best + Strain Graph), 1: Ranking Local (Top Récords)
    bool rankingExpanded = false; // true: lista expandida que ocupa toda la parte inferior
    int rankingScroll = 0; // Desplazamiento en la lista si hay muchos récords

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
    float prevCursorOsuX = 256.0f;
    float prevCursorOsuY = 192.0f;
    std::vector<TrailPoint> cursorTrail;
    std::vector<SmokeParticle> smokeParticles;
    u64 gameplayFallbackStartTick = 0;
    int combo = 0;
    int prevCombo = 0;
    int lastBrokenCombo = 0;
    int maxCombo = 0;
    int score = 0;
    int count300 = 0, count100 = 0, count50 = 0, countMiss = 0;
    int k1PressCount = 0, k2PressCount = 0;
    float comboScale = 1.0f;
    float comboBreakAnim = 0.0f;
    std::vector<HitPopup> popups;
    std::vector<GameplayParticle> particles;
    std::vector<HitErrorTick> hitErrorTicks;
    std::vector<float> allHitErrors;
    struct HitJudgementEvent {
        int time;
        HitState state;
        float errMs;
    };
    std::vector<HitJudgementEvent> judgementHistory;
    float hitErrorNeedle = 0.0f;
    float lastUnstableRate = 0.0f;
    float lastMeanHitError = 0.0f;
    float spinnerRPM = 0.0f;
    float spinnerClearAnim = 0.0f;
    float spinnerBonusAnim = 0.0f;
    int lastSpinnerBonus = 0;
    float shakeTimer = 0.0f;
    float shakeIntensity = 0.0f;
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    int animTick = 0;
    float carouselPos = 0.0f;
    AppState lastAppState = STATE_MAIN_MENU;
    float screenFade = 1.0f;

    // Variables de Skip, Marcador en Vivo, Descansos (Breaks) y Efecto Kiai Time
    std::vector<ScoreRecord> localRecords;
    float overtakeFlashAnim = 0.0f;
    bool hasOvertakenBest = false;
    bool skipAvailable = false;
    int skipTargetMs = 0;
    bool inBreakPeriod = false;
    int breakRemainingMs = 0;
    int breakTotalMs = 0;
    Vec2 breakNextObjPos = {256.0f, 192.0f};
    bool inKiai = false;
    bool prevInKiai = false;
    float kiaiGlow = 0.0f;
    float kiaiBurstFlash = 0.0f;
    int comboBurstTimer = 0;
    int comboBurstSide = 0;
    int comboBurstValue = 0;
    int lastComboBurstMilestone = 0;
    int lastNcStep = -999999;

    auto spawnParticles = [&](float osuX, float osuY, u32 color, int count, float minSpd, float maxSpd, float baseSize, int maxLifeFrames) {
        for (int p = 0; p < count; ++p) {
            if (particles.size() >= 64) particles.erase(particles.begin());
            float ang = static_cast<float>(std::rand() % 628) * 0.01f;
            float spd = minSpd + (static_cast<float>(std::rand() % 100) / 100.0f) * (maxSpd - minSpd);
            int lf = std::max(6, maxLifeFrames - (std::rand() % 5));
            particles.push_back({
                osuX * SCALE_FACTOR, osuY * SCALE_FACTOR,
                std::cos(ang) * spd, std::sin(ang) * spd,
                baseSize, color, lf, lf
            });
        }
    };

    auto checkComboBurst = [&](int curCombo) {
        if (!settings.comboBursts) return;
        bool isMilestone = false;
        if (curCombo == 30 || curCombo == 60) isMilestone = true;
        else if (curCombo >= 100 && (curCombo % 100 == 0 || curCombo % 50 == 0)) isMilestone = true;

        if (isMilestone && curCombo > lastComboBurstMilestone) {
            lastComboBurstMilestone = curCombo;
            comboBurstTimer = 48;
            comboBurstSide = (comboBurstSide == 0) ? 1 : 0;
            comboBurstValue = curCombo;
            float burstEdgeX = (comboBurstSide == 0) ? 460.0f : 50.0f;
            spawnParticles(burstEdgeX, 220.0f, Ui::rgba(255, 102, 171), 10, 1.2f, 3.2f, 2.5f, 24);
            spawnParticles(burstEdgeX, 220.0f, Ui::rgba(255, 215, 60), 8, 1.0f, 2.8f, 2.2f, 22);
        }
    };

    auto triggerShake = [&](float intensity, float frames) {
        if (intensity > shakeIntensity || shakeTimer <= 0.0f) {
            shakeIntensity = intensity;
        }
        if (frames > shakeTimer) {
            shakeTimer = frames;
        }
    };

    auto updateSongPreview = [&]() {
        if (songMgr.sets.empty()) {
            audio.stopMusic();
            bgTex.unload();
            return;
        }
        selectedSet = std::max(0, std::min(selectedSet, static_cast<int>(songMgr.sets.size()) - 1));
        const auto& curSet = songMgr.sets[selectedSet];
        if (curSet.difficulties.empty()) return;
        selectedDiff = std::max(0, std::min(selectedDiff, static_cast<int>(curSet.difficulties.size()) - 1));
        const auto& curDiff = curSet.difficulties[selectedDiff];

        std::string bgPath = !curDiff.backgroundPath.empty() ? curDiff.backgroundPath : curSet.backgroundPath;
        if (!bgPath.empty()) {
            if (!(bgTex.loaded && bgTex.sourcePath == bgPath)) {
                bgLoader.request(bgPath, bgMaxW, bgMaxH);
            }
        } else {
            bgLoader.cancel();
            bgTex.unload();
        }

        if (!settings.songPreview) {
            audio.stopMusic();
            return;
        }
        const std::string& audioPath = curDiff.audioPath;
        float speedRate = getModsSpeedMultiplier(settings.activeMods);
        bool shouldLoop = (appState == STATE_SONG_SELECT);
        int startOff = (curDiff.previewTimeMs > 0) ? curDiff.previewTimeMs : 0;
        if (!audio.getIsPlaying() || audio.isSongFinished() || audio.getLoadedMusicPath() != audioPath) {
            audio.requestPreview(audioPath, speedRate, startOff, shouldLoop);
        } else {
            audio.setLoop(shouldLoop, startOff);
        }
    };

    auto rotateMenuSong = [&]() {
        if (!settings.songPreview) return;
        if (songMgr.sets.empty()) {
            std::string themePath = "romfs:/circles.mp3";
            audio.startMusic(themePath, 0, 1.0f, 0);
            audio.setLoop(false);
            bgLoader.cancel();
            bgTex.unload();
            return;
        }

        int nSets = static_cast<int>(songMgr.sets.size());
        if (nSets > 1) {
            int next = selectedSet;
            while (next == selectedSet) {
                next = std::rand() % nSets;
            }
            selectedSet = next;
        } else {
            selectedSet = 0;
        }
        selectedDiff = 0;
        carouselPos = static_cast<float>(selectedSet);
        updateSongPreview();
    };

    auto getBpmPulse = [&]() -> float {
        float beatLen = 500.0f;
        float offset = 0.0f;
        if (audio.getLoadedMusicPath() == "romfs:/circles.mp3") {
            // Nekodex - Circles (175 BPM)
            beatLen = 60000.0f / 175.0f;
            offset = 0.0f;
        } else if (!songMgr.sets.empty() && selectedSet >= 0 && selectedSet < static_cast<int>(songMgr.sets.size())) {
            const auto& cs = songMgr.sets[selectedSet];
            if (!cs.difficulties.empty()) {
                int dIdx = std::max(0, std::min(selectedDiff, static_cast<int>(cs.difficulties.size()) - 1));
                beatLen = std::max(120.0f, cs.difficulties[dIdx].beatLengthMs / getModsSpeedMultiplier(settings.activeMods));
                offset = cs.difficulties[dIdx].timingOffsetMs;
            }
        }
        float tMs = audio.getIsPlaying()
            ? static_cast<float>(audio.getTimeMs())
            : static_cast<float>(animTick) * (1000.0f / 60.0f);
        float rel = std::fmod(std::max(0.0f, tMs - offset), beatLen) / beatLen;
        return std::pow(1.0f - rel, 3.0f);
    };

    auto toggleSearchTag = [&](const std::string& tag) {
        size_t pos = songMgr.searchQuery.find(tag);
        if (pos != std::string::npos) {
            songMgr.searchQuery.erase(pos, tag.length());
            size_t sp = songMgr.searchQuery.find("  ");
            while (sp != std::string::npos) {
                songMgr.searchQuery.erase(sp, 1);
                sp = songMgr.searchQuery.find("  ");
            }
            songMgr.searchQuery = Beatmap::trim(songMgr.searchQuery);
        } else {
            if (tag.rfind("mode:", 0) == 0) {
                const char* modes[] = {"mode:std", "mode:taiko", "mode:catch", "mode:mania"};
                for (const char* m : modes) {
                    size_t mp = songMgr.searchQuery.find(m);
                    if (mp != std::string::npos) {
                        songMgr.searchQuery.erase(mp, std::strlen(m));
                    }
                }
            }
            if (tag == "★4" || tag == "★5") {
                size_t p4 = songMgr.searchQuery.find("★4");
                if (p4 != std::string::npos) songMgr.searchQuery.erase(p4, 2);
                size_t p5 = songMgr.searchQuery.find("★5");
                if (p5 != std::string::npos) songMgr.searchQuery.erase(p5, 2);
            }
            songMgr.searchQuery = Beatmap::trim(songMgr.searchQuery);
            if (!songMgr.searchQuery.empty()) songMgr.searchQuery += " ";
            songMgr.searchQuery += tag;
        }
        songMgr.applySortAndFilter(&scoreMgr);
        selectedSet = 0;
        selectedDiff = 0;
        rankingScroll = 0;
        carouselPos = 0.0f;
        audio.playHitSound(2);
        updateSongPreview();
    };

    auto openSystemKeyboard = [&]() {
        SwkbdState swkbd;
        char inputBuf[64] = {0};
        std::snprintf(inputBuf, sizeof(inputBuf), "%s", songMgr.searchQuery.c_str());
        swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 32);
        swkbdSetInitialText(&swkbd, inputBuf);
        swkbdSetHintText(&swkbd, LOC(STR_SEARCH_SWKBD_HINT));
        swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, LOC(STR_CLEAR), false);
        swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, LOC(STR_SEARCH), true);
        SwkbdButton btn = swkbdInputText(&swkbd, inputBuf, sizeof(inputBuf));
        if (btn == SWKBD_BUTTON_RIGHT) {
            songMgr.searchQuery = Beatmap::trim(inputBuf);
        } else if (btn == SWKBD_BUTTON_LEFT) {
            songMgr.searchQuery = "";
        }
        songMgr.applySortAndFilter(&scoreMgr);
        selectedSet = 0;
        selectedDiff = 0;
        carouselPos = 0.0f;
        audio.playHitSound(2);
        updateSongPreview();
    };

    auto openSearchKeyboard = [&]() {
        searchKeyboardOpen = true;
        searchLastKeyIndex = -1;
        searchKeyFlashTimer = 0;
        searchBackspaceHoldTimer = 0;
        audio.playHitSound(0);
    };

    auto startSelectedBeatmap = [&]() -> bool {
        if (songMgr.sets.empty()) return false;
        const auto& curSet = songMgr.sets[selectedSet];
        if (curSet.difficulties.empty()) return false;
        const auto& diffEntry = curSet.difficulties[selectedDiff];

        if (!beatmap.loadFromFile(diffEntry.osuPath, false)) return false;
        beatmap.applyMods(settings.activeMods);

        bgLoader.cancel();
        previewDelay = 0;
        if (!beatmap.fullBackgroundPath.empty()) {
            bgTex.loadFromFile(beatmap.fullBackgroundPath, bgMaxW, bgMaxH);
        }
        if (settings.enableVideo && !beatmap.fullVideoPath.empty()) {
            videoPlayer.openVideo(beatmap.fullVideoPath, beatmap.videoOffsetMs);
        } else {
            videoPlayer.closeVideo();
        }

        audio.setVolumes(settings.musicVolume, settings.sfxVolume);
        applyActiveSkinAudio();
        if (settings.beatmapHitsounds && !beatmap.folderPath.empty()) {
            audio.loadBeatmapHitsounds(beatmap.folderPath, beatmap.sampleSetName);
        }
        audio.playHitSound(2);

        bool isTaikoPlay = (settings.gameMode == MODE_TAIKO || diffEntry.mode == 1);
        bool isCatchPlay = (settings.gameMode == MODE_CATCH || diffEntry.mode == 2);
        bool isManiaPlay = (settings.gameMode == MODE_MANIA || diffEntry.mode == 3);
        if (isTaikoPlay) {
            taikoEngine.initFromBeatmap(beatmap);
        } else if (isCatchPlay) {
            catchEngine.initFromBeatmap(beatmap);
        } else if (isManiaPlay) {
            maniaEngine.initFromBeatmap(beatmap);
        }

        int leadIn = std::max(0, beatmap.audioLeadIn);
        if (isTaikoPlay) {
            if (!taikoEngine.notes.empty()) {
                int firstObjTime = taikoEngine.notes.front().time;
                int minStartWait = 1400;
                if (firstObjTime < minStartWait) {
                    leadIn = std::max(leadIn, minStartWait - firstObjTime);
                }
            }
        } else if (isCatchPlay) {
            if (!catchEngine.fruits.empty()) {
                int firstObjTime = catchEngine.fruits.front().time;
                int minStartWait = 1300;
                if (firstObjTime < minStartWait) {
                    leadIn = std::max(leadIn, minStartWait - firstObjTime);
                }
            }
        } else if (isManiaPlay) {
            if (!maniaEngine.notes.empty()) {
                int firstObjTime = maniaEngine.notes.front().time;
                int minStartWait = 1200;
                if (firstObjTime < minStartWait) {
                    leadIn = std::max(leadIn, minStartWait - firstObjTime);
                }
            }
        } else if (!beatmap.circles.empty()) {
            int firstObjTime = beatmap.circles.front().time;
            int minStartWait = static_cast<int>(beatmap.diff.preempt) + 500;
            if (firstObjTime < minStartWait) {
                leadIn = std::max(leadIn, minStartWait - firstObjTime);
            }
        }

        float speedRate = getModsSpeedMultiplier(settings.activeMods);
        audio.startMusic(beatmap.fullAudioPath, leadIn, speedRate, 0);
        gameplayFallbackStartTick = osGetTime() + leadIn;

        combo = 0;
        prevCombo = 0;
        lastBrokenCombo = 0;
        maxCombo = 0;
        score = 0;
        count300 = count100 = count50 = countMiss = 0;
        k1PressCount = k2PressCount = 0;
        comboScale = 1.0f;
        comboBreakAnim = 0.0f;
        hitErrorTicks.clear();
        allHitErrors.clear();
        judgementHistory.clear();
        hitErrorNeedle = 0.0f;
        lastUnstableRate = 0.0f;
        lastMeanHitError = 0.0f;
        spinnerRPM = 0.0f;
        spinnerClearAnim = 0.0f;
        spinnerBonusAnim = 0.0f;
        lastSpinnerBonus = 0;
        shakeTimer = 0.0f;
        shakeIntensity = 0.0f;
        shakeX = 0.0f;
        shakeY = 0.0f;
        hp = 1.0f;
        stageFailed = false;
        newRecordAchieved = false;
        lastEarnedPP = 0.0f;
        isPaused = false;
        pauseOption = 0;
        cursorOsuX = 256.0f;
        cursorOsuY = 192.0f;
        prevCursorOsuX = 256.0f;
        prevCursorOsuY = 192.0f;
        cursorTrail.clear();
        smokeParticles.clear();
        popups.clear();
        particles.clear();
        int curStartMode = isTaikoPlay ? 1 : isCatchPlay ? 2 : isManiaPlay ? 3 : 0;
        localRecords = scoreMgr.getTopRecords(diffEntry.osuPath, 4, curStartMode);
        overtakeFlashAnim = 0.0f;
        hasOvertakenBest = false;
        skipAvailable = false;
        skipTargetMs = 0;
        inBreakPeriod = false;
        breakRemainingMs = 0;
        breakTotalMs = 0;
        breakNextObjPos = !beatmap.circles.empty() ? Vec2(beatmap.circles.front().x, beatmap.circles.front().y) : Vec2(256.0f, 192.0f);
        inKiai = false;
        prevInKiai = false;
        kiaiGlow = 0.0f;
        kiaiBurstFlash = 0.0f;
        comboBurstTimer = 0;
        comboBurstSide = 0;
        comboBurstValue = 0;
        lastComboBurstMilestone = 0;
        lastNcStep = -999999;
        taikoGuideTimer = 240.0f;
        appState = STATE_PLAYING;
        return true;
    };

    auto toggleModByIndex = [&](int idx) {
        if (idx < 0 || idx >= 15) return;
        uint32_t bit = modTable[idx].bit;
        if (multiMgr.inMultiplayer && (bit == MOD_AUTO || bit == MOD_RX || bit == MOD_AP)) {
            // Mods no permitidos en partidas multijugador (Auto, Relax, AutoPilot)
            audio.playHitSound(4);
            return;
        }
        if (settings.activeMods & bit) {
            settings.activeMods &= ~bit;
        } else {
            settings.activeMods |= bit;
            // Incompatibilidades clásicas y de osu!lazer
            if (bit == MOD_EZ) settings.activeMods &= ~MOD_HR;
            if (bit == MOD_HR) settings.activeMods &= ~MOD_EZ;
            if (bit == MOD_HT) settings.activeMods &= ~(MOD_DT | MOD_NC);
            if (bit == MOD_DT) settings.activeMods &= ~(MOD_HT | MOD_NC);
            if (bit == MOD_NC) settings.activeMods &= ~(MOD_HT | MOD_DT);
            if (bit == MOD_SD) settings.activeMods &= ~(MOD_PF | MOD_NF);
            if (bit == MOD_PF) settings.activeMods &= ~(MOD_SD | MOD_NF);
            if (bit == MOD_NF) settings.activeMods &= ~(MOD_SD | MOD_PF);
            if (bit == MOD_AUTO) settings.activeMods &= ~(MOD_RX | MOD_AP);
            if (bit == MOD_RX) settings.activeMods &= ~(MOD_AUTO | MOD_AP);
            if (bit == MOD_AP) settings.activeMods &= ~(MOD_AUTO | MOD_RX);
        }
        if (multiMgr.inMultiplayer) {
            settings.activeMods &= ~(MOD_AUTO | MOD_RX | MOD_AP);
            multiMgr.players[multiMgr.localSlot].activeMods = settings.activeMods;
            if (!multiMgr.isHost()) {
                multiMgr.sendPlayerUpdate();
            } else {
                multiMgr.broadcastLobbyState();
            }
        }
        settings.save();
    };

    // Al arrancar el juego, reproducir siempre en primer lugar la canción emblemática ("Click the circles")
    std::srand(static_cast<unsigned int>(osGetTime()));
    selectedSet = 0;
    selectedDiff = 0;
    carouselPos = 0.0f;

    std::string themePath = "romfs:/circles.mp3";
    bool themeStarted = false;
    FILE* fTheme = std::fopen(themePath.c_str(), "rb");
    if (fTheme) {
        std::fclose(fTheme);
        themeStarted = audio.startMusic(themePath, 0, 1.0f, 0);
        if (themeStarted) {
            audio.setLoop(false);
            bgLoader.cancel();
            bgTex.unload();
        }
    }

    if (!themeStarted && !songMgr.sets.empty()) {
        selectedSet = std::rand() % static_cast<int>(songMgr.sets.size());
        selectedDiff = 0;
        carouselPos = static_cast<float>(selectedSet);
        updateSongPreview();
    }

    bool stereoEnabledState = false;
    int analogNavCooldown = 0;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        u32 kUp   = hidKeysUp();
        circlePosition cpad;
        hidCircleRead(&cpad);
        circlePosition cstick = {0, 0};
        hidCstickRead(&cstick);

        float slider3D = osGet3DSliderState();
        if (stereoEnabledState) {
            if (slider3D < 0.02f) {
                stereoEnabledState = false;
                gfxSet3D(false);
            }
        } else {
            if (slider3D > 0.06f) {
                stereoEnabledState = true;
                gfxSet3D(true);
            }
        }
        bool stereoActive = stereoEnabledState;
        if (analogNavCooldown > 0) analogNavCooldown--;

        touchPosition touch;
        bool isTouching = (kHeld & KEY_TOUCH) != 0;
        bool touchJustDown = (kDown & KEY_TOUCH) != 0;
        bool touchJustUp   = (kUp & KEY_TOUCH) != 0;
        if (isTouching) {
            hidTouchRead(&touch);
        }

        animTick++;
        if (appState != lastAppState) {
            lastAppState = appState;
            screenFade = 1.0f;
        } else {
            screenFade = Ui::approach(screenFade, 0.0f, 0.18f);
        }
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
            audio.update();
            if (bgLoader.poll(bgTex)) bgFade = 0.0f;
            bgFade = Ui::approach(bgFade, 255.0f, 0.10f);
            // Al terminar la canción del menú (sea Click the circles o una descargada),
            // continuar con otra de las canciones descargadas continuamente sin quedarse en silencio
            if (settings.songPreview && !audio.isPreviewLoading() && (audio.isSongFinished() || !audio.getIsPlaying())) {
                rotateMenuSong();
            }
            if (kDown & KEY_START) break;

            if (kDown & KEY_DUP) {
                mainMenuOption = (mainMenuOption + 4) % 5;
                audio.playHitSound(0);
            }
            if (kDown & KEY_DDOWN) {
                mainMenuOption = (mainMenuOption + 1) % 5;
                audio.playHitSound(0);
            }
            // Cambiar canción del menú manualmente con L y R o cruceta Izq/Der
            if (!songMgr.sets.empty() && (kDown & (KEY_L | KEY_R | KEY_DLEFT | KEY_DRIGHT))) {
                int dir = (kDown & (KEY_R | KEY_DRIGHT)) ? 1 : -1;
                int nSets = static_cast<int>(songMgr.sets.size());
                selectedSet = (selectedSet + dir + nSets) % nSets;
                selectedDiff = 0;
                carouselPos = static_cast<float>(selectedSet);
                audio.playHitSound(0);
                updateSongPreview();
            }

            bool activateOption = (kDown & KEY_A) != 0;

            if (touchJustDown) {
                if (touch.px >= 18 && touch.px <= 302) {
                    for (int i = 0; i < 5; ++i) {
                        float by = 12.0f + i * 34.0f;
                        if (touch.py >= by && touch.py <= by + 30.0f) {
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
                    // Abrir Navegador de Salas Multijugador Local (LAN)
                    multiMgr.startBrowser(sysUser);
                    appState = STATE_MULTI_BROWSER;
                } else if (mainMenuOption == 2) {
                    audio.stopMusic();
                    multiMgr.stop();
                    wifiReturnState = STATE_MAIN_MENU;
                    wifiServer.start();
                    appState = STATE_WIFI_TRANSFER;
                } else if (mainMenuOption == 3) {
                    settingsReturnState = STATE_MAIN_MENU;
                    calibrationMode = false;
                    appState = STATE_SETTINGS;
                } else if (mainMenuOption == 4) {
                    break;
                }
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            float beat = getBpmPulse();
            const char* menuLabels[5] = {
                LOC(STR_MENU_PLAY),
                LOC(STR_MENU_MULTI),
                LOC(STR_MENU_WIFI),
                LOC(STR_MENU_SETTINGS),
                LOC(STR_MENU_EXIT)
            };
            const char* menuSubPills[5] = {
                LOC(STR_MENU_PILL_SOLO),
                LOC(STR_MENU_PILL_MULTI),
                LOC(STR_MENU_PILL_WIFI),
                LOC(STR_MENU_PILL_SETTINGS),
                LOC(STR_MENU_PILL_EXIT)
            };
            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                float dBg = depthShift(-3.5f);
                if (bgTex.loaded) {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(34, 14, 56), Ui::rgba(10, 7, 22), clrPrimary, 0.01f);
                    bgTex.drawRect(dBg - beat * 2.0f, -beat * 1.5f, 400.0f + beat * 4.0f, 240.0f + beat * 3.0f, 0.02f, static_cast<u8>(bgFade));
                    u8 dimAlpha = static_cast<u8>((settings.bgDim * 180) / 100);
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 10, 24, dimAlpha));
                } else {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(34, 14, 56), Ui::rgba(10, 7, 22), clrPrimary, 0.02f);
                }

                // Barra superior de canción en reproducción (Estilo osu! PC / Multijugador)
                if (!songMgr.sets.empty() && selectedSet >= 0 && selectedSet < static_cast<int>(songMgr.sets.size())) {
                    const auto& curSet = songMgr.sets[selectedSet];
                    Ui::gradV(0.0f, 0.0f, 400.0f, 26.0f, 0.28f, Ui::rgba(14, 10, 24, 240), Ui::rgba(14, 10, 24, 60));

                    // Pastilla indicadora de música estilo multijugador (sin signos de interrogación)
                    Ui::pill(10.0f, 5.0f, 56.0f, 16.0f, 0.32f, Ui::rgba(255, 102, 171));
                    drawCenteredText(textBuf, 38.0f, 12.0f, 0.28f, clrWhite, LOC(STR_MENU_MUSIC_NOW), 0.35f);

                    std::string nowPlaying = curSet.artist + " - " + curSet.title;
                    drawTextWithShadow(textBuf, 72.0f, 5.0f, 0.38f, clrWhite, truncateString(nowPlaying, 34).c_str(), 0.35f);

                    Ui::pill(322.0f, 5.0f, 70.0f, 16.0f, 0.32f, Ui::rgba(30, 24, 46, 230));
                    drawCenteredText(textBuf, 357.0f, 12.0f, 0.29f, isNew3DS ? clr100 : clrMuted, isNew3DS ? "New3DS 804M" : "3DS 268M", 0.35f);
                }

                // Logo que late sincronizado al BPM real y flota hacia adelante en 3D
                float dLogo = depthShift(3.5f + beat * 1.2f);
                float logoX = 118.0f + dLogo;
                float logoY = 126.0f;
                float logoR = 55.0f + beat * 8.0f;

                for (int k = 0; k < 3; ++k) {
                    float phase = std::fmod(animTick * 0.012f + k / 3.0f, 1.0f);
                    Ui::ring(logoX, logoY, 0.15f, logoR + phase * 60.0f, 2.0f, Ui::withAlpha(clrPrimary, static_cast<u8>((1.0f - phase) * 95.0f)), 40);
                }
                Ui::glow(logoX, logoY, 0.16f, logoR * 1.9f, Ui::rgba(255, 102, 170, static_cast<u8>(115 + beat * 45)), 6);
                C2D_DrawCircleSolid(logoX + 3.0f, logoY + 5.0f, 0.18f, logoR, Ui::rgba(0, 0, 0, 120));
                C2D_DrawCircleSolid(logoX, logoY, 0.2f, logoR, Ui::rgba(255, 98, 168, 255));
                C2D_DrawCircleSolid(logoX - logoR * 0.22f, logoY - logoR * 0.28f, 0.21f, logoR * 0.6f, Ui::rgba(255, 255, 255, 30));
                Ui::ring(logoX, logoY, 0.25f, logoR, 3.5f, clrWhite, 40);
                drawCenteredText(textBuf, logoX, logoY - 6.0f, 1.10f + beat * 0.06f, clrWhite, "osu!");
                drawCenteredText(textBuf, logoX, logoY + 24.0f, 0.46f, Ui::rgba(255, 235, 245), "3DS Edition");

                for (int i = 0; i < 5; ++i) {
                    bool sel = (mainMenuOption == i);
                    float dBtn = depthShift(sel ? 4.2f : 1.5f);
                    float bx = (sel ? (202.0f - beat * 3.5f) : 218.0f) + dBtn;
                    float by = 28.0f + i * 36.0f;
                    float bw = 178.0f;
                    float bh = 30.0f;

                    const RGBColor& c = activeSkin.comboColors[i % activeSkin.comboColors.size()];
                    u32 base = C2D_Color32(c.r, c.g, c.b, 255);
                    u32 bg = sel ? Ui::rgba(42, 34, 70, 250) : Ui::rgba(22, 19, 36, 235);
                    Ui::button(bx, by, bw, bh, 0.20f, bg, base, sel);

                    // Pastilla indicadora de selección [>] o [A]
                    Ui::pill(bx + 8.0f, by + 6.0f, 20.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(38, 32, 55, 240));
                    drawCenteredText(textBuf, bx + 18.0f, by + 14.0f, 0.28f, sel ? Ui::rgba(0, 0, 0, 245) : clrWhite, sel ? "A" : ">", 0.28f);

                    // Título de la opción del menú
                    drawTextWithShadow(textBuf, bx + 34.0f, by + 6.5f, 0.38f, clrWhite, menuLabels[i], 0.28f);

                    // Pastilla de categoría / modo a la derecha estilo multijugador
                    Ui::pill(bx + bw - 58.0f, by + 6.0f, 52.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(32, 26, 48, 220));
                    drawCenteredText(textBuf, bx + bw - 32.0f, by + 14.0f, 0.26f, sel ? Ui::rgba(0, 0, 0, 245) : Ui::rgba(215, 210, 235), menuSubPills[i], 0.28f);
                }

                // Barra inferior de atajos estilo multijugador con v4.0 3D
                Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(14, 11, 26, 140), Ui::rgba(14, 11, 26, 250));
                float byY = 219.0f;
                drawKeyBadge(textBuf, 12.0f, byY, "A", LOC(STR_SELECT_ACTION), Ui::rgba(50, 165, 255));
                drawKeyBadge(textBuf, 88.0f, byY, "DPAD", LOC(STR_MOVE_ACTION), Ui::rgba(255, 180, 40));
                drawKeyBadge(textBuf, 164.0f, byY, "TOUCH", LOC(STR_TOUCH_ACTION), Ui::rgba(50, 215, 115));

                Ui::pill(328.0f, byY - 1.0f, 62.0f, 16.0f, 0.30f, Ui::rgba(25, 40, 65, 230));
                drawCenteredText(textBuf, 359.0f, byY + 6.5f, 0.30f, Ui::rgba(100, 215, 255), "v4.0 3D", 0.32f);
                Ui::drawTransition(400.0f, 240.0f, screenFade, clrPrimary);
            }

            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), clrPrimary, 0.02f);
            if (bgTex.loaded) {
                bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.03f, 75);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 320.0f, 240.0f, Ui::rgba(14, 11, 24, 160));
            }

            for (int i = 0; i < 5; ++i) {
                bool sel = (mainMenuOption == i);
                float bx = 20.0f;
                float by = 12.0f + i * 34.0f;
                float bw = 280.0f;
                float bh = 30.0f;

                const RGBColor& c = activeSkin.comboColors[i % activeSkin.comboColors.size()];
                u32 base = C2D_Color32(c.r, c.g, c.b, 255);
                u32 bg = sel ? Ui::rgba(45, 36, 72, 250) : Ui::rgba(22, 19, 36, 235);
                Ui::button(bx, by, bw, bh, 0.20f, bg, base, sel);

                // Pastilla indicadora de atajo [A]
                Ui::pill(bx + 8.0f, by + 6.0f, 22.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(38, 32, 55, 240));
                drawCenteredText(textBuf, bx + 19.0f, by + 14.5f, 0.28f, sel ? Ui::rgba(0, 0, 0, 245) : clrWhite, "A", 0.28f);

                // Titulo de la opcion del menu con sombra nitida
                drawTextWithShadow(textBuf, bx + 36.0f, by + 6.0f, 0.42f, clrWhite, menuLabels[i], 0.28f);

                // Pastilla de categoria / modo a la derecha estilo multijugador
                Ui::pill(bx + bw - 74.0f, by + 6.0f, 66.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(32, 26, 48, 220));
                drawCenteredText(textBuf, bx + bw - 41.0f, by + 14.5f, 0.27f, sel ? Ui::rgba(0, 0, 0, 245) : Ui::rgba(215, 210, 235), menuSubPills[i], 0.28f);
            }

            Ui::panel(10.0f, 186.0f, 300.0f, 46.0f, 0.20f, Ui::rgba(20, 18, 34, 240), clr300);
            char summary1[96];
            std::snprintf(summary1, sizeof(summary1), "%s: %s  •  Area: %d%%  •  Offset: %+dms",
                LOC(STR_MODE),
                settings.getShortInputModeLabel(),
                static_cast<int>(std::round(settings.tabletScale * 100.0f)),
                settings.audioOffsetMs);
            drawCenteredTextWithShadow(textBuf, 160.0f, 199.0f, 0.35f, clr300, summary1, 0.25f);

            std::string skinLine = "Skin: " + truncateString(activeSkin.name, 22) + "  •  Mods: " + formatModsString(settings.activeMods);
            drawCenteredTextWithShadow(textBuf, 160.0f, 217.0f, 0.35f, clrCursor, skinLine.c_str(), 0.25f);
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);

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
                songMgr.initAndScan(&scoreMgr);
                skinMgr.initAndScan();
                selectedSet = 0;
                selectedDiff = 0;
                audio.playHitSound(2);
            }

            bool exitWifi = (kDown & KEY_B) != 0;
            if (touchJustDown && touch.px >= 166 && touch.px <= 306 && touch.py >= 180 && touch.py <= 228) {
                exitWifi = true;
            }

            bool triggerRescan = (kDown & KEY_X) != 0;
            if (touchJustDown && touch.px >= 14 && touch.px <= 154 && touch.py >= 180 && touch.py <= 228) {
                triggerRescan = true;
            }

            if (triggerRescan) {
                audio.playHitSound(2);
                wifiServer.statusText = LOC(STR_WIFI_UPDATING_LIB);
                OszExtractor::extractAllPendingOsz("/osu/songs", "/osu/songs");
                OszExtractor::extractAllPendingOsk("/osu/skins", "/osu/skins");
                songMgr.initAndScan(&scoreMgr);
                skinMgr.initAndScan();
                selectedSet = 0;
                selectedDiff = 0;
                char rescanMsg[96];
                std::snprintf(rescanMsg, sizeof(rescanMsg), LOC(STR_WIFI_LIB_UPDATED), static_cast<int>(songMgr.sets.size()));
                wifiServer.statusText = rescanMsg;
                if (!wifiServer.isRunning()) {
                    wifiServer.restart();
                }
            }

            if (exitWifi) {
                wifiServer.stop();
                songMgr.initAndScan(&scoreMgr);
                skinMgr.initAndScan();
                audio.playHitSound(0);
                appState = wifiReturnState;
                if (appState == STATE_SONG_SELECT) updateSongPreview();
                continue;
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: URL DEL SERVIDOR Y PROGRESO EN VIVO ---
            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                float dCard = eyeSign * slider3D * 2.5f;

                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);

                Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(28, 18, 52), Ui::rgba(10, 8, 22), clr300, 0.02f);

                Ui::gradV(0.0f, 0.0f, 400.0f, 32.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 160));
                C2D_DrawRectSolid(0.0f, 31.0f, 0.31f, 400.0f, 1.5f, clr300);
                drawText(textBuf, 12.0f, 7.0f, 0.50f, clrWhite, LOC(STR_WIFI_HEADER));

                Ui::panel(16.0f + dCard, 42.0f, 368.0f, 66.0f, 0.2f, Ui::rgba(26, 22, 42, 235), clrPrimary);
                drawCenteredText(textBuf, 200.0f + dCard, 56.0f, 0.42f, clrWhite,
                    LOC(STR_WIFI_BROWSER_PROMPT));
                drawCenteredText(textBuf, 200.0f + dCard, 84.0f, 0.72f, clrCursor,
                    wifiServer.serverUrl.empty() ? (wifiServer.isConnecting() ? LOC(STR_WIFI_IP_CONNECTING) : LOC(STR_WIFI_IP_NONE)) : wifiServer.serverUrl.c_str());

                Ui::panel(16.0f + dCard * 0.6f, 114.0f, 368.0f, 42.0f, 0.2f, Ui::rgba(22, 19, 36, 220));
                drawText(textBuf, 24.0f + dCard * 0.6f, 118.0f, 0.40f, clrWhite,
                    LOC(STR_WIFI_STEP1));
                drawText(textBuf, 24.0f + dCard * 0.6f, 136.0f, 0.40f, clr100,
                    LOC(STR_WIFI_STEP2));

                // Barra de progreso de recepción en tiempo real
                Ui::panel(16.0f + dCard, 162.0f, 368.0f, 42.0f, 0.2f, Ui::rgba(26, 22, 42, 235), clr300);
                drawCenteredText(textBuf, 200.0f + dCard, 174.0f, 0.41f, clr300,
                    truncateString(wifiServer.statusText, 50).c_str());

                float prog = wifiServer.getProgressRatio();
                Ui::statBar(30.0f + dCard, 189.0f, 340.0f, 8.0f, 0.35f, prog, clr100);

                Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
                char footerWifi[96];
                std::snprintf(footerWifi, sizeof(footerWifi),
                    LOC(STR_WIFI_FOOTER_FMT),
                    wifiServer.uploadedCount, static_cast<int>(songMgr.sets.size()));
                drawCenteredText(textBuf, 200.0f, 226.0f, 0.40f, clrWhite, footerWifi);
                Ui::drawTransition(400.0f, 240.0f, screenFade, clr300);
            }

            // --- PANTALLA INFERIOR: CÓDIGO QR, SEÑAL Y BOTONES INTERACTIVOS ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), clr300, 0.02f);

            // Cabecera superior
            Ui::gradV(0.0f, 0.0f, 320.0f, 26.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 160));
            C2D_DrawRectSolid(0.0f, 25.0f, 0.31f, 320.0f, 1.5f, clr300);
            drawCenteredText(textBuf, 160.0f, 5.0f, 0.44f, clrWhite, LOC(STR_WIFI_TITLE));

            // Panel Izquierdo: Código QR nítido
            if (!wifiServer.serverUrl.empty()) {
                renderQrCode(wifiServer.serverUrl.c_str(), 14.0f, 32.0f, 138.0f);
            } else {
                Ui::panel(14.0f, 32.0f, 138.0f, 138.0f, 0.22f, Ui::rgba(26, 22, 42, 235), clrPrimary);
                if (wifiServer.isConnecting()) {
                    drawCenteredText(textBuf, 83.0f, 85.0f, 0.40f, clrCursor, LOC(STR_WIFI_CONNECTING));
                    drawCenteredText(textBuf, 83.0f, 105.0f, 0.40f, clrWhite, "Wi-Fi...");
                } else {
                    drawCenteredText(textBuf, 83.0f, 85.0f, 0.40f, clrCursor, LOC(STR_WIFI_SEARCHING));
                    drawCenteredText(textBuf, 83.0f, 105.0f, 0.40f, clrWhite, LOC(STR_WIFI_NETWORK));
                }
            }

            // Panel Derecho: Monitor de señal Wi-Fi e instrucciones rápidas
            Ui::panel(158.0f, 32.0f, 148.0f, 138.0f, 0.22f, Ui::rgba(24, 20, 40, 240), clr300);
            u8 wifiSig = osGetWifiStrength();
            renderWifiBars(166.0f, 40.0f, wifiSig, clr100, Ui::rgba(70, 65, 90));
            const char* sigText = (wifiSig == 3) ? LOC(STR_WIFI_SIG_EXCELLENT) : ((wifiSig == 2) ? LOC(STR_WIFI_SIG_GOOD) : ((wifiSig == 1) ? LOC(STR_WIFI_SIG_WEAK) : LOC(STR_WIFI_SIG_NONE)));
            drawText(textBuf, 188.0f, 39.0f, 0.36f, (wifiSig > 0) ? clr100 : clrMiss, sigText);

            drawText(textBuf, 166.0f, 58.0f, 0.44f, clrCursor, LOC(STR_WIFI_SCAN_MOBILE));
            drawText(textBuf, 166.0f, 78.0f, 0.34f, clrWhite, LOC(STR_WIFI_AIM_CAM));
            drawText(textBuf, 166.0f, 94.0f, 0.34f, clrWhite, LOC(STR_WIFI_OPEN_WEB));
            drawText(textBuf, 166.0f, 110.0f, 0.34f, clrWhite, LOC(STR_WIFI_DL_AUDIO));
            drawText(textBuf, 166.0f, 126.0f, 0.34f, clr100, LOC(STR_WIFI_STAR_FILTER));

            char countBuf[48];
            std::snprintf(countBuf, sizeof(countBuf), "%s: %d", LOC(STR_SONGS), static_cast<int>(songMgr.sets.size()));
            drawText(textBuf, 166.0f, 148.0f, 0.36f, clrWhite, countBuf);

            // Botón Izquierdo: Re-escanear canciones
            Ui::button(14.0f, 178.0f, 140.0f, 48.0f, 0.25f, Ui::rgba(22, 38, 62, 240), Ui::rgba(40, 180, 240), false);
            Ui::pill(22.0f, 184.0f, 26.0f, 16.0f, 0.28f, Ui::rgba(40, 180, 240));
            drawCenteredText(textBuf, 35.0f, 192.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "X", 0.32f);
            drawTextWithShadow(textBuf, 54.0f, 184.0f, 0.38f, clrWhite, LOC(STR_WIFI_RESCAN_BTN), 0.30f);
            drawTextWithShadow(textBuf, 22.0f, 206.0f, 0.30f, clr100, LOC(STR_WIFI_RESCAN_SUB), 0.30f);

            // Botón Derecho: Volver al menú
            Ui::button(166.0f, 178.0f, 140.0f, 48.0f, 0.25f, Ui::rgba(42, 20, 52, 240), clrPrimary, false);
            Ui::pill(174.0f, 184.0f, 26.0f, 16.0f, 0.28f, clrPrimary);
            drawCenteredText(textBuf, 187.0f, 192.0f, 0.30f, clrWhite, "B", 0.32f);
            drawTextWithShadow(textBuf, 206.0f, 184.0f, 0.40f, clrWhite, LOC(STR_WIFI_BACK_BTN), 0.30f);
            drawTextWithShadow(textBuf, 174.0f, 206.0f, 0.30f, Ui::rgba(255, 180, 215), LOC(STR_WIFI_SAVE_SUB), 0.30f);

            Ui::drawTransition(320.0f, 240.0f, screenFade, clr300);

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 2. MENÚ DE CONFIGURACIONES (Tablet, Audio, Calibración, Skins)
        // =========================================================
        if (appState == STATE_SETTINGS) {
            audio.update();
            if (!calibrationMode && settings.songPreview && !audio.isPreviewLoading() && audio.isSongFinished()) {
                rotateMenuSong();
            }
            auto getTabForOption = [](int opt) -> int {
                if (opt >= 0 && opt <= 3) return 0;
                if (opt >= 4 && opt <= 7) return 1;
                if (opt >= 8 && opt <= 11) return 2;
                if ((opt >= 12 && opt <= 15) || opt == 20 || opt == 22) return 3;
                if ((opt >= 16 && opt <= 19) || opt == 21) return 4;
                return 0;
            };
            const int tabItems[5][6] = {
                { 0, 1, 2, 3, -1, -1 },
                { 4, 5, 6, 7, -1, -1 },
                { 8, 9, 10, 11, -1, -1 },
                { 22, 12, 13, 14, 15, 20 },
                { 16, 17, 18, 19, 21, -1 }
            };
            const int tabItemCounts[5] = { 4, 4, 4, 6, 5 };
            int activeTab = getTabForOption(settingsOption); // 0: TABLET, 1: CONTROLES, 2: AUDIO, 3: JUEGO, 4: VISUAL

            if (capturingKeyIndex != -1) {
                u32 anyKey = kDown & (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_SELECT | KEY_START);
                if (touchJustDown) {
                    u32 touchedKey = 0;
                    if (activeTab == 1) {
                        if (touch.px >= 28 && touch.px <= 70 && touch.py >= 68 && touch.py <= 84) touchedKey = KEY_L;
                        else if (touch.px >= 250 && touch.px <= 292 && touch.py >= 68 && touch.py <= 84) touchedKey = KEY_R;
                        else if (touch.px >= 72 && touch.px <= 100 && touch.py >= 68 && touch.py <= 82) touchedKey = KEY_ZL;
                        else if (touch.px >= 220 && touch.px <= 248 && touch.py >= 68 && touch.py <= 82) touchedKey = KEY_ZR;
                        else if (touch.px >= 53 && touch.px <= 67 && touch.py >= 135 && touch.py <= 150) touchedKey = KEY_DUP;
                        else if (touch.px >= 53 && touch.px <= 67 && touch.py >= 155 && touch.py <= 170) touchedKey = KEY_DDOWN;
                        else if (touch.px >= 42 && touch.px <= 55 && touch.py >= 145 && touch.py <= 162) touchedKey = KEY_DLEFT;
                        else if (touch.px >= 63 && touch.px <= 78 && touch.py >= 145 && touch.py <= 162) touchedKey = KEY_DRIGHT;
                        else if (std::hypot(touch.px - 258.0f, touch.py - 124.0f) <= 12.0f) touchedKey = KEY_X;
                        else if (std::hypot(touch.px - 258.0f, touch.py - 156.0f) <= 12.0f) touchedKey = KEY_B;
                        else if (std::hypot(touch.px - 242.0f, touch.py - 140.0f) <= 12.0f) touchedKey = KEY_Y;
                        else if (std::hypot(touch.px - 274.0f, touch.py - 140.0f) <= 12.0f) touchedKey = KEY_A;
                        else if (touch.px >= 118 && touch.px <= 140 && touch.py >= 174 && touch.py <= 188) touchedKey = KEY_SELECT;
                        else if (touch.px >= 180 && touch.px <= 202 && touch.py >= 174 && touch.py <= 188) touchedKey = KEY_START;
                    }

                    if (touchedKey != 0) {
                        anyKey = touchedKey;
                    } else {
                        capturingKeyIndex = -1;
                        audio.playHitSound(0);
                        continue;
                    }
                }

                if (anyKey) {
                    if (anyKey & KEY_B) {
                        capturingKeyIndex = -1;
                        audio.playHitSound(0);
                    } else {
                        audio.playHitSound(2);
                        int row = capturingKeyIndex - 4; // 0..3
                        if (settingsControlsMode == MODE_STD) {
                            if (row == 0) settings.keyOsuK1 = anyKey;
                            else if (row == 1) settings.keyOsuK2 = anyKey;
                            else if (row == 2) settings.keyOsuSmoke = anyKey;
                            else if (row == 3) settings.keyQuickRetry = anyKey;
                        } else if (settingsControlsMode == MODE_TAIKO) {
                            if (row == 0) settings.keyTaikoDonLeft = anyKey;
                            else if (row == 1) settings.keyTaikoDonRight = anyKey;
                            else if (row == 2) settings.keyTaikoKatsuLeft = anyKey;
                            else if (row == 3) settings.keyTaikoKatsuRight = anyKey;
                            settings.keyTaikoDon = settings.keyTaikoDonLeft | settings.keyTaikoDonRight;
                            settings.keyTaikoKatsu = settings.keyTaikoKatsuLeft | settings.keyTaikoKatsuRight;
                        } else if (settingsControlsMode == MODE_CATCH) {
                            if (row == 0) settings.keyCatchLeft = anyKey;
                            else if (row == 1) settings.keyCatchRight = anyKey;
                            else if (row == 2) settings.keyCatchDash = anyKey;
                            else if (row == 3) settings.keyQuickRetry = anyKey;
                        } else if (settingsControlsMode == MODE_MANIA) {
                            if (row == 0) settings.keyManiaCol1 = anyKey;
                            else if (row == 1) settings.keyManiaCol2 = anyKey;
                            else if (row == 2) settings.keyManiaCol3 = anyKey;
                            else if (row == 3) settings.keyManiaCol4 = anyKey;
                        }
                        capturingKeyIndex = -1;
                        settings.save();
                    }
                }
                continue;
            } else if (kDown & KEY_B) {
                calibrationMode = false;
                settings.save();
                audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                audio.playHitSound(0);
                appState = settingsReturnState;
                if (appState == STATE_SONG_SELECT || appState == STATE_MAIN_MENU) updateSongPreview();
                continue;
            }

            if (capturingKeyIndex == -1 && (kDown & KEY_X)) {
                if (activeTab == 1) {
                    settings.resetControlsToDefault(settingsControlsMode);
                    audio.playHitSound(2);
                } else {
                    calibrationMode = !calibrationMode;
                    calibSamples.clear();
                    calibStartTick = osGetTime();
                    calibLastBeatIndex = -1;
                    if (calibrationMode) audio.stopMusic();
                    audio.playHitSound(2);
                }
            }

            if (capturingKeyIndex == -1 && (kDown & KEY_Y)) {
                if (activeTab == 1) {
                    settings.resetControlsToDefault(settingsControlsMode);
                    audio.playHitSound(2);
                } else {
                    settings.resetTabletArea();
                    audio.playHitSound(0);
                }
            }

            // Cambiar de categoría con gatillos L / R o ZL / ZR (5 categorías)
            if (activeTab != 1) {
                if (kDown & (KEY_L | KEY_ZL)) {
                    activeTab = (activeTab + 4) % 5;
                    settingsOption = tabItems[activeTab][0];
                    audio.playHitSound(0);
                }
                if (kDown & (KEY_R | KEY_ZR)) {
                    activeTab = (activeTab + 1) % 5;
                    settingsOption = tabItems[activeTab][0];
                    audio.playHitSound(0);
                }
            } else {
                if (kDown & KEY_ZL) {
                    activeTab = (activeTab + 4) % 5;
                    settingsOption = tabItems[activeTab][0];
                    audio.playHitSound(0);
                }
                if (kDown & KEY_ZR) {
                    activeTab = (activeTab + 1) % 5;
                    settingsOption = tabItems[activeTab][0];
                    audio.playHitSound(0);
                }
            }

            // Tocar pestañas superiores o botones inferiores en pantalla táctil
            if (touchJustDown && !calibrationMode && capturingKeyIndex == -1) {
                if (touch.py >= 6 && touch.py <= 30) {
                    for (int t = 0; t < 5; ++t) {
                        float tx = 7.0f + t * 62.0f;
                        if (touch.px >= tx && touch.px <= tx + 58.0f) {
                            activeTab = t;
                            settingsOption = tabItems[t][0];
                            audio.playHitSound(0);
                            break;
                        }
                    }
                } else if (touch.py >= 204 && touch.py <= 234) {
                    if (touch.px >= 10 && touch.px <= 104) { // CALIBRAR (X) / RESTABLECER (Tab Teclas)
                        if (activeTab == 1) {
                            settings.resetControlsToDefault(settingsControlsMode);
                            audio.playHitSound(2);
                        } else {
                            calibrationMode = !calibrationMode;
                            calibSamples.clear();
                            calibStartTick = osGetTime();
                            calibLastBeatIndex = -1;
                            if (calibrationMode) audio.stopMusic();
                            audio.playHitSound(2);
                        }
                    } else if (touch.px >= 110 && touch.px <= 204) { // RESET (Y)
                        if (activeTab == 1) {
                            settings.resetControlsToDefault(settingsControlsMode);
                            audio.playHitSound(2);
                        } else {
                            settings.resetTabletArea();
                            audio.playHitSound(0);
                        }
                    } else if (touch.px >= 210 && touch.px <= 304) { // GUARDAR (B)
                        calibrationMode = false;
                        capturingKeyIndex = -1;
                        settings.save();
                        audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                        audio.playHitSound(0);
                        appState = settingsReturnState;
                        if (appState == STATE_SONG_SELECT || appState == STATE_MAIN_MENU) updateSongPreview();
                        continue;
                    }
                }
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
            } else if (capturingKeyIndex == -1) {
                if (activeTab == 1) {
                    // En pestaña TECLAS: D-Pad Arriba / Abajo cicla únicamente entre las 4 acciones sin salirse de la pestaña
                    if (kDown & KEY_DUP) {
                        settingsOption = (settingsOption <= 4) ? 7 : (settingsOption - 1);
                        audio.playHitSound(0);
                    }
                    if (kDown & KEY_DDOWN) {
                        settingsOption = (settingsOption >= 7) ? 4 : (settingsOption + 1);
                        audio.playHitSound(0);
                    }
                    // D-Pad Izquierda / Derecha cambia de modo (STD / TAIKO / CATCH / MANIA)
                    if (kDown & KEY_DLEFT) {
                        settingsControlsMode = (settingsControlsMode + 3) % 4;
                        audio.playHitSound(2);
                    }
                    if (kDown & KEY_DRIGHT) {
                        settingsControlsMode = (settingsControlsMode + 1) % 4;
                        audio.playHitSound(2);
                    }
                } else {
                    int curIdx = 0;
                    int count = tabItemCounts[activeTab];
                    for (int i = 0; i < count; ++i) {
                        if (tabItems[activeTab][i] == settingsOption) {
                            curIdx = i;
                            break;
                        }
                    }
                    if (kDown & KEY_DUP) {
                        curIdx = (curIdx - 1 + count) % count;
                        settingsOption = tabItems[activeTab][curIdx];
                        audio.playHitSound(0);
                    }
                    if (kDown & KEY_DDOWN) {
                        curIdx = (curIdx + 1) % count;
                        settingsOption = tabItems[activeTab][curIdx];
                        audio.playHitSound(0);
                    }

                    int stepDir = 0;
                    if (kDown & KEY_DLEFT)  stepDir = -1;
                    if (kDown & KEY_DRIGHT) stepDir = +1;

                    if (stepDir != 0) {
                        int numSkins = std::max(1, static_cast<int>(skinMgr.skins.size()));
                        switch (settingsOption) {
                            case 0: // Modo de Entrada
                                settings.cycleInputMode(stepDir);
                                break;
                            case 1: // Tamaño Área Tablet
                                settings.tabletScale += stepDir * 0.05f;
                                break;
                            case 2: // Posición Horizontal X
                                settings.tabletCenterX += stepDir * 8.0f;
                                break;
                            case 3: // Posición Vertical Y
                                settings.tabletCenterY += stepDir * 8.0f;
                                break;
                        case 8: // Volumen Música
                            settings.musicVolume += stepDir * 5;
                            settings.clampValues();
                            audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                            break;
                        case 9: // Volumen Hitsounds
                            settings.sfxVolume += stepDir * 5;
                            settings.clampValues();
                            audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                            audio.playHitSound(2);
                            break;
                        case 10: // Offset de Audio
                            settings.audioOffsetMs += stepDir * ((kHeld & (KEY_L | KEY_R)) ? 5 : 1);
                            break;
                        case 11: // Hitsounds Propios del Beatmap
                            settings.beatmapHitsounds = !settings.beatmapHitsounds;
                            break;
                        case 22: // Idioma / Language
                            settings.language = (settings.language + stepDir + LANG_COUNT) % LANG_COUNT;
                            Localization::setLanguage(static_cast<AppLanguage>(settings.language));
                            settings.save();
                            audio.playHitSound(2);
                            break;
                        case 12: // Modo de Juego (STD / TAIKO / CATCH / MANIA)
                            settings.gameMode = static_cast<GameMode>((settings.gameMode + stepDir + 4) % 4);
                            break;
                        case 13: // Skin Activo (.osk / Presets)
                            settings.skinIndex = (settings.skinIndex + stepDir + numSkins) % numSkins;
                            applyActiveSkinAudio();
                            audio.playHitSound(2);
                            break;
                        case 14: // Sliders Serpiente (Snaking Sliders)
                            settings.snakingSliders = !settings.snakingSliders;
                            break;
                        case 15: // Combo Bursts (Mascota)
                            settings.comboBursts = !settings.comboBursts;
                            break;
                        case 20: // Efecto Visual Golpes 300
                            settings.showHit300 = !settings.showHit300;
                            break;
                        case 16: // Fondo en Modo Taiko (Festivo / Imagen / Video)
                            settings.taikoBgMode = (settings.taikoBgMode + stepDir + 3) % 3;
                            break;
                        case 17: // Tamaño del Cursor
                            settings.cursorScale += stepDir * 0.1f;
                            break;
                        case 18: // Atenuación del Fondo (Dim %)
                            settings.bgDim += stepDir * 5;
                            break;
                        case 19: // Videos de Fondo (.3dsv)
                            settings.enableVideo = !settings.enableVideo;
                            break;
                        case 21: // Descargas en Multijugador
                            settings.multiDlOption = (settings.multiDlOption + stepDir + 2) % 2;
                            multiMgr.enableBgDownload = (settings.multiDlOption != 0);
                            break;
                    }
                    settings.clampValues();
                }

                if (kDown & KEY_A) {
                    if (settingsOption == 0) settings.cycleInputMode(1);
                    else if (settingsOption >= 4 && settingsOption <= 7) {
                        capturingKeyIndex = settingsOption;
                        audio.playHitSound(2);
                    }
                    else if (settingsOption == 10) { // Abrir calibrador rápido
                        calibrationMode = true;
                        calibSamples.clear();
                        calibStartTick = osGetTime();
                        calibLastBeatIndex = -1;
                        audio.stopMusic();
                    }
                    else if (settingsOption == 11) settings.beatmapHitsounds = !settings.beatmapHitsounds;
                    else if (settingsOption == 22) {
                        settings.language = (settings.language + 1) % LANG_COUNT;
                        Localization::setLanguage(static_cast<AppLanguage>(settings.language));
                        settings.save();
                        audio.playHitSound(2);
                    }
                    else if (settingsOption == 12) {
                        settings.gameMode = static_cast<GameMode>((settings.gameMode + 1) % 4);
                        settings.save();
                    }
                    else if (settingsOption == 13) {
                        settings.skinIndex = (settings.skinIndex + 1) % std::max(1, static_cast<int>(skinMgr.skins.size()));
                        applyActiveSkinAudio();
                    }
                    else if (settingsOption == 14) settings.snakingSliders = !settings.snakingSliders;
                    else if (settingsOption == 15) settings.comboBursts = !settings.comboBursts;
                    else if (settingsOption == 20) {
                        settings.showHit300 = !settings.showHit300;
                        settings.save();
                    }
                    else if (settingsOption == 16) {
                        settings.taikoBgMode = (settings.taikoBgMode + 1) % 3;
                        settings.save();
                    }
                    else if (settingsOption == 19) settings.enableVideo = !settings.enableVideo;
                    else if (settingsOption == 21) {
                        settings.multiDlOption = (settings.multiDlOption + 1) % 2;
                        multiMgr.enableBgDownload = (settings.multiDlOption != 0);
                        settings.save();
                    }
                    audio.playHitSound(0);
                }
            }
        }

            if (std::abs(cpad.dx) > 20 || std::abs(cpad.dy) > 20) {
                settings.tabletCenterX += (cpad.dx / 156.0f) * 3.0f;
                settings.tabletCenterY -= (cpad.dy / 156.0f) * 3.0f;
                settings.clampValues();
            }

            if (isTouching) {
                if (!settings.playOnBottomScreen && (settingsOption == 2 || settingsOption == 3 || (kHeld & KEY_L))) {
                    settings.tabletCenterX = touch.px;
                    settings.tabletCenterY = touch.py;
                    settings.clampValues();
                }
                settings.mapTouchToOsu(touch.px, touch.py, cursorOsuX, cursorOsuY);
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            float rx, ry, rw, rh;
            settings.getTabletRect(rx, ry, rw, rh);
            activeTab = getTabForOption(settingsOption);

            // --- PANTALLA SUPERIOR: TARJETAS REDISEÑADAS POR CATEGORÍA ESTILO OSU!LAZER ---
            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                float dBg = depthShift(-3.5f);
                if (bgTex.loaded) {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(28, 18, 52), Ui::rgba(10, 8, 22), clrPrimary, 0.01f);
                    bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
                    u8 dimAlpha = static_cast<u8>((settings.bgDim * 220) / 100);
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(14, 10, 24, dimAlpha));
                } else {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(28, 18, 52), Ui::rgba(10, 8, 22), clrPrimary, 0.02f);
                }

                // Barra superior con pestañas de categoría
                Ui::gradV(0.0f, 0.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 160));
                C2D_DrawRectSolid(0.0f, 27.0f, 0.31f, 400.0f, 1.5f, clrPrimary);

                const char* tabNames[5] = {
                    LOC(STR_TAB_TABLET),
                    LOC(STR_TAB_CONTROLS),
                    LOC(STR_TAB_AUDIO),
                    LOC(STR_TAB_GAME),
                    LOC(STR_TAB_VISUAL)
                };
                for (int t = 0; t < 5; ++t) {
                    float tx = 6.0f + t * 62.0f;
                    bool isTabActive = (activeTab == t);
                    if (isTabActive) {
                        Ui::pill(tx, 4.0f, 58.0f, 19.0f, 0.32f, clrPrimary);
                        drawCenteredTextWithShadow(textBuf, tx + 29.0f, 13.5f, 0.33f, clrWhite, tabNames[t], 0.85f);
                    } else {
                        Ui::pill(tx, 4.0f, 58.0f, 19.0f, 0.32f, Ui::rgba(42, 36, 64, 240));
                        drawCenteredTextWithShadow(textBuf, tx + 29.0f, 13.5f, 0.31f, Ui::rgba(225, 220, 245), tabNames[t], 0.85f);
                    }
                }
                Ui::pill(320.0f, 4.0f, 44.0f, 19.0f, 0.32f, isNew3DS ? Ui::rgba(40, 75, 45, 220) : Ui::rgba(35, 30, 50, 220));
                drawCenteredTextWithShadow(textBuf, 342.0f, 13.5f, 0.31f, isNew3DS ? clr100 : Ui::rgba(210, 205, 230), isNew3DS ? "N3DS" : "3DS", 0.85f);

                // Si la categoría activa es CONTROLES (activeTab == 1), mostramos la PREVISUALIZACIÓN VIVA DEL MODO
                if (activeTab == 1) {
                    float panX = 12.0f + depthShift(1.8f);
                    float panY = 32.0f;
                    float panW = 376.0f;
                    float panH = 178.0f;
                    Ui::panel(panX, panY, panW, panH, 0.14f, Ui::rgba(18, 14, 28, 240), Ui::rgba(65, 52, 95));

                    const char* subNames[4] = { "osu! (Standard)", "osu!taiko", "osu!catch", "osu!mania (4K)" };
                    char mTitleBuf[64];
                    std::snprintf(mTitleBuf, sizeof(mTitleBuf), "%s: %s", LOC(STR_PREVIEW), subNames[settingsControlsMode]);
                    u32 mTitleColors[4] = {
                        Ui::rgba(255, 102, 171),
                        Ui::rgba(255, 75, 80),
                        Ui::rgba(45, 215, 115),
                        Ui::rgba(60, 205, 255)
                    };

                    drawTextWithShadow(textBuf, panX + 10.0f, panY + 5.0f, 0.42f, mTitleColors[settingsControlsMode], mTitleBuf, 0.16f);
                    drawTextWithShadow(textBuf, panX + 10.0f, panY + 19.0f, 0.31f, Ui::rgba(200, 195, 225), LOC(STR_CTRL_HELP_LIVE), 0.16f);

                    if (capturingKeyIndex != -1) {
                        Ui::pill(panX + 180.0f, panY + 4.0f, 186.0f, 20.0f, 0.90f, Ui::rgba(255, 180, 40, 240));
                        drawCenteredTextWithShadow(textBuf, panX + 273.0f, panY + 14.0f, 0.32f, clrBgDark, LOC(STR_CTRL_ASSIGNING), 0.95f);
                    }

                    if (settingsControlsMode == MODE_STD) {
                        float hudX = panX + 10.0f;
                        float hudY = panY + 34.0f;
                        float hudW = 110.0f;

                        bool k1Active = (kHeld & settings.keyOsuK1) || (animTick % 120 >= 56 && animTick % 120 <= 64);
                        bool k2Active = (kHeld & settings.keyOsuK2) || (animTick % 120 >= 114 && animTick % 120 <= 122);
                        bool smokeActive = (kHeld & settings.keyOsuSmoke);
                        bool retryActive = (kHeld & settings.keyQuickRetry);

                        auto drawKeyBadge = [&](float x, float y, const char* name, const char* keyStr, bool active, u32 actClr) {
                            u32 bg = active ? actClr : Ui::rgba(28, 22, 44, 230);
                            Ui::pill(x, y, hudW, 29.0f, 0.18f, bg);
                            drawTextWithShadow(textBuf, x + 8.0f, y + 4.0f, 0.33f, active ? clrWhite : clrCursor, name, 0.20f);
                            drawTextWithShadow(textBuf, x + 8.0f, y + 16.0f, 0.30f, active ? clrWhite : Ui::rgba(225, 220, 245), keyStr, 0.20f);
                            if (active) Ui::glow(x + hudW * 0.5f, y + 14.0f, 0.17f, 16.0f, Ui::withAlpha(actClr, 130), 2);
                        };

                        std::string sK1 = truncateString(GameSettings::formatKeyMask(settings.keyOsuK1), 14);
                        std::string sK2 = truncateString(GameSettings::formatKeyMask(settings.keyOsuK2), 14);
                        std::string sSm = truncateString(GameSettings::formatKeyMask(settings.keyOsuSmoke), 14);
                        std::string sRt = truncateString(GameSettings::formatKeyMask(settings.keyQuickRetry), 14);

                        drawKeyBadge(hudX, hudY,        LOC(STR_KEY_K1), sK1.c_str(), k1Active, Ui::rgba(180, 70, 240));
                        drawKeyBadge(hudX, hudY + 33.0f, LOC(STR_KEY_K2), sK2.c_str(), k2Active, Ui::rgba(255, 95, 180));
                        drawKeyBadge(hudX, hudY + 66.0f, LOC(STR_KEY_SMOKE),  sSm.c_str(), smokeActive, Ui::rgba(240, 200, 50));
                        drawKeyBadge(hudX, hudY + 99.0f, LOC(STR_KEY_RETRY),     sRt.c_str(), retryActive, Ui::rgba(255, 75, 75));

                        float pfX = panX + 128.0f;
                        float pfY = panY + 34.0f;
                        float pfW = 238.0f;
                        float pfH = 132.0f;
                        Ui::panel(pfX, pfY, pfW, pfH, 0.15f, Ui::rgba(12, 10, 20, 240), Ui::rgba(55, 45, 80));

                        int t = animTick % 120;
                        float c1X = pfX + 65.0f, c1Y = pfY + 66.0f;
                        float c2X = pfX + 172.0f, c2Y = pfY + 56.0f;

                        if (t < 60) {
                            float appR = 16.0f + (1.0f - (t / 60.0f)) * 32.0f;
                            drawRing(c1X, c1Y, appR, 1.5f, Ui::rgba(255, 102, 171), 22, 0.22f);
                            C2D_DrawCircleSolid(c1X, c1Y, 0.20f, 16.0f, Ui::rgba(255, 102, 171));
                            drawRing(c1X, c1Y, 16.0f, 2.0f, clrWhite, 22, 0.21f);
                            drawCenteredText(textBuf, c1X, c1Y - 6.0f, 0.40f, clrWhite, "1", 0.23f);
                        } else if (t < 78) {
                            float burstR = 16.0f + (t - 60) * 1.5f;
                            u8 bAlpha = static_cast<u8>(255 * (1.0f - (t - 60) / 18.0f));
                            drawRing(c1X, c1Y, burstR, 2.0f, Ui::rgba(255, 215, 60, bAlpha), 20, 0.24f);
                            drawCenteredText(textBuf, c1X, c1Y - 10.0f - (t - 60) * 0.8f, 0.42f, Ui::rgba(100, 240, 255, bAlpha), "300", 0.25f);
                        }

                        if (t >= 30) {
                            float p2 = std::min(1.0f, (t - 30) / 90.0f);
                            float appR = 16.0f + (1.0f - p2) * 32.0f;
                            drawRing(c2X, c2Y, appR, 1.5f, Ui::rgba(100, 180, 255), 22, 0.22f);
                            C2D_DrawCircleSolid(c2X, c2Y, 0.20f, 16.0f, Ui::rgba(100, 180, 255));
                            drawRing(c2X, c2Y, 16.0f, 2.0f, clrWhite, 22, 0.21f);
                            drawCenteredText(textBuf, c2X, c2Y - 6.0f, 0.40f, clrWhite, "2", 0.23f);
                        }

                        float curX, curY;
                        if (t < 60) {
                            float prog = t / 60.0f;
                            float smooth = prog * prog * (3.0f - 2.0f * prog);
                            curX = pfX + 20.0f + smooth * (c1X - (pfX + 20.0f));
                            curY = pfY + 100.0f + smooth * (c1Y - (pfY + 100.0f));
                        } else {
                            float prog = (t - 60) / 60.0f;
                            float smooth = prog * prog * (3.0f - 2.0f * prog);
                            curX = c1X + smooth * (c2X - c1X);
                            curY = c1Y + smooth * (c2Y - c1Y);
                        }

                        Ui::glow(curX, curY, 0.26f, 12.0f, Ui::withAlpha(clrCursor, 140), 2);
                        C2D_DrawCircleSolid(curX, curY, 0.27f, 6.0f, clrCursor);
                        C2D_DrawCircleSolid(curX, curY, 0.28f, 2.0f, clrWhite);
                    }
                    else if (settingsControlsMode == MODE_TAIKO) {
                        float trkX = panX + 10.0f;
                        float trkY = panY + 34.0f;
                        float trkW = 356.0f;
                        float trkH = 70.0f;
                        Ui::panel(trkX, trkY, trkW, trkH, 0.15f, Ui::rgba(22, 18, 32, 250), Ui::rgba(70, 55, 95));

                        float drumX = trkX + 46.0f;
                        float drumY = trkY + trkH * 0.5f;

                        bool kLeftDown  = (kHeld & settings.keyTaikoKatsuLeft);
                        bool kRightDown = (kHeld & settings.keyTaikoKatsuRight);
                        bool dLeftDown  = (kHeld & settings.keyTaikoDonLeft);
                        bool dRightDown = (kHeld & settings.keyTaikoDonRight);

                        C2D_DrawCircleSolid(drumX, drumY, 0.17f, 28.0f, (kLeftDown || kRightDown) ? clrWhite : Ui::rgba(35, 120, 220));
                        C2D_DrawCircleSolid(drumX, drumY, 0.19f, 18.0f, (dLeftDown || dRightDown) ? clrWhite : Ui::rgba(240, 55, 65));

                        C2D_DrawCircleSolid(drumX - 6.0f, drumY - 2.0f, 0.21f, 2.2f, clrBgDark);
                        C2D_DrawCircleSolid(drumX + 6.0f, drumY - 2.0f, 0.21f, 2.2f, clrBgDark);
                        drawRing(drumX, drumY + 4.0f, 4.0f, 1.2f, clrBgDark, 12, 0.21f);

                        float targetX = trkX + 115.0f;
                        drawRing(targetX, drumY, 15.0f, 2.0f, clrWhite, 24, 0.20f);
                        drawRing(targetX, drumY, 8.0f, 1.5f, Ui::rgba(180, 175, 210), 16, 0.19f);

                        int loopT = animTick % 160;
                        float speed = 2.4f;

                        for (int n = 0; n < 4; ++n) {
                            int nOffset = n * 40;
                            float notePos = ((loopT + nOffset) % 160) * speed;
                            float nx = (trkX + trkW) - notePos;
                            if (nx >= trkX + 85.0f && nx <= trkX + trkW - 5.0f) {
                                bool isDon = (n % 2 == 0);
                                u32 noteClr = isDon ? Ui::rgba(245, 60, 65) : Ui::rgba(45, 135, 255);
                                C2D_DrawCircleSolid(nx, drumY, 0.22f, 12.0f, noteClr);
                                drawRing(nx, drumY, 12.0f, 1.8f, clrWhite, 20, 0.23f);

                                if (std::abs(nx - targetX) < 6.0f) {
                                    Ui::glow(targetX, drumY, 0.25f, 22.0f, Ui::withAlpha(noteClr, 160), 2);
                                    drawCenteredText(textBuf, targetX, drumY - 24.0f, 0.38f, Ui::rgba(255, 220, 80), "GREAT 300", 0.26f);
                                }
                            }
                        }

                        float rowY = panY + 110.0f;
                        float cW = 86.0f;
                        float cH = 56.0f;

                        auto drawTaikoCard = [&](float cx, const char* name, const char* sub, uint32_t mask, u32 actClr, bool active) {
                            Ui::panel(cx, rowY, cW, cH, 0.16f, active ? Ui::rgba(45, 38, 70, 255) : Ui::rgba(24, 20, 36, 230), active ? clrWhite : actClr);
                            drawCenteredText(textBuf, cx + cW * 0.5f, rowY + 5.0f, 0.28f, actClr, name, 0.18f);
                            drawCenteredText(textBuf, cx + cW * 0.5f, rowY + 18.0f, 0.24f, clrMuted, sub, 0.18f);
                            std::string kStr = truncateString(GameSettings::formatKeyMask(mask), 12);
                            Ui::pill(cx + 6.0f, rowY + 32.0f, cW - 12.0f, 18.0f, 0.19f, active ? actClr : Ui::rgba(40, 32, 60));
                            drawCenteredText(textBuf, cx + cW * 0.5f, rowY + 41.0f, 0.26f, clrWhite, kStr.c_str(), 0.20f);
                        };

                        drawTaikoCard(trkX,               "DON IZQ",   LOC(STR_TAIKO_RED), settings.keyTaikoDonLeft,    Ui::rgba(245, 60, 65),  dLeftDown);
                        drawTaikoCard(trkX + cW + 4.0f,   "DON DER",   LOC(STR_TAIKO_RED), settings.keyTaikoDonRight,   Ui::rgba(245, 60, 65),  dRightDown);
                        drawTaikoCard(trkX + (cW+4.0f)*2, "KATSU IZQ", LOC(STR_TAIKO_BLUE), settings.keyTaikoKatsuLeft,  Ui::rgba(45, 135, 255), kLeftDown);
                        drawTaikoCard(trkX + (cW+4.0f)*3, "KATSU DER", LOC(STR_TAIKO_BLUE), settings.keyTaikoKatsuRight, Ui::rgba(45, 135, 255), kRightDown);
                    }
                    else if (settingsControlsMode == MODE_CATCH) {
                        float hudX = panX + 10.0f;
                        float hudY = panY + 34.0f;
                        float hudW = 110.0f;

                        bool cLeftActive  = (kHeld & settings.keyCatchLeft);
                        bool cRightActive = (kHeld & settings.keyCatchRight);
                        bool cDashActive  = (kHeld & settings.keyCatchDash);
                        bool cRetryActive = (kHeld & settings.keyQuickRetry);

                        auto drawCatchBadge = [&](float x, float y, const char* name, const char* keyStr, bool active, u32 actClr) {
                            u32 bg = active ? actClr : Ui::rgba(28, 22, 44, 230);
                            Ui::pill(x, y, hudW, 29.0f, 0.18f, bg);
                            drawText(textBuf, x + 8.0f, y + 4.0f, 0.30f, active ? clrWhite : clrCursor, name, 0.20f);
                            drawText(textBuf, x + 8.0f, y + 16.0f, 0.26f, active ? clrWhite : clrMuted, keyStr, 0.20f);
                            if (active) Ui::glow(x + hudW * 0.5f, y + 14.0f, 0.17f, 16.0f, Ui::withAlpha(actClr, 130), 2);
                        };

                        std::string sL = truncateString(GameSettings::formatKeyMask(settings.keyCatchLeft), 13);
                        std::string sR = truncateString(GameSettings::formatKeyMask(settings.keyCatchRight), 13);
                        std::string sD = truncateString(GameSettings::formatKeyMask(settings.keyCatchDash), 13);
                        std::string sRt = truncateString(GameSettings::formatKeyMask(settings.keyQuickRetry), 13);

                        drawCatchBadge(hudX, hudY,        LOC(STR_KEY_MOVE_L), sL.c_str(), cLeftActive, Ui::rgba(50, 210, 120));
                        drawCatchBadge(hudX, hudY + 33.0f, LOC(STR_KEY_MOVE_R), sR.c_str(), cRightActive, Ui::rgba(50, 210, 120));
                        drawCatchBadge(hudX, hudY + 66.0f, LOC(STR_KEY_DASH),   sD.c_str(), cDashActive, Ui::rgba(255, 175, 30));
                        drawCatchBadge(hudX, hudY + 99.0f, LOC(STR_KEY_RETRY),  sRt.c_str(), cRetryActive, Ui::rgba(255, 75, 75));

                        float stX = panX + 128.0f;
                        float stY = panY + 34.0f;
                        float stW = 238.0f;
                        float stH = 132.0f;
                        Ui::panel(stX, stY, stW, stH, 0.15f, Ui::rgba(12, 10, 20, 240), Ui::rgba(50, 150, 95));

                        int cTick = animTick % 100;
                        float f1Y = stY + (cTick * 1.3f);
                        float f1X = stX + 70.0f;

                        if (f1Y < stY + stH - 18.0f) {
                            C2D_DrawCircleSolid(f1X, f1Y, 0.20f, 9.0f, Ui::rgba(235, 50, 60));
                            C2D_DrawRectSolid(f1X - 1.0f, f1Y - 12.0f, 0.21f, 2.0f, 4.0f, Ui::rgba(80, 180, 50));
                        }

                        int cTick2 = (animTick + 50) % 100;
                        float f2Y = stY + (cTick2 * 1.3f);
                        float f2X = stX + 165.0f;
                        if (f2Y < stY + stH - 18.0f) {
                            C2D_DrawCircleSolid(f2X, f2Y, 0.20f, 8.5f, Ui::rgba(255, 160, 30));
                            drawRing(f2X, f2Y, 5.0f, 1.2f, Ui::rgba(255, 210, 80), 12, 0.21f);
                        }

                        float catcherBaseX = stX + 70.0f + std::sin(animTick * 0.05f) * 60.0f;
                        if (cLeftActive)  catcherBaseX = std::max(stX + 25.0f, catcherBaseX - 3.0f);
                        if (cRightActive) catcherBaseX = std::min(stX + stW - 25.0f, catcherBaseX + 3.0f);

                        float plateY = stY + stH - 16.0f;
                        float plateW = 44.0f;

                        if (cDashActive || (cTick > 40 && cTick < 70)) {
                            Ui::pill(catcherBaseX - plateW * 0.5f - 6.0f, plateY, plateW, 6.0f, 0.18f, Ui::rgba(255, 120, 40, 120));
                        }
                        Ui::pill(catcherBaseX - plateW * 0.5f, plateY, plateW, 6.0f, 0.22f, Ui::rgba(210, 215, 235));
                        C2D_DrawCircleSolid(catcherBaseX, plateY + 8.0f, 0.21f, 6.0f, Ui::rgba(60, 180, 100));
                    }
                    else if (settingsControlsMode == MODE_MANIA) {
                        float hudX = panX + 10.0f;
                        float hudY = panY + 34.0f;
                        float hudW = 110.0f;

                        bool col1Active = (kHeld & settings.keyManiaCol1) || (animTick % 80 >= 18 && animTick % 80 <= 26);
                        bool col2Active = (kHeld & settings.keyManiaCol2) || (animTick % 80 >= 38 && animTick % 80 <= 46);
                        bool col3Active = (kHeld & settings.keyManiaCol3) || (animTick % 80 >= 58 && animTick % 80 <= 66);
                        bool col4Active = (kHeld & settings.keyManiaCol4) || (animTick % 80 >= 72 && animTick % 80 <= 79);

                        auto drawManiaBadge = [&](float x, float y, const char* name, const char* keyStr, bool active, u32 actClr) {
                            u32 bg = active ? actClr : Ui::rgba(28, 22, 44, 230);
                            Ui::pill(x, y, hudW, 29.0f, 0.18f, bg);
                            drawText(textBuf, x + 8.0f, y + 4.0f, 0.30f, active ? clrWhite : clrCursor, name, 0.20f);
                            drawText(textBuf, x + 8.0f, y + 16.0f, 0.26f, active ? clrWhite : clrMuted, keyStr, 0.20f);
                            if (active) Ui::glow(x + hudW * 0.5f, y + 14.0f, 0.17f, 16.0f, Ui::withAlpha(actClr, 130), 2);
                        };

                        std::string sC1 = truncateString(GameSettings::formatKeyMask(settings.keyManiaCol1), 13);
                        std::string sC2 = truncateString(GameSettings::formatKeyMask(settings.keyManiaCol2), 13);
                        std::string sC3 = truncateString(GameSettings::formatKeyMask(settings.keyManiaCol3), 13);
                        std::string sC4 = truncateString(GameSettings::formatKeyMask(settings.keyManiaCol4), 13);

                        char colBuf[4][32];
                        for (int c = 0; c < 4; ++c) {
                            std::snprintf(colBuf[c], sizeof(colBuf[c]), LOC(STR_KEY_COL_FMT), c + 1);
                        }
                        drawManiaBadge(hudX, hudY,        colBuf[0], sC1.c_str(), col1Active, Ui::rgba(50, 160, 255));
                        drawManiaBadge(hudX, hudY + 33.0f, colBuf[1], sC2.c_str(), col2Active, Ui::rgba(235, 235, 245));
                        drawManiaBadge(hudX, hudY + 66.0f, colBuf[2], sC3.c_str(), col3Active, Ui::rgba(235, 235, 245));
                        drawManiaBadge(hudX, hudY + 99.0f, colBuf[3], sC4.c_str(), col4Active, Ui::rgba(50, 160, 255));

                        float mStageX = panX + 158.0f;
                        float mStageY = panY + 34.0f;
                        float colW = 44.0f;
                        float mStageW = colW * 4.0f;
                        float mStageH = 132.0f;

                        Ui::panel(mStageX, mStageY, mStageW, mStageH, 0.15f, Ui::rgba(14, 12, 22, 250), Ui::rgba(60, 120, 180));

                        for (int c = 1; c < 4; ++c) {
                            float lx = mStageX + c * colW;
                            C2D_DrawLine(lx, mStageY, Ui::rgba(45, 40, 65), lx, mStageY + mStageH, Ui::rgba(45, 40, 65), 1.0f, 0.16f);
                        }

                        float judgeY = mStageY + mStageH - 24.0f;
                        C2D_DrawRectSolid(mStageX, judgeY, 0.18f, mStageW, 2.5f, clrWhite);

                        bool colActs[4] = { col1Active, col2Active, col3Active, col4Active };
                        u32 colClrs[4] = { Ui::rgba(50, 160, 255), Ui::rgba(240, 240, 255), Ui::rgba(240, 240, 255), Ui::rgba(50, 160, 255) };

                        for (int c = 0; c < 4; ++c) {
                            float cx = mStageX + c * colW;
                            if (colActs[c]) {
                                C2D_DrawRectSolid(cx + 2.0f, judgeY + 2.5f, 0.19f, colW - 4.0f, 20.0f, colClrs[c]);
                                Ui::glow(cx + colW * 0.5f, judgeY, 0.20f, 16.0f, Ui::withAlpha(colClrs[c], 140), 2);
                            } else {
                                C2D_DrawRectSolid(cx + 2.0f, judgeY + 2.5f, 0.19f, colW - 4.0f, 20.0f, Ui::rgba(30, 26, 45));
                            }
                        }

                        int mTick = animTick % 80;
                        float noteSpd = 1.6f;

                        auto drawManiaNote = [&](int col, int offset) {
                            float ny = mStageY + (((mTick + offset) % 80) * noteSpd);
                            if (ny < judgeY) {
                                float nx = mStageX + col * colW + 3.0f;
                                C2D_DrawRectSolid(nx, ny, 0.22f, colW - 6.0f, 8.0f, colClrs[col]);
                                drawRectOutline(nx, ny, colW - 6.0f, 8.0f, 1.0f, clrWhite, 0.23f);
                            }
                        };

                        drawManiaNote(0, 60);
                        drawManiaNote(1, 40);
                        drawManiaNote(2, 20);
                        drawManiaNote(3, 5);
                    }
                } else {
                    // Renderizado de las tarjetas de la categoría activa (Tabs 0, 2, 3, 4)
                    int count = tabItemCounts[activeTab];
                    float ch = (count == 6) ? 27.0f : ((count == 5) ? 31.0f : 38.0f);
                    float step = (count == 6) ? 29.5f : ((count == 5) ? 34.0f : 44.0f);
                    float startY = (count == 6) ? 31.0f : ((count == 5) ? 32.0f : 34.0f);
                    float titleYOff = (count == 6) ? 1.5f : ((count == 5) ? 2.5f : 4.0f);
                    float descYOff  = (count == 6) ? 14.5f : ((count == 5) ? 16.5f : 20.0f);
                    float ctrlYOff  = (count == 6) ? 4.0f : ((count == 5) ? 5.5f : 8.0f);
                    float swYOff    = (count == 6) ? 4.5f : ((count == 5) ? 6.5f : 11.0f);
                    float sldYOff   = (count == 6) ? 8.5f : ((count == 5) ? 10.5f : 14.0f);

                    for (int i = 0; i < count; ++i) {
                        int optIdx = tabItems[activeTab][i];
                        bool sel = (!calibrationMode && settingsOption == optIdx);
                        float dCard = depthShift(sel ? 3.5f : 1.2f);
                        float cy = startY + i * step;
                        float cw = 376.0f;
                        float cx = 12.0f + dCard;

                        u32 cardBg = sel ? Ui::rgba(38, 30, 64, 245) : Ui::rgba(22, 19, 36, 225);
                        Ui::panel(cx, cy, cw, ch, 0.15f, cardBg, sel ? clrPrimary : 0);
                        if (sel) {
                            C2D_DrawRectSolid(cx, cy, 0.22f, 3.5f, ch, clrCursor);
                            C2D_DrawLine(cx + 4.0f, cy, clrWhite, cx + cw, cy, clrWhite, 1.0f, 0.22f);
                        }

                        // Título y valor según opción
                        if (optIdx == 0) { // Modo de Entrada
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_INPUT_MODE));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_INPUT_MODE_DESC));
                            Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, (count == 6) ? 19.0f : 22.0f, 0.25f, settings.playOnBottomScreen ? Ui::rgba(35, 110, 75) : Ui::rgba(55, 45, 95));
                            drawCenteredText(textBuf, cx + 285.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.36f, clrWhite, settings.getInputModeLabel(), 0.85f);
                        } else if (optIdx == 1) { // Tamaño Área Tablet
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_TABLET_SIZE));
                            char desc[64];
                            std::snprintf(desc, sizeof(desc), "%d%% (%dx%d px)", static_cast<int>(std::round(settings.tabletScale * 100.0f)), (int)rw, (int)rh);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            float ratio = (settings.tabletScale - 0.30f) / 0.70f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clrPrimary, sel);
                        } else if (optIdx == 2) { // Posición X
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_POS_X));
                            char desc[64]; std::snprintf(desc, sizeof(desc), "X = %.0f px  (C-Pad / Stylus)", settings.tabletCenterX);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            float ratio = settings.tabletCenterX / 320.0f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clrPrimary, sel);
                        } else if (optIdx == 3) { // Posición Y
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_POS_Y));
                            char desc[64]; std::snprintf(desc, sizeof(desc), "Y = %.0f px  (C-Pad / Stylus)", settings.tabletCenterY);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            float ratio = settings.tabletCenterY / 240.0f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clrPrimary, sel);
                        } else if (optIdx == 8) { // Volumen Música
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_MUSIC_VOL));
                            char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", settings.musicVolume);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            float ratio = settings.musicVolume / 100.0f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clrPrimary, sel);
                        } else if (optIdx == 9) { // Volumen Hitsounds
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_SFX_VOL));
                            char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", settings.sfxVolume);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            float ratio = settings.sfxVolume / 100.0f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clrPrimary, sel);
                        } else if (optIdx == 10) { // Offset de Audio
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_OFFSET));
                            char desc[64]; std::snprintf(desc, sizeof(desc), LOC(STR_OFFSET_CALIB_HINT), settings.audioOffsetMs);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr100, desc);
                            float ratio = (settings.audioOffsetMs + 150) / 300.0f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clr300, sel);
                        } else if (optIdx == 11) { // Hitsounds del Beatmap
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_MAP_HITSOUNDS));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_MAP_HITSOUNDS_DESC));
                            Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, settings.beatmapHitsounds, clr100);
                        } else if (optIdx == 22) { // Idioma / Language
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_LANGUAGE));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_LANGUAGE_DESC));
                            u32 lBg = Ui::rgba(45, 115, 215);
                            Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, (count == 6) ? 19.0f : 22.0f, 0.25f, lBg);
                            drawCenteredText(textBuf, cx + 285.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.36f, clrWhite, settings.getLanguageName(), 0.85f);
                        } else if (optIdx == 12) { // Modo de Juego
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_MAIN_MODE));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_MAIN_MODE_DESC));
                            u32 mBg = (settings.gameMode == MODE_TAIKO) ? Ui::rgba(180, 45, 45) : ((settings.gameMode == MODE_CATCH) ? Ui::rgba(35, 160, 85) : ((settings.gameMode == MODE_MANIA) ? Ui::rgba(140, 35, 175) : Ui::rgba(65, 45, 115)));
                            Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, (count == 6) ? 19.0f : 22.0f, 0.25f, mBg);
                            drawCenteredText(textBuf, cx + 285.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.36f, clrWhite, settings.getGameModeName(), 0.85f);
                        } else if (optIdx == 13) { // Skin Activo
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_ACTIVE_SKIN));
                            char desc[64]; std::snprintf(desc, sizeof(desc), "%s (%d/%d)", truncateString(activeSkin.name, 22).c_str(), settings.skinIndex + 1, (int)skinMgr.skins.size());
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            for (size_t c = 0; c < std::min((size_t)4, activeSkin.comboColors.size()); ++c) {
                                u32 cc = C2D_Color32(activeSkin.comboColors[c].r, activeSkin.comboColors[c].g, activeSkin.comboColors[c].b, 255);
                                C2D_DrawCircleSolid(cx + 276.0f + c * 20.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.25f, 6.5f, cc);
                            }
                        } else if (optIdx == 14) { // Sliders Serpiente (Snaking)
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_SNAKING));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_SNAKING_DESC));
                            Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, settings.snakingSliders, clr100);
                        } else if (optIdx == 15) { // Combo Bursts (Mascota)
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_BURSTS));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_BURSTS_DESC));
                            Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, settings.comboBursts, clr100);
                        } else if (optIdx == 20) { // Efecto Visual Golpes 300
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_HIT300));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_HIT300_DESC));
                            Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, settings.showHit300, clr100);
                        } else if (optIdx == 16) { // Fondo Taiko
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_TAIKO_BG));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_TAIKO_BG_DESC));
                            u32 bClr = (settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25) : ((settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(165, 55, 185));
                            Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, 22.0f, 0.25f, bClr);
                            drawCenteredText(textBuf, cx + 285.0f, cy + ctrlYOff + 11.0f, 0.34f, clrWhite, settings.getTaikoBgModeName(), 0.85f);
                        } else if (optIdx == 17) { // Tamaño del Cursor
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_CURSOR_SIZE));
                            char desc[32]; std::snprintf(desc, sizeof(desc), "%.1fx", settings.cursorScale);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            float ratio = (settings.cursorScale - 0.5f) / 1.5f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clrPrimary, sel);
                        } else if (optIdx == 18) { // Oscurecimiento Fondo
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_BG_DIM));
                            char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", settings.bgDim);
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clr300, desc);
                            float ratio = (settings.bgDim - 20) / 80.0f;
                            Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? clrCursor : clrPrimary, sel);
                        } else if (optIdx == 19) { // Videos de Fondo
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_VIDEOS));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_VIDEOS_DESC));
                            Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, settings.enableVideo, clr100);
                        } else if (optIdx == 21) { // Descargas Multijugador
                            drawText(textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? clrWhite : clrCursor, LOC(STR_SET_MULTI_DL));
                            drawText(textBuf, cx + 12.0f, cy + descYOff, 0.35f, clrMuted, LOC(STR_SET_MULTI_DL_DESC));
                            u32 dClr = (settings.multiDlOption == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(110, 95, 140);
                            Ui::pill(cx + 185.0f, cy + ctrlYOff, 180.0f, 22.0f, 0.25f, dClr);
                            drawCenteredText(textBuf, cx + 275.0f, cy + ctrlYOff + 11.0f, 0.34f, clrWhite, settings.getMultiDlOptionName(), 0.85f);
                        }
                    }
                }

                if (capturingKeyIndex != -1 && activeTab != 1) {
                    float dCap = eyeSign * slider3D * 5.0f;
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.92f, 400.0f, 240.0f, Ui::rgba(8, 6, 16, 215));
                    Ui::panel(40.0f + dCap, 50.0f, 320.0f, 130.0f, 0.94f, Ui::rgba(26, 20, 44, 250), clrCursor);
                    int cIdx = std::max(0, std::min(3, capturingKeyIndex - 4));
                    const char* capModeName = (settingsControlsMode == MODE_TAIKO) ? "TAIKO" : ((settingsControlsMode == MODE_CATCH) ? "CATCH" : ((settingsControlsMode == MODE_MANIA) ? "MANIA" : "STANDARD"));
                    const char* capRowName = "CONTROL";
                    char maniaCap[4][32];
                    if (settingsControlsMode == MODE_STD) {
                        const char* n[4] = { LOC(STR_KEY_K1), LOC(STR_KEY_K2), LOC(STR_KEY_SMOKE), LOC(STR_KEY_RETRY) };
                        capRowName = n[cIdx];
                    } else if (settingsControlsMode == MODE_TAIKO) {
                        const char* n[4] = { LOC(STR_KEY_DON_L), LOC(STR_KEY_DON_R), LOC(STR_KEY_KAT_L), LOC(STR_KEY_KAT_R) };
                        capRowName = n[cIdx];
                    } else if (settingsControlsMode == MODE_CATCH) {
                        const char* n[4] = { LOC(STR_KEY_MOVE_L), LOC(STR_KEY_MOVE_R), LOC(STR_KEY_DASH), LOC(STR_KEY_RETRY) };
                        capRowName = n[cIdx];
                    } else if (settingsControlsMode == MODE_MANIA) {
                        for (int c = 0; c < 4; ++c) {
                            std::snprintf(maniaCap[c], sizeof(maniaCap[c]), LOC(STR_KEY_COL_FMT), c + 1);
                        }
                        capRowName = maniaCap[cIdx];
                    }
                    char capHead[64];
                    std::snprintf(capHead, sizeof(capHead), "%s [%s]", LOC(STR_CTRL_ASSIGNING), capModeName);
                    drawCenteredText(textBuf, 200.0f + dCap, 72.0f, 0.48f, clrCursor, capHead, 0.99f);
                    drawCenteredText(textBuf, 200.0f + dCap, 96.0f, 0.44f, clrWhite, capRowName, 0.99f);
                    drawCenteredText(textBuf, 200.0f + dCap, 126.0f, 0.38f, clr100, LOC(STR_CTRL_PRESS_ANY), 0.99f);
                    drawCenteredText(textBuf, 200.0f + dCap, 152.0f, 0.32f, clrMuted, LOC(STR_CTRL_CANCEL_HINT), 0.99f);
                }

                if (calibrationMode) {
                    float dCal = eyeSign * slider3D * 4.5f;
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.88f, 400.0f, 240.0f, Ui::rgba(8, 6, 16, 185));
                    Ui::panel(30.0f + dCal, 52.0f, 340.0f, 128.0f, 0.90f, Ui::rgba(22, 18, 38, 250), clr300);
                    drawCenteredText(textBuf, 200.0f + dCal, 70.0f, 0.50f, clrCursor, LOC(STR_CALIB_TITLE), 0.98f);
                    drawCenteredText(textBuf, 200.0f + dCal, 94.0f, 0.40f, clrWhite, LOC(STR_CALIB_DESC), 0.98f);

                    u64 elapsed = osGetTime() - calibStartTick;
                    float beatPhase = (elapsed % 500) / 500.0f;
                    float pulseR = 14.0f * (1.0f - beatPhase * 0.55f);
                    Ui::ring(200.0f + dCal, 122.0f, 0.96f, pulseR, 2.0f, clr300, 20);

                    char calStr[96];
                    std::snprintf(calStr, sizeof(calStr), LOC(STR_CALIB_STATUS_FMT),
                        static_cast<int>(calibSamples.size()), settings.audioOffsetMs);
                    drawCenteredText(textBuf, 200.0f + dCal, 146.0f, 0.44f, clr100, calStr, 0.98f);
                    drawCenteredText(textBuf, 200.0f + dCal, 166.0f, 0.38f, clr300, LOC(STR_CALIB_APPLY_HINT), 0.98f);
                }

                Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
                drawText(textBuf, 8.0f, 219.0f, 0.37f, clr300, LOC(STR_SET_HELP_BAR));

                float testR = 6.0f * settings.cursorScale;
                float dCur = eyeSign * slider3D * 4.5f;
                Ui::glow(osuToTopX(cursorOsuX) + dCur, osuToTopY(cursorOsuY), 0.94f, testR * 1.9f, Ui::withAlpha(clrCursor, 110), 3);
                C2D_DrawCircleSolid(osuToTopX(cursorOsuX) + dCur, osuToTopY(cursorOsuY), 0.95f, testR, clrCursor);
                C2D_DrawCircleSolid(osuToTopX(cursorOsuX) + dCur, osuToTopY(cursorOsuY), 0.97f, 2.0f, clrWhite);
                Ui::drawTransition(400.0f, 240.0f, screenFade, clrPrimary);
            }

            // --- PANTALLA INFERIOR: CONTROL TÁCTIL INTERACTIVO + EDITOR DE ÁREA ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), clrPrimary, 0.02f);
            if (bgTex.loaded) {
                bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.03f, 60);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 320.0f, 240.0f, Ui::rgba(14, 11, 24, 180));
            }

            // Botones de pestañas táctiles arriba (5 pestañas)
            const char* tabNames[5] = {
                LOC(STR_TAB_TABLET),
                LOC(STR_TAB_CONTROLS),
                LOC(STR_TAB_AUDIO),
                LOC(STR_TAB_GAME),
                LOC(STR_TAB_VISUAL)
            };
            for (int t = 0; t < 5; ++t) {
                float tx = 7.0f + t * 62.0f;
                float tw = 58.0f;
                float th = 22.0f;
                bool isTab = (activeTab == t);
                u32 tabBg = isTab ? clrPrimary : Ui::rgba(24, 20, 36, 235);
                Ui::panel(tx, 6.0f, tw, th, 0.20f, tabBg, isTab ? Ui::rgba(255, 255, 255) : 0);
                if (isTab) {
                    Ui::glow(tx + tw * 0.5f, 17.0f, 0.18f, tw * 0.45f, Ui::withAlpha(clrPrimary, 90), 2);
                }
                const char* tLbl = tabNames[t];
                if (tLbl[0] >= '1' && tLbl[0] <= '5' && tLbl[1] == '.') tLbl += 3;
                drawCenteredTextWithShadow(textBuf, tx + tw * 0.5f, 17.0f, 0.32f, isTab ? clrWhite : Ui::rgba(180, 175, 205), tLbl, 0.25f);
            }

            // Contenido central interactivo
            if (activeTab == 0) { // Controles / Tablet
                if (settings.playOnBottomScreen) {
                    Ui::panel(10.0f, 36.0f, 300.0f, 162.0f, 0.15f, Ui::rgba(28, 22, 48, 225), clr100);
                    drawCenteredText(textBuf, 160.0f, 52.0f, 0.44f, clr100, LOC(STR_CTRL_TOUCH_1TO1));
                    drawCenteredText(textBuf, 160.0f, 72.0f, 0.36f, clrWhite, LOC(STR_CTRL_TOUCH_1TO1_DESC));
                    drawRing(160.0f, 122.0f, 22.0f, 2.5f, clrPrimary, 24, 0.3f);
                    C2D_DrawCircleSolid(160.0f, 122.0f, 0.25f, 22.0f, Ui::withAlpha(clrPrimary, 140));
                    drawCenteredText(textBuf, 160.0f, 122.0f, 0.55f, clrWhite, "1", 0.35f);
                } else {
                    Ui::panel(rx, ry, rw, rh, 0.15f, Ui::rgba(42, 34, 70, 225), clrPrimary);
                    u32 gridClr = C2D_Color32(110, 95, 160, 140);
                    C2D_DrawLine(rx + rw * 0.5f, ry, gridClr, rx + rw * 0.5f, ry + rh, gridClr, 1.0f, 0.2f);
                    C2D_DrawLine(rx, ry + rh * 0.5f, gridClr, rx + rw, ry + rh * 0.5f, gridClr, 1.0f, 0.2f);
                    drawRectOutline(rx, ry, rw, rh, 2.0f, clrPrimary, 0.25f);
                    drawRing(settings.tabletCenterX, settings.tabletCenterY, 7.0f, 1.5f, clrCursor, 18, 0.3f);

                    char areaCaption[64];
                    std::snprintf(areaCaption, sizeof(areaCaption), LOC(STR_TABLET_AREA_FMT),
                        static_cast<int>(std::round(settings.tabletScale * 100.0f)), (int)rw, (int)rh);
                    drawCenteredText(textBuf, settings.tabletCenterX, std::max(40.0f, ry + 14.0f), 0.38f, clrWhite, areaCaption);
                }
            } else if (activeTab == 1) { // Pestaña CONTROLES: Modo 3DS Interactivo
                // 1. Selector táctil de modo (STD / TAIKO / CATCH / MANIA)
                const char* subModeNames[4] = { "STANDARD", "TAIKO", "CATCH", "MANIA" };
                u32 subModeColors[4] = { Ui::rgba(255, 102, 171), Ui::rgba(255, 75, 80), Ui::rgba(45, 215, 115), Ui::rgba(60, 205, 255) };

                for (int m = 0; m < 4; ++m) {
                    float sx = 6.0f + m * 77.0f;
                    bool isCurM = (settingsControlsMode == m);
                    u32 pillBg = isCurM ? Ui::withAlpha(subModeColors[m], 235) : Ui::rgba(32, 26, 48, 235);
                    Ui::pill(sx, 31.0f, 74.0f, 17.0f, 0.22f, pillBg);
                    if (isCurM) drawRectOutline(sx, 31.0f, 74.0f, 17.0f, 1.2f, clrWhite, 0.24f);
                    drawCenteredTextWithShadow(textBuf, sx + 37.0f, 39.5f, 0.32f, isCurM ? clrWhite : Ui::rgba(220, 215, 240), subModeNames[m], 0.30f);

                    if (touchJustDown && touch.px >= sx && touch.px <= sx + 74.0f && touch.py >= 29.0f && touch.py <= 49.0f) {
                        settingsControlsMode = m;
                        audio.playHitSound(2);
                    }
                }

                // 2. 4 Pastillas de acción para el modo activo
                const char* kShortNames[4];
                uint32_t kMasks[4];
                u32 actColors[4];

                if (settingsControlsMode == MODE_STD) {
                    kShortNames[0] = "K1";         kMasks[0] = settings.keyOsuK1;      actColors[0] = Ui::rgba(180, 70, 240);
                    kShortNames[1] = "K2";         kMasks[1] = settings.keyOsuK2;      actColors[1] = Ui::rgba(255, 95, 180);
                    kShortNames[2] = LOC(STR_KEY_SMOKE); kMasks[2] = settings.keyOsuSmoke;   actColors[2] = Ui::rgba(240, 200, 50);
                    kShortNames[3] = LOC(STR_KEY_RETRY); kMasks[3] = settings.keyQuickRetry; actColors[3] = Ui::rgba(255, 75, 75);
                } else if (settingsControlsMode == MODE_TAIKO) {
                    kShortNames[0] = LOC(STR_KEY_DON_L);    kMasks[0] = settings.keyTaikoDonLeft;    actColors[0] = Ui::rgba(245, 60, 65);
                    kShortNames[1] = LOC(STR_KEY_DON_R);    kMasks[1] = settings.keyTaikoDonRight;   actColors[1] = Ui::rgba(245, 60, 65);
                    kShortNames[2] = LOC(STR_KEY_KAT_L);  kMasks[2] = settings.keyTaikoKatsuLeft;  actColors[2] = Ui::rgba(45, 135, 255);
                    kShortNames[3] = LOC(STR_KEY_KAT_R);  kMasks[3] = settings.keyTaikoKatsuRight; actColors[3] = Ui::rgba(45, 135, 255);
                } else if (settingsControlsMode == MODE_CATCH) {
                    kShortNames[0] = LOC(STR_KEY_MOVE_L);  kMasks[0] = settings.keyCatchLeft;  actColors[0] = Ui::rgba(50, 210, 120);
                    kShortNames[1] = LOC(STR_KEY_MOVE_R);  kMasks[1] = settings.keyCatchRight; actColors[1] = Ui::rgba(50, 210, 120);
                    kShortNames[2] = LOC(STR_KEY_DASH);    kMasks[2] = settings.keyCatchDash;   actColors[2] = Ui::rgba(255, 175, 30);
                    kShortNames[3] = LOC(STR_KEY_RETRY);   kMasks[3] = settings.keyQuickRetry;  actColors[3] = Ui::rgba(255, 75, 75);
                } else {
                    kShortNames[0] = "Col 1";      kMasks[0] = settings.keyManiaCol1;  actColors[0] = Ui::rgba(50, 160, 255);
                    kShortNames[1] = "Col 2";      kMasks[1] = settings.keyManiaCol2;  actColors[1] = Ui::rgba(235, 235, 245);
                    kShortNames[2] = "Col 3";      kMasks[2] = settings.keyManiaCol3;  actColors[2] = Ui::rgba(235, 235, 245);
                    kShortNames[3] = "Col 4";      kMasks[3] = settings.keyManiaCol4;  actColors[3] = Ui::rgba(50, 160, 255);
                }

                for (int i = 0; i < 4; ++i) {
                    float bx = 6.0f + i * 77.0f;
                    float by = 50.0f;
                    float bw = 74.0f;
                    float bh = 22.0f;
                    int optIdx = 4 + i;
                    bool sel = (settingsOption == optIdx);
                    bool cap = (capturingKeyIndex == optIdx);

                    u32 pillBg = cap ? Ui::rgba(255, 180, 40) : (sel ? Ui::rgba(52, 40, 85, 250) : Ui::rgba(24, 20, 38, 230));
                    Ui::panel(bx, by, bw, bh, 0.15f, pillBg, cap ? clrWhite : (sel ? clrCursor : actColors[i]));
                    drawCenteredTextWithShadow(textBuf, bx + bw * 0.5f, by + 4.5f, 0.31f, cap ? clrBgDark : (sel ? clrWhite : actColors[i]), kShortNames[i], 0.85f);
                    std::string mStr = cap ? LOC(STR_PRESS_SHORT) : truncateString(GameSettings::formatKeyMask(kMasks[i]), 11);
                    drawCenteredTextWithShadow(textBuf, bx + bw * 0.5f, by + 14.5f, 0.28f, cap ? clrBgDark : (sel ? clrCursor : Ui::rgba(235, 230, 255)), mStr.c_str(), 0.85f);

                    if (touchJustDown && touch.px >= bx && touch.px <= bx + bw && touch.py >= by && touch.py <= by + bh) {
                        settingsOption = optIdx;
                        capturingKeyIndex = optIdx;
                        audio.playHitSound(2);
                    }
                }

                // 3. Dibujo estilizado de la consola Nintendo 3DS
                float chassisX = 22.0f, chassisY = 75.0f, chassisW = 276.0f, chassisH = 124.0f;
                Ui::panel(chassisX, chassisY, chassisW, chassisH, 0.12f, Ui::rgba(28, 24, 42, 250), Ui::rgba(65, 55, 90));
                C2D_DrawRectSolid(chassisX + 30.0f, chassisY + 1.0f, 0.13f, chassisW - 60.0f, 2.0f, Ui::rgba(45, 38, 65));

                // Pantalla interior de la 3DS
                float scX = 96.0f, scY = 85.0f, scW = 128.0f, scH = 86.0f;
                Ui::panel(scX, scY, scW, scH, 0.14f, Ui::rgba(10, 8, 16, 255), Ui::rgba(65, 55, 95));

                if (capturingKeyIndex != -1) {
                    int cIdx = std::max(0, std::min(3, capturingKeyIndex - 4));
                    Ui::glow(scX + scW * 0.5f, scY + scH * 0.5f, 0.15f, 32.0f, Ui::rgba(255, 180, 40, 60), 2);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 14.0f, 0.35f, clrCursor, LOC(STR_CTRL_REASSIGN_TITLE), 0.90f);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 32.0f, 0.40f, clrWhite, kShortNames[cIdx], 0.90f);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 50.0f, 0.33f, clr100, LOC(STR_CTRL_PRESS_ANY), 0.90f);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 68.0f, 0.30f, clrWhite, LOC(STR_CTRL_CANCEL_HINT), 0.90f);
                } else {
                    int selIdx = std::max(0, std::min(3, settingsOption - 4));
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 12.0f, 0.35f, subModeColors[settingsControlsMode], subModeNames[settingsControlsMode], 0.90f);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 28.0f, 0.36f, clrWhite, kShortNames[selIdx], 0.90f);
                    std::string curKeyFmt = GameSettings::formatKeyMask(kMasks[selIdx]);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 44.0f, 0.33f, clrCursor, curKeyFmt.c_str(), 0.90f);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 62.0f, 0.32f, Ui::rgba(140, 240, 255), LOC(STR_CTRL_REASSIGN_HINT), 0.90f);
                    drawCenteredTextWithShadow(textBuf, scX + scW * 0.5f, scY + 74.0f, 0.30f, clrWhite, LOC(STR_CTRL_REASSIGN_SUB), 0.90f);
                }

                auto getBtnClr = [&](uint32_t mask, u32 defClr) -> u32 {
                    if (kHeld & mask) return Ui::rgba(255, 255, 255);
                    for (int a = 0; a < 4; ++a) {
                        if (kMasks[a] & mask) return actColors[a];
                    }
                    return defClr;
                };

                // Gatillos L / R / ZL / ZR
                u32 clrL = getBtnClr(KEY_L, Ui::rgba(55, 48, 75));
                u32 clrR = getBtnClr(KEY_R, Ui::rgba(55, 48, 75));
                u32 clrZL = getBtnClr(KEY_ZL, Ui::rgba(45, 40, 62));
                u32 clrZR = getBtnClr(KEY_ZR, Ui::rgba(45, 40, 62));

                Ui::pill(28.0f, 67.0f, 42.0f, 12.0f, 0.16f, clrL);
                drawCenteredTextWithShadow(textBuf, 49.0f, 73.0f, 0.33f, clrWhite, "L", 0.85f);

                Ui::pill(250.0f, 67.0f, 42.0f, 12.0f, 0.16f, clrR);
                drawCenteredTextWithShadow(textBuf, 271.0f, 73.0f, 0.33f, clrWhite, "R", 0.85f);

                Ui::pill(72.0f, 68.0f, 26.0f, 11.0f, 0.16f, clrZL);
                drawCenteredTextWithShadow(textBuf, 85.0f, 73.5f, 0.29f, clrWhite, "ZL", 0.85f);

                Ui::pill(222.0f, 68.0f, 26.0f, 11.0f, 0.16f, clrZR);
                drawCenteredTextWithShadow(textBuf, 235.0f, 73.5f, 0.29f, clrWhite, "ZR", 0.85f);

                // Circle Pad
                C2D_DrawCircleSolid(60.0f, 108.0f, 0.15f, 14.0f, Ui::rgba(40, 35, 55));
                C2D_DrawCircleSolid(60.0f, 108.0f, 0.16f, 11.0f, Ui::rgba(80, 75, 100));
                drawRing(60.0f, 108.0f, 7.0f, 1.2f, Ui::rgba(55, 50, 75), 16, 0.17f);
                C2D_DrawCircleSolid(60.0f, 108.0f, 0.18f, 2.0f, Ui::rgba(55, 50, 75));

                // D-Pad (Cruz direccional)
                u32 clrDUP = getBtnClr(KEY_DUP, Ui::rgba(55, 48, 75));
                u32 clrDDOWN = getBtnClr(KEY_DDOWN, Ui::rgba(55, 48, 75));
                u32 clrDLEFT = getBtnClr(KEY_DLEFT, Ui::rgba(55, 48, 75));
                u32 clrDRIGHT = getBtnClr(KEY_DRIGHT, Ui::rgba(55, 48, 75));

                C2D_DrawRectSolid(55.0f, 137.0f, 0.16f, 10.0f, 30.0f, Ui::rgba(35, 30, 50));
                C2D_DrawRectSolid(45.0f, 147.0f, 0.16f, 30.0f, 10.0f, Ui::rgba(35, 30, 50));

                C2D_DrawRectSolid(55.0f, 137.0f, 0.17f, 10.0f, 10.0f, clrDUP);
                C2D_DrawRectSolid(55.0f, 157.0f, 0.17f, 10.0f, 10.0f, clrDDOWN);
                C2D_DrawRectSolid(45.0f, 147.0f, 0.17f, 10.0f, 10.0f, clrDLEFT);
                C2D_DrawRectSolid(65.0f, 147.0f, 0.17f, 10.0f, 10.0f, clrDRIGHT);

                drawCenteredTextWithShadow(textBuf, 60.0f, 140.0f, 0.26f, clrWhite, "^", 0.85f);
                drawCenteredTextWithShadow(textBuf, 60.0f, 160.0f, 0.26f, clrWhite, "v", 0.85f);
                drawCenteredTextWithShadow(textBuf, 50.0f, 150.0f, 0.26f, clrWhite, "<", 0.85f);
                drawCenteredTextWithShadow(textBuf, 70.0f, 150.0f, 0.26f, clrWhite, ">", 0.85f);

                // C-Stick (N3DS)
                C2D_DrawCircleSolid(238.0f, 104.0f, 0.16f, 5.0f, Ui::rgba(85, 80, 105));

                // ABXY Diamond
                u32 clrX = getBtnClr(KEY_X, Ui::rgba(60, 100, 200));
                u32 clrB = getBtnClr(KEY_B, Ui::rgba(210, 175, 40));
                u32 clrY = getBtnClr(KEY_Y, Ui::rgba(60, 180, 80));
                u32 clrA = getBtnClr(KEY_A, Ui::rgba(220, 55, 65));

                C2D_DrawCircleSolid(258.0f, 124.0f, 0.17f, 8.0f, clrX);
                drawCenteredTextWithShadow(textBuf, 258.0f, 124.0f, 0.34f, clrWhite, "X", 0.85f);

                C2D_DrawCircleSolid(258.0f, 156.0f, 0.17f, 8.0f, clrB);
                drawCenteredTextWithShadow(textBuf, 258.0f, 156.0f, 0.34f, clrWhite, "B", 0.85f);

                C2D_DrawCircleSolid(242.0f, 140.0f, 0.17f, 8.0f, clrY);
                drawCenteredTextWithShadow(textBuf, 242.0f, 140.0f, 0.34f, clrWhite, "Y", 0.85f);

                C2D_DrawCircleSolid(274.0f, 140.0f, 0.17f, 8.0f, clrA);
                drawCenteredTextWithShadow(textBuf, 274.0f, 140.0f, 0.34f, clrWhite, "A", 0.85f);

                // START / SELECT / HOME
                u32 clrSel = getBtnClr(KEY_SELECT, Ui::rgba(50, 44, 70));
                u32 clrStr = getBtnClr(KEY_START, Ui::rgba(50, 44, 70));

                Ui::pill(116.0f, 176.0f, 24.0f, 9.0f, 0.16f, clrSel);
                drawCenteredTextWithShadow(textBuf, 128.0f, 180.5f, 0.28f, clrWhite, "SEL", 0.85f);

                Ui::pill(148.0f, 176.0f, 24.0f, 9.0f, 0.16f, Ui::rgba(40, 36, 58));
                drawCenteredTextWithShadow(textBuf, 160.0f, 180.5f, 0.27f, Ui::rgba(210, 205, 230), "HOME", 0.85f);

                Ui::pill(180.0f, 176.0f, 24.0f, 9.0f, 0.16f, clrStr);
                drawCenteredTextWithShadow(textBuf, 192.0f, 180.5f, 0.28f, clrWhite, "STR", 0.85f);
            } else { // Pestañas Audio (2), Juego (3) o Visual (4): tarjetas táctiles para ajustar con stylus
                int count = tabItemCounts[activeTab];
                float bCardH = (count == 6) ? 25.0f : ((count == 5) ? 30.0f : 37.0f);
                float bCardStep = (count == 6) ? 28.0f : ((count == 5) ? 33.0f : 41.0f);
                float bStartY = (count == 6) ? 32.0f : ((count == 5) ? 33.0f : 36.0f);
                float bTitleYOff = (count == 6) ? 1.0f : ((count == 5) ? 2.0f : 5.0f);
                float bDescYOff  = (count == 6) ? 13.5f : ((count == 5) ? 15.5f : 20.0f);
                float bSwYOff    = (count == 6) ? 3.5f : ((count == 5) ? 6.0f : 10.0f);
                float bSldYOff   = (count == 6) ? 8.0f : ((count == 5) ? 10.0f : 14.0f);
                float bPillYOff  = (count == 6) ? 3.5f : ((count == 5) ? 5.0f : 8.0f);

                for (int i = 0; i < count; ++i) {
                    int optIdx = tabItems[activeTab][i];
                    float cy = bStartY + i * bCardStep;
                    bool sel = (settingsOption == optIdx);
                    Ui::panel(10.0f, cy, 300.0f, bCardH, 0.15f, sel ? Ui::rgba(42, 34, 70, 240) : Ui::rgba(22, 19, 36, 220), sel ? clrCursor : 0);

                    // Detección de toque en la fila
                    if (isTouching && touch.px >= 10 && touch.px <= 310 && touch.py >= cy && touch.py <= cy + bCardH) {
                        settingsOption = optIdx;
                        if (optIdx == 8) { // Música
                            settings.musicVolume = std::max(0, std::min(100, (int)(((touch.px - 180.0f) / 115.0f) * 100.0f)));
                            audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                        } else if (optIdx == 9) { // SFX
                            settings.sfxVolume = std::max(0, std::min(100, (int)(((touch.px - 180.0f) / 115.0f) * 100.0f)));
                            audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                        } else if (optIdx == 10) { // Offset
                            settings.audioOffsetMs = std::max(-150, std::min(150, (int)(((touch.px - 180.0f) / 115.0f) * 300.0f - 150.0f)));
                        } else if (optIdx == 11 && touchJustDown) { // Hitsounds Beatmap
                            settings.beatmapHitsounds = !settings.beatmapHitsounds;
                        } else if (optIdx == 22 && touchJustDown) { // Idioma / Language
                            settings.language = (settings.language + 1) % LANG_COUNT;
                            Localization::setLanguage(static_cast<AppLanguage>(settings.language));
                            settings.save();
                            audio.playHitSound(2);
                        } else if (optIdx == 12 && touchJustDown) { // Modo de Juego
                            settings.gameMode = static_cast<GameMode>((settings.gameMode + 1) % 4);
                            settings.save();
                        } else if (optIdx == 13 && touchJustDown) { // Skin
                            settings.skinIndex = (settings.skinIndex + 1) % std::max(1, (int)skinMgr.skins.size());
                            applyActiveSkinAudio();
                        } else if (optIdx == 14 && touchJustDown) { // Sliders Serpiente
                            settings.snakingSliders = !settings.snakingSliders;
                        } else if (optIdx == 15 && touchJustDown) { // Combo Bursts
                            settings.comboBursts = !settings.comboBursts;
                        } else if (optIdx == 20 && touchJustDown) { // Efecto Golpes 300
                            settings.showHit300 = !settings.showHit300;
                            audio.playHitSound(0);
                        } else if (optIdx == 16 && touchJustDown) { // Fondo Taiko
                            settings.taikoBgMode = (settings.taikoBgMode + 1) % 3;
                            settings.save();
                        } else if (optIdx == 17) { // Cursor
                            settings.cursorScale = std::max(0.5f, std::min(2.0f, 0.5f + ((touch.px - 180.0f) / 115.0f) * 1.5f));
                        } else if (optIdx == 18) { // BgDim
                            settings.bgDim = std::max(20, std::min(100, (int)(20 + ((touch.px - 180.0f) / 115.0f) * 80.0f)));
                        } else if (optIdx == 19 && touchJustDown) { // Videos
                            settings.enableVideo = !settings.enableVideo;
                        } else if (optIdx == 21 && touchJustDown) { // Descargas Multijugador
                            settings.multiDlOption = (settings.multiDlOption + 1) % 2;
                            multiMgr.enableBgDownload = (settings.multiDlOption != 0);
                            settings.save();
                        }
                        settings.clampValues();
                    }

                    if (optIdx == 8) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_MUSIC_VOL));
                        char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", settings.musicVolume);
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, desc);
                        Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, settings.musicVolume / 100.0f, clrCursor, sel);
                    } else if (optIdx == 9) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_SFX_VOL));
                        char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", settings.sfxVolume);
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, desc);
                        Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, settings.sfxVolume / 100.0f, clrCursor, sel);
                    } else if (optIdx == 10) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_OFFSET));
                        char desc[32]; std::snprintf(desc, sizeof(desc), "%+d ms", settings.audioOffsetMs);
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr100, desc);
                        float r = (settings.audioOffsetMs + 150) / 300.0f;
                        Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, r, clr300, sel);
                    } else if (optIdx == 11) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_MAP_HITSOUNDS));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clrMuted, settings.beatmapHitsounds ? LOC(STR_ON) : LOC(STR_OFF));
                        Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, settings.beatmapHitsounds, clr100);
                    } else if (optIdx == 22) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_LANGUAGE));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, settings.getLanguageName());
                        u32 lPill = Ui::rgba(45, 115, 215);
                        Ui::pill(232.0f, cy + bPillYOff, 68.0f, (count == 6) ? 18.0f : 20.0f, 0.25f, lPill);
                        drawCenteredText(textBuf, 266.0f, cy + bPillYOff + ((count == 6) ? 9.0f : 10.0f), 0.33f, clrWhite, settings.getLanguageName(), 0.85f);
                    } else if (optIdx == 12) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_MAIN_MODE));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, settings.getGameModeName());
                        u32 mPill = (settings.gameMode == MODE_TAIKO) ? Ui::rgba(180, 45, 45) : ((settings.gameMode == MODE_CATCH) ? Ui::rgba(35, 160, 85) : ((settings.gameMode == MODE_MANIA) ? Ui::rgba(140, 35, 175) : Ui::rgba(55, 40, 95)));
                        Ui::pill(232.0f, cy + bPillYOff, 68.0f, 20.0f, 0.25f, mPill);
                        drawCenteredText(textBuf, 266.0f, cy + bPillYOff + 10.0f, 0.33f, clrWhite, settings.getGameModeShortName(), 0.85f);
                    } else if (optIdx == 13) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_ACTIVE_SKIN));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, truncateString(activeSkin.name, 20).c_str());
                        for (size_t c = 0; c < std::min((size_t)4, activeSkin.comboColors.size()); ++c) {
                            u32 cc = C2D_Color32(activeSkin.comboColors[c].r, activeSkin.comboColors[c].g, activeSkin.comboColors[c].b, 255);
                            C2D_DrawCircleSolid(225.0f + c * 18.0f, cy + bPillYOff + 10.0f, 0.25f, 6.0f, cc);
                        }
                    } else if (optIdx == 14) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_SNAKING));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clrMuted, settings.snakingSliders ? LOC(STR_ON) : LOC(STR_OFF));
                        Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, settings.snakingSliders, clr100);
                    } else if (optIdx == 15) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_BURSTS));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clrMuted, settings.comboBursts ? LOC(STR_ON) : LOC(STR_OFF));
                        Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, settings.comboBursts, clr100);
                    } else if (optIdx == 20) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_HIT300));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clrMuted, settings.showHit300 ? LOC(STR_ON) : LOC(STR_OFF));
                        Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, settings.showHit300, clr100);
                    } else if (optIdx == 16) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_TAIKO_BG));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, settings.getTaikoBgModeName());
                        u32 bClr = (settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25) : ((settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(165, 55, 185));
                        Ui::pill(180.0f, cy + bPillYOff + 1.0f, 115.0f, 20.0f, 0.25f, bClr);
                        drawCenteredText(textBuf, 237.5f, cy + bPillYOff + 11.0f, 0.32f, clrWhite, settings.getTaikoBgModeName(), 0.35f);
                    } else if (optIdx == 17) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_CURSOR_SIZE));
                        char desc[32]; std::snprintf(desc, sizeof(desc), "%.1fx", settings.cursorScale);
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, desc);
                        float r = (settings.cursorScale - 0.5f) / 1.5f;
                        Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, r, clrCursor, sel);
                    } else if (optIdx == 18) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_BG_DIM));
                        char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", settings.bgDim);
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, desc);
                        float r = (settings.bgDim - 20) / 80.0f;
                        Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, r, clrPrimary, sel);
                    } else if (optIdx == 19) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_VIDEOS));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clrMuted, settings.enableVideo ? LOC(STR_ON) : LOC(STR_OFF));
                        Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, settings.enableVideo, clr100);
                    } else if (optIdx == 21) {
                        drawText(textBuf, 20.0f, cy + bTitleYOff, 0.40f, clrWhite, LOC(STR_SET_MULTI_DL));
                        drawText(textBuf, 20.0f, cy + bDescYOff, 0.33f, clr300, settings.getMultiDlOptionName());
                        u32 dClr = (settings.multiDlOption == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(110, 95, 140);
                        Ui::pill(175.0f, cy + bPillYOff + 1.0f, 120.0f, 20.0f, 0.25f, dClr);
                        drawCenteredText(textBuf, 235.0f, cy + bPillYOff + 11.0f, 0.30f, clrWhite, settings.getMultiDlOptionName(), 0.35f);
                    }
                }
            }

            if (capturingKeyIndex != -1 && activeTab != 1) {
                C2D_DrawRectSolid(0.0f, 0.0f, 0.88f, 320.0f, 240.0f, Ui::rgba(8, 6, 16, 215));
                Ui::panel(20.0f, 50.0f, 280.0f, 140.0f, 0.90f, Ui::rgba(26, 20, 44, 250), clrCursor);
                const char* capNames[4] = { LOC(STR_KEY_K1), LOC(STR_KEY_K2), LOC(STR_KEY_SMOKE), LOC(STR_KEY_RETRY) };
                int cIdx = std::max(0, std::min(3, capturingKeyIndex - 4));
                drawCenteredText(textBuf, 160.0f, 72.0f, 0.48f, clrCursor, LOC(STR_CTRL_ASSIGN_KEY_TITLE), 0.95f);
                drawCenteredText(textBuf, 160.0f, 96.0f, 0.42f, clrWhite, capNames[cIdx], 0.95f);
                drawCenteredText(textBuf, 160.0f, 126.0f, 0.36f, clr100, LOC(STR_CTRL_PRESS_ANY), 0.95f);
                drawCenteredText(textBuf, 160.0f, 154.0f, 0.32f, clrMuted, LOC(STR_CTRL_CANCEL_HINT), 0.95f);
                if (touchJustDown) {
                    capturingKeyIndex = -1;
                    audio.playHitSound(0);
                }
            }

            // Barra inferior con 3 botones de acción estilo multijugador
            Ui::button(10.0f, 204.0f, 96.0f, 28.0f, 0.20f, Ui::rgba(20, 36, 60, 240), Ui::rgba(40, 180, 240), false);
            Ui::pill(15.0f, 209.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(40, 180, 240));
            drawCenteredText(textBuf, 24.0f, 218.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "X", 0.28f);
            drawTextWithShadow(textBuf, 37.0f, 211.0f, 0.33f, clrWhite, (activeTab == 1) ? LOC(STR_DEFAULT) : LOC(STR_CALIBRATE), 0.28f);

            Ui::button(112.0f, 204.0f, 96.0f, 28.0f, 0.20f, Ui::rgba(45, 35, 20, 240), Ui::rgba(255, 180, 40), false);
            Ui::pill(117.0f, 209.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(255, 180, 40));
            drawCenteredText(textBuf, 126.0f, 218.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "Y", 0.28f);
            drawTextWithShadow(textBuf, 140.0f, 211.0f, 0.34f, clrWhite, LOC(STR_RESET), 0.28f);

            Ui::button(214.0f, 204.0f, 96.0f, 28.0f, 0.20f, Ui::rgba(42, 20, 52, 240), clrPrimary, true);
            Ui::pill(219.0f, 209.0f, 18.0f, 18.0f, 0.24f, clrPrimary);
            drawCenteredText(textBuf, 228.0f, 218.0f, 0.30f, clrWhite, "B", 0.28f);
            drawTextWithShadow(textBuf, 242.0f, 211.0f, 0.34f, clrWhite, LOC(STR_SAVE), 0.28f);

            if (isTouching) {
                Ui::glow(touch.px, touch.py, 0.75f, 11.0f, Ui::withAlpha(clr300, 140), 3);
                C2D_DrawCircleSolid(touch.px, touch.py, 0.8f, 4.5f, clr300);
            }
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 3. SELECTOR DE CANCIONES, RECORDS Y SELECTOR DE MODS
        // =========================================================
        if (appState == STATE_SONG_SELECT) {
            audio.update();
            float beatPulse = getBpmPulse();

            // Debounce inteligente: solo cargar fondo y audio cuando el carrusel se ha detenido visualmente
            // y el usuario ha dejado de desplazarse entre canciones
            bool carouselSettled = (std::fabs(carouselPos - static_cast<float>(selectedSet)) < 0.05f);
            if (!carouselSettled) {
                // Mientras el carrusel se está moviendo con suavidad, aplazamos la carga
                // para que el hilo de renderizado fluya a 60 FPS continuos sin competir por I/O
                if (previewDelay < (isNew3DS ? 10 : 16)) {
                    previewDelay = isNew3DS ? 10 : 16;
                }
            } else if (previewDelay > 0) {
                if (--previewDelay == 0) {
                    updateSongPreview();
                }
            }

            // Subir a GPU el fondo decodificado en segundo plano (si ya esta listo)
            if (bgLoader.poll(bgTex)) bgFade = 0.0f;
            bgFade = Ui::approach(bgFade, 255.0f, 0.12f);
            if (settings.songPreview && audio.isSongFinished() && !audio.isPreviewLoading()) {
                updateSongPreview();
            }

            // Animación suave de elevación e iluminación de las tarjetas inferiores estilo osu!lazer
            footerCardHover = -1;
            if (isTouching && !modMenuOpen && !deleteConfirmOpen && !rankingExpanded && !searchKeyboardOpen && touch.py >= 136 && touch.py <= 236) {
                for (int k = 0; k < 5; ++k) {
                    float bx = 8.0f + k * 62.0f;
                    if (touch.px >= bx && touch.px <= bx + 56.0f) {
                        footerCardHover = k;
                        break;
                    }
                }
            }
            for (int k = 0; k < 5; ++k) {
                float targetLift = 0.0f;
                if (footerCardHover == k) targetLift = 1.0f;
                else if (k == 2 && modMenuOpen) targetLift = 0.85f;
                else if (k == 1 && !songMgr.searchQuery.empty()) targetLift = 0.25f + 0.15f * std::sin(animTick * 0.12f);
                footerCardLift[k] = Ui::approach(footerCardLift[k], targetLift, 0.25f);
            }

            // Abrir/Cerrar menú de Mods con SELECT o botón táctil en pantalla inferior
            if (kDown & KEY_SELECT && !deleteConfirmOpen && !searchKeyboardOpen) {
                modMenuOpen = !modMenuOpen;
                audio.playHitSound(0);
            }
            if (touchJustDown && !modMenuOpen && !deleteConfirmOpen && !searchKeyboardOpen) {
                // Tocar botón pequeño [BORRAR] en el header (x: 246..312, y: 6..28) (Deshabilitado en multijugador)
                if (touch.py >= 6 && touch.py <= 28 && touch.px >= 246 && touch.px <= 312) {
                    if (!multiMgr.inMultiplayer && !songMgr.sets.empty()) {
                        deleteConfirmOpen = true;
                        audio.playHitSound(0);
                    }
                }
                // Tocar la dificultad / estrellas para alternar a la siguiente dificultad del mapa
                else if (touch.py >= 32 && touch.py <= 54 && touch.px >= 8 && touch.px <= 148) {
                    if (!songMgr.sets.empty()) {
                        const auto& curSet = songMgr.sets[selectedSet];
                        if (curSet.difficulties.size() > 1) {
                            selectedDiff = (selectedDiff + 1) % curSet.difficulties.size();
                            audio.playHitSound(0);
                            previewDelay = isNew3DS ? 6 : 10;
                            updateSongPreview();
                        }
                    }
                }
                // Tocar pestañas [ RESUMEN ] / [ RANKING LOCAL ] o el botón de acción
                else if (touch.py >= 50 && touch.py <= 72) {
                    if (touch.px >= 8 && touch.px <= 118) {
                        songSelectTab = 0;
                        rankingExpanded = false;
                        audio.playHitSound(0);
                    } else if (touch.px >= 122 && touch.px <= 244) {
                        songSelectTab = 1;
                        audio.playHitSound(0);
                    } else if (touch.px >= 246 && touch.px <= 312) {
                        if (songSelectTab == 1) {
                            rankingExpanded = !rankingExpanded;
                            audio.playHitSound(2);
                        } else {
                            songSelectTab = 1;
                            audio.playHitSound(0);
                        }
                    }
                }
                // Si está en RESUMEN y toca la tarjeta de Personal Best, abrir Ranking Local
                else if (songSelectTab == 0 && touch.py >= 72 && touch.py <= 112 && touch.px >= 8 && touch.px <= 312) {
                    songSelectTab = 1;
                    audio.playHitSound(0);
                }
                // Si está en RANKING LOCAL (modo compacto) y toca la lista, expandirla
                else if (songSelectTab == 1 && !rankingExpanded && touch.py >= 72 && touch.py <= 142 && touch.px >= 8 && touch.px <= 312) {
                    rankingExpanded = true;
                    audio.playHitSound(0);
                }
                // Si está en modo expandido y toca la barra inferior de cerrar
                else if (rankingExpanded && touch.py >= 220 && touch.py <= 240) {
                    rankingExpanded = false;
                    audio.playHitSound(0);
                }

                // Solo procesar las 5 tarjetas inferiores si NO está el ranking expandido
                if (!rankingExpanded) {
                    for (int k = 0; k < 5; ++k) {
                        float bx = 8.0f + k * 62.0f;
                        if (touch.px >= bx && touch.px <= bx + 56.0f && touch.py >= 136 && touch.py <= 236) {
                            footerCardLift[k] = 1.0f;
                            if (k == 0) { // Alternar Modo Táctil / Tablet en la misma tarjeta
                                if (!multiMgr.inMultiplayer) {
                                    settings.playOnBottomScreen = !settings.playOnBottomScreen;
                                    settings.save();
                                    audio.playHitSound(2);
                                }
                            } else if (k == 1) { // Buscar
                                openSearchKeyboard();
                                touchJustDown = false;
                                break;
                            } else if (k == 2) { // Mods
                                modMenuOpen = true;
                                audio.playHitSound(0);
                            } else if (k == 3) { // Orden
                                songMgr.sortMode = (songMgr.sortMode + 1) % 4;
                                songMgr.applySortAndFilter(&scoreMgr);
                                selectedSet = 0;
                                selectedDiff = 0;
                                rankingScroll = 0;
                                carouselPos = 0.0f;
                                audio.playHitSound(2);
                                updateSongPreview();
                            } else if (k == 4) { // Modo de Juego (STD / TAIKO / CATCH / MANIA)
                                if (!multiMgr.inMultiplayer) {
                                    if (settings.gameMode == MODE_STD) settings.gameMode = MODE_TAIKO;
                                    else if (settings.gameMode == MODE_TAIKO) settings.gameMode = MODE_CATCH;
                                    else if (settings.gameMode == MODE_CATCH) settings.gameMode = MODE_MANIA;
                                    else settings.gameMode = MODE_STD;
                                    settings.save();
                                    audio.playHitSound(2);
                                }
                            }
                            break;
                        }
                    }
                }
            }

            if (deleteConfirmOpen) {
                if (kDown & KEY_B) {
                    deleteConfirmOpen = false;
                    audio.playHitSound(0);
                }
                if (kDown & KEY_A) {
                    if (!songMgr.sets.empty() && selectedSet >= 0 && selectedSet < static_cast<int>(songMgr.sets.size())) {
                        audio.stopMusic();
                        songMgr.deleteSet(selectedSet, &scoreMgr);
                        if (selectedSet >= static_cast<int>(songMgr.sets.size())) {
                            selectedSet = std::max(0, static_cast<int>(songMgr.sets.size()) - 1);
                        }
                        selectedDiff = 0;
                        carouselPos = static_cast<float>(selectedSet);
                        audio.playHitSound(2);
                        updateSongPreview();
                    }
                    deleteConfirmOpen = false;
                }
                if (touchJustDown) {
                    if (touch.px >= 28 && touch.px <= 154 && touch.py >= 146 && touch.py <= 184) { // CANCELAR
                        deleteConfirmOpen = false;
                        audio.playHitSound(0);
                    } else if (touch.px >= 166 && touch.px <= 292 && touch.py >= 146 && touch.py <= 184) { // BORRAR
                        if (!songMgr.sets.empty() && selectedSet >= 0 && selectedSet < static_cast<int>(songMgr.sets.size())) {
                            audio.stopMusic();
                            songMgr.deleteSet(selectedSet, &scoreMgr);
                            if (selectedSet >= static_cast<int>(songMgr.sets.size())) {
                                selectedSet = std::max(0, static_cast<int>(songMgr.sets.size()) - 1);
                            }
                            selectedDiff = 0;
                            carouselPos = static_cast<float>(selectedSet);
                            audio.playHitSound(2);
                            updateSongPreview();
                        }
                        deleteConfirmOpen = false;
                    }
                }
            } else if (modMenuOpen) {
                if (kDown & (KEY_B | KEY_SELECT)) {
                    modMenuOpen = false;
                    audio.playHitSound(0);
                    updateSongPreview();
                }
                if (kDown & KEY_DLEFT) {
                    if (modCursor % 5 == 0) modCursor += 4;
                    else modCursor -= 1;
                    audio.playHitSound(0);
                }
                if (kDown & KEY_DRIGHT) {
                    if (modCursor % 5 == 4) modCursor -= 4;
                    else modCursor += 1;
                    audio.playHitSound(0);
                }
                if (kDown & KEY_DUP) {
                    modCursor = (modCursor + 10) % 15;
                    audio.playHitSound(0);
                }
                if (kDown & KEY_DDOWN) {
                    modCursor = (modCursor + 5) % 15;
                    audio.playHitSound(0);
                }

                if (kDown & KEY_A) {
                    toggleModByIndex(modCursor);
                    audio.playHitSound(2);
                    updateSongPreview();
                }
                if (kDown & KEY_Y) {
                    settings.activeMods = MOD_NONE;
                    if (multiMgr.inMultiplayer) {
                        multiMgr.players[multiMgr.localSlot].activeMods = MOD_NONE;
                        if (!multiMgr.isHost()) multiMgr.sendPlayerUpdate();
                        else multiMgr.broadcastLobbyState();
                    }
                    settings.save();
                    audio.playHitSound(0);
                    updateSongPreview();
                }

                // Soporte táctil para las 15 tarjetas de Mods en la cuadrícula 5x3
                if (touchJustDown) {
                    for (int m = 0; m < 15; ++m) {
                        int col = m % 5;
                        int row = m / 5;
                        float mx = 8.0f + col * 62.0f;
                        float my = 44.0f + row * 44.0f;
                        if (touch.px >= mx && touch.px <= mx + 56.0f &&
                            touch.py >= my && touch.py <= my + 38.0f) {
                            modCursor = m;
                            toggleModByIndex(m);
                            audio.playHitSound(2);
                            updateSongPreview();
                            break;
                        }
                    }
                    // Botón Limpiar (esquina superior derecha) o volver
                    if (touch.px >= 220 && touch.px <= 312 && touch.py >= 6 && touch.py <= 36) {
                        settings.activeMods = MOD_NONE;
                        if (multiMgr.inMultiplayer) {
                            multiMgr.players[multiMgr.localSlot].activeMods = MOD_NONE;
                            if (!multiMgr.isHost()) multiMgr.sendPlayerUpdate();
                            else multiMgr.broadcastLobbyState();
                        }
                        settings.save();
                        audio.playHitSound(0);
                        updateSongPreview();
                    }
                }
            } else if (searchKeyboardOpen) {
                if (searchKeyFlashTimer > 0) searchKeyFlashTimer--;

                // Botones físicos cuando el teclado está abierto
                if (kDown & (KEY_B | KEY_START)) {
                    searchKeyboardOpen = false;
                    audio.playHitSound(0);
                }
                else if (kDown & KEY_X) {
                    songMgr.searchQuery.clear();
                    songMgr.applySortAndFilter(&scoreMgr);
                    selectedSet = 0;
                    selectedDiff = 0;
                    rankingScroll = 0;
                    carouselPos = 0.0f;
                    audio.playHitSound(0);
                    updateSongPreview();
                }
                else if (kDown & KEY_Y) {
                    if (!songMgr.searchQuery.empty()) {
                        songMgr.searchQuery.pop_back();
                        songMgr.applySortAndFilter(&scoreMgr);
                        selectedSet = 0;
                        selectedDiff = 0;
                        rankingScroll = 0;
                        carouselPos = 0.0f;
                        audio.playHitSound(0);
                        updateSongPreview();
                    }
                }
                else if (kDown & (KEY_ZL | KEY_ZR)) {
                    searchKeyboardOpen = false;
                    audio.playHitSound(0);
                }
                else if (kDown & KEY_A) {
                    searchKeyboardOpen = false;
                    audio.playHitSound(2);
                }

                // Navegar por la lista filtrada de canciones mientras el teclado está abierto
                if (!songMgr.sets.empty()) {
                    int numSets = static_cast<int>(songMgr.sets.size());
                    int setDelta = 0;
                    if (kDown & (KEY_DUP | KEY_L))   setDelta = -1;
                    if (kDown & (KEY_DDOWN | KEY_R)) setDelta = +1;
                    if (setDelta == 0 && analogNavCooldown == 0) {
                        if (cstick.dy > 45 || cpad.dy > 65) {
                            setDelta = -1;
                            analogNavCooldown = 11;
                        } else if (cstick.dy < -45 || cpad.dy < -65) {
                            setDelta = +1;
                            analogNavCooldown = 11;
                        }
                    }
                    if (setDelta != 0) {
                        selectedSet = (selectedSet + setDelta + numSets) % numSets;
                        selectedDiff = 0;
                        rankingScroll = 0;
                        carouselPos = static_cast<float>(selectedSet);
                        audio.playHitSound(0);
                        previewDelay = isNew3DS ? 10 : 16;
                    }
                }

                // Mantener pulsado el botón de borrar para repetición rápida
                bool touchingDelete = isTouching && (
                    (touch.px >= 271.0f && touch.px <= 316.0f && touch.py >= 54.0f && touch.py <= 86.0f) ||
                    (touch.px >= 208.0f && touch.px <= 260.0f && touch.py >= 194.0f && touch.py <= 236.0f)
                );
                if (touchingDelete) {
                    searchBackspaceHoldTimer++;
                    if (searchBackspaceHoldTimer > 20 && (searchBackspaceHoldTimer % 4 == 0)) {
                        if (!songMgr.searchQuery.empty()) {
                            songMgr.searchQuery.pop_back();
                            songMgr.applySortAndFilter(&scoreMgr);
                            selectedSet = 0;
                            selectedDiff = 0;
                            rankingScroll = 0;
                            carouselPos = 0.0f;
                            audio.playHitSound(0);
                            updateSongPreview();
                        }
                    }
                } else {
                    searchBackspaceHoldTimer = 0;
                }

                // Entrada táctil por pulsación única
                if (touchJustDown) {
                    // 1. Cabecera: Limpiar [✕] o Listo [LISTO]
                    if (touch.px >= 232.0f && touch.px <= 258.0f && touch.py >= 3.0f && touch.py <= 31.0f) {
                        songMgr.searchQuery.clear();
                        songMgr.applySortAndFilter(&scoreMgr);
                        selectedSet = 0;
                        selectedDiff = 0;
                        rankingScroll = 0;
                        carouselPos = 0.0f;
                        audio.playHitSound(0);
                        updateSongPreview();
                    } else if (touch.px >= 262.0f && touch.px <= 316.0f && touch.py >= 3.0f && touch.py <= 31.0f) {
                        searchKeyboardOpen = false;
                        audio.playHitSound(2);
                    }
                    // 2. Fila de Filtros Rápidos (Chips) en y: 34..52
                    else if (touch.py >= 34.0f && touch.py <= 52.0f) {
                        if (touch.px >= 4.0f && touch.px <= 44.0f) { // TODOS
                            songMgr.searchQuery.clear();
                            songMgr.applySortAndFilter(&scoreMgr);
                            selectedSet = 0;
                            selectedDiff = 0;
                            rankingScroll = 0;
                            carouselPos = 0.0f;
                            audio.playHitSound(2);
                            updateSongPreview();
                        } else if (touch.px >= 47.0f && touch.px <= 79.0f) { // STD
                            toggleSearchTag("mode:std");
                        } else if (touch.px >= 82.0f && touch.px <= 122.0f) { // TAIKO
                            toggleSearchTag("mode:taiko");
                        } else if (touch.px >= 125.0f && touch.px <= 165.0f) { // CATCH
                            toggleSearchTag("mode:catch");
                        } else if (touch.px >= 168.0f && touch.px <= 208.0f) { // MANIA
                            toggleSearchTag("mode:mania");
                        } else if (touch.px >= 211.0f && touch.px <= 243.0f) { // ★4+
                            toggleSearchTag("★4");
                        } else if (touch.px >= 246.0f && touch.px <= 278.0f) { // ★5+
                            toggleSearchTag("★5");
                        }
                    }
                    // 3. Fila 0: Números 1..0 y Borrar superior (y: 54..86)
                    else if (touch.py >= 54.0f && touch.py <= 86.0f) {
                        bool hit = false;
                        const char* numChars = "1234567890";
                        for (int i = 0; i < 10; ++i) {
                            float kx = 4.0f + i * 26.5f;
                            if (touch.px >= kx && touch.px <= kx + 24.5f) {
                                if (songMgr.searchQuery.size() < 32) {
                                    songMgr.searchQuery += numChars[i];
                                    searchLastKeyIndex = i;
                                    searchKeyFlashTimer = 6;
                                    songMgr.applySortAndFilter(&scoreMgr);
                                    selectedSet = 0;
                                    selectedDiff = 0;
                                    rankingScroll = 0;
                                    carouselPos = 0.0f;
                                    audio.playHitSound(0);
                                    updateSongPreview();
                                }
                                hit = true;
                                break;
                            }
                        }
                        if (!hit && touch.px >= 271.0f && touch.px <= 316.0f) {
                            if (!songMgr.searchQuery.empty()) {
                                songMgr.searchQuery.pop_back();
                                searchLastKeyIndex = 10;
                                searchKeyFlashTimer = 6;
                                songMgr.applySortAndFilter(&scoreMgr);
                                selectedSet = 0;
                                selectedDiff = 0;
                                rankingScroll = 0;
                                carouselPos = 0.0f;
                                audio.playHitSound(0);
                                updateSongPreview();
                            }
                        }
                    }
                    // 4. Fila 1: Q W E R T Y U I O P (y: 89..121)
                    else if (touch.py >= 89.0f && touch.py <= 121.0f) {
                        const char* r1Chars = "qwertyuiop";
                        for (int i = 0; i < 10; ++i) {
                            float kx = 4.0f + i * 31.0f;
                            if (touch.px >= kx && touch.px <= kx + 29.0f) {
                                if (songMgr.searchQuery.size() < 32) {
                                    songMgr.searchQuery += r1Chars[i];
                                    searchLastKeyIndex = 11 + i;
                                    searchKeyFlashTimer = 6;
                                    songMgr.applySortAndFilter(&scoreMgr);
                                    selectedSet = 0;
                                    selectedDiff = 0;
                                    rankingScroll = 0;
                                    carouselPos = 0.0f;
                                    audio.playHitSound(0);
                                    updateSongPreview();
                                }
                                break;
                            }
                        }
                    }
                    // 5. Fila 2: A S D F G H J K L - (y: 124..156)
                    else if (touch.py >= 124.0f && touch.py <= 156.0f) {
                        const char* r2Chars = "asdfghjkl-";
                        for (int i = 0; i < 10; ++i) {
                            float kx = 8.0f + i * 30.2f;
                            if (touch.px >= kx && touch.px <= kx + 28.2f) {
                                if (songMgr.searchQuery.size() < 32) {
                                    songMgr.searchQuery += r2Chars[i];
                                    searchLastKeyIndex = 21 + i;
                                    searchKeyFlashTimer = 6;
                                    songMgr.applySortAndFilter(&scoreMgr);
                                    selectedSet = 0;
                                    selectedDiff = 0;
                                    rankingScroll = 0;
                                    carouselPos = 0.0f;
                                    audio.playHitSound(0);
                                    updateSongPreview();
                                }
                                break;
                            }
                        }
                    }
                    // 6. Fila 3: Z X C V B N M . / (y: 159..191)
                    else if (touch.py >= 159.0f && touch.py <= 191.0f) {
                        const char* r3Chars = "zxcvbnm./";
                        for (int i = 0; i < 9; ++i) {
                            float kx = 16.0f + i * 32.0f;
                            if (touch.px >= kx && touch.px <= kx + 29.8f) {
                                if (songMgr.searchQuery.size() < 32) {
                                    songMgr.searchQuery += r3Chars[i];
                                    searchLastKeyIndex = 31 + i;
                                    searchKeyFlashTimer = 6;
                                    songMgr.applySortAndFilter(&scoreMgr);
                                    selectedSet = 0;
                                    selectedDiff = 0;
                                    rankingScroll = 0;
                                    carouselPos = 0.0f;
                                    audio.playHitSound(0);
                                    updateSongPreview();
                                }
                                break;
                            }
                        }
                    }
                    // 7. Fila 4: Acciones inferiores (y: 194..238)
                    else if (touch.py >= 194.0f && touch.py <= 238.0f) {
                        // Limpiar (x: 4..60)
                        if (touch.px >= 4.0f && touch.px <= 60.0f) {
                            songMgr.searchQuery.clear();
                            searchLastKeyIndex = 40;
                            searchKeyFlashTimer = 6;
                            songMgr.applySortAndFilter(&scoreMgr);
                            selectedSet = 0;
                            selectedDiff = 0;
                            rankingScroll = 0;
                            carouselPos = 0.0f;
                            audio.playHitSound(0);
                            updateSongPreview();
                        }
                        // Espacio (x: 63..205)
                        else if (touch.px >= 63.0f && touch.px <= 205.0f) {
                            if (songMgr.searchQuery.size() < 32 && !songMgr.searchQuery.empty() && songMgr.searchQuery.back() != ' ') {
                                songMgr.searchQuery += ' ';
                                searchLastKeyIndex = 41;
                                searchKeyFlashTimer = 6;
                                audio.playHitSound(0);
                            }
                        }
                        // Borrar (x: 208..260)
                        else if (touch.px >= 208.0f && touch.px <= 260.0f) {
                            if (!songMgr.searchQuery.empty()) {
                                songMgr.searchQuery.pop_back();
                                searchLastKeyIndex = 42;
                                searchKeyFlashTimer = 6;
                                songMgr.applySortAndFilter(&scoreMgr);
                                selectedSet = 0;
                                selectedDiff = 0;
                                rankingScroll = 0;
                                carouselPos = 0.0f;
                                audio.playHitSound(0);
                                updateSongPreview();
                            }
                        }
                        // OK / Listo (x: 263..316)
                        else if (touch.px >= 263.0f && touch.px <= 316.0f) {
                            searchLastKeyIndex = 43;
                            searchKeyFlashTimer = 6;
                            searchKeyboardOpen = false;
                            audio.playHitSound(2);
                        }
                    }
                }
            } else {
                if (rankingExpanded) {
                    if (kDown & KEY_B) {
                        rankingExpanded = false;
                        audio.playHitSound(0);
                        continue;
                    }
                    if (kDown & KEY_DUP) {
                        if (rankingScroll > 0) {
                            rankingScroll--;
                            audio.playHitSound(0);
                        }
                    }
                    if (kDown & KEY_DDOWN) {
                        rankingScroll++;
                        audio.playHitSound(0);
                    }
                } else {
                    if (kDown & KEY_B) {
                        if (songSelectTab != 0) {
                            songSelectTab = 0;
                            audio.playHitSound(0);
                            continue;
                        }
                        audio.playHitSound(0);
                        if (multiMgr.inMultiplayer) {
                            if (multiMgr.isHost()) {
                                multiMgr.hostChangingSong = false;
                                multiMgr.broadcastLobbyState();
                            }
                            appState = STATE_MULTI_LOBBY;
                        } else {
                            audio.setLoop(false);
                            appState = STATE_MAIN_MENU;
                        }
                        continue;
                    }
                }

                if (kDown & KEY_START) {
                    songMgr.sortMode = (songMgr.sortMode + 1) % 4;
                    songMgr.applySortAndFilter(&scoreMgr);
                    selectedSet = 0;
                    selectedDiff = 0;
                    rankingScroll = 0;
                    carouselPos = 0.0f;
                    audio.playHitSound(2);
                    updateSongPreview();
                }

                if (kDown & (KEY_ZL | KEY_ZR)) {
                    openSearchKeyboard();
                }

                if (kDown & KEY_X && !multiMgr.inMultiplayer) {
                    audio.playHitSound(0);
                    settingsReturnState = STATE_SONG_SELECT;
                    calibrationMode = false;
                    appState = STATE_SETTINGS;
                    continue;
                }

                if (kDown & KEY_Y && !multiMgr.inMultiplayer) {
                    audio.stopMusic();
                    audio.playHitSound(2);
                    multiMgr.stop();
                    wifiReturnState = STATE_SONG_SELECT;
                    wifiServer.start();
                    appState = STATE_WIFI_TRANSFER;
                    continue;
                }

                if (!songMgr.sets.empty()) {
                    int numSets = static_cast<int>(songMgr.sets.size());
                    int setDelta = 0;
                    if (kDown & (KEY_DUP | KEY_L))   setDelta = -1;
                    if (kDown & (KEY_DDOWN | KEY_R)) setDelta = +1;

                    // Desplazamiento vertical de canciones con C-Stick (New 3DS) o Circle Pad (Arriba / Abajo)
                    if (setDelta == 0 && analogNavCooldown == 0 && !rankingExpanded) {
                        if (cstick.dy > 45 || cpad.dy > 65) {
                            setDelta = -1;
                            analogNavCooldown = (std::abs(cstick.dy) > 90 || std::abs(cpad.dy) > 110) ? 5 : 8;
                        } else if (cstick.dy < -45 || cpad.dy < -65) {
                            setDelta = +1;
                            analogNavCooldown = (std::abs(cstick.dy) > 90 || std::abs(cpad.dy) > 110) ? 5 : 8;
                        }
                    }

                    if (setDelta != 0) {
                        selectedSet = (selectedSet + setDelta + numSets) % numSets;
                        selectedDiff = 0;
                        rankingScroll = 0;
                        audio.playHitSound(0);
                        previewDelay = isNew3DS ? 10 : 16;
                    }

                    const auto& curSet = songMgr.sets[selectedSet];
                    int numDiffs = static_cast<int>(curSet.difficulties.size());
                    if (numDiffs > 0) {
                        int diffDelta = 0;
                        if (kDown & KEY_DLEFT)  diffDelta = -1;
                        if (kDown & KEY_DRIGHT) diffDelta = +1;
                        if (diffDelta == 0 && analogNavCooldown == 0 && !rankingExpanded) {
                            if (cstick.dx < -45 || cpad.dx < -65)      { diffDelta = -1; analogNavCooldown = 10; }
                            else if (cstick.dx > 45 || cpad.dx > 65)   { diffDelta = +1; analogNavCooldown = 10; }
                        }
                        if (diffDelta != 0) {
                            selectedDiff = (selectedDiff + diffDelta + numDiffs) % numDiffs;
                            rankingScroll = 0;
                            audio.playHitSound(0);
                            previewDelay = isNew3DS ? 6 : 10;
                        }

                        if (kDown & KEY_A) {
                            if (multiMgr.inMultiplayer) {
                                const auto& s = songMgr.sets[selectedSet];
                                if (!s.difficulties.empty()) {
                                    int dIdx = std::max(0, std::min(selectedDiff, static_cast<int>(s.difficulties.size()) - 1));
                                    const auto& d = s.difficulties[dIdx];
                                    QueuedBeatmap qm(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, multiMgr.localSlot);
                                    if (multiMgr.isHost()) {
                                        multiMgr.hostChangingSong = false;
                                        if (multiPickerAction == PICKER_ACTION_QUEUE) {
                                            multiMgr.enqueueBeatmap(qm);
                                        } else {
                                            multiMgr.setActiveBeatmap(qm);
                                        }
                                    } else {
                                        multiMgr.requestQueueMap(qm);
                                    }
                                    audio.playHitSound(2);
                                    appState = STATE_MULTI_LOBBY;
                                    continue;
                                }
                            } else {
                                startSelectedBeatmap();
                                continue;
                            }
                        }
                    }
                }
            }

            carouselPos = Ui::approach(carouselPos, static_cast<float>(selectedSet), 0.22f);

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: FONDO DEL BEATMAP, LISTA DE CANCIONES Y ESTRELLAS (★) EN 3D ESTEREOSCÓPICO ---
            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);

                float dBg = depthShift(-3.5f);
                if (bgTex.loaded) {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), clrPrimary, 0.01f);
                    bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, static_cast<u8>(bgFade));
                    u8 dimAlpha = static_cast<u8>((settings.bgDim * 200) / 100);
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 12, 22, dimAlpha));
                } else {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), clrPrimary, 0.02f);
                }
                // Vineta izquierda para dar legibilidad al panel de informacion
                Ui::gradH(0.0f, 0.0f, 240.0f, 240.0f, 0.05f, Ui::rgba(8, 6, 16, 220), Ui::rgba(8, 6, 16, 0));

                // Cabecera superior estilo osu!lazer con branding y badges
                Ui::gradV(0.0f, 0.0f, 400.0f, 30.0f, 0.3f, Ui::rgba(14, 11, 24, 250), Ui::rgba(14, 11, 24, 180));
                C2D_DrawRectSolid(0.0f, 29.0f, 0.31f, 400.0f, 1.5f, Ui::rgba(255, 102, 171, 190));

                Ui::pill(10.0f, 6.0f, 42.0f, 18.0f, 0.32f, Ui::rgba(255, 102, 171, 230));
                drawCenteredText(textBuf, 31.0f, 15.0f, 0.37f, clrWhite, "osu!", 0.9f);
                if (multiMgr.inMultiplayer) {
                    const char* sTitle = (multiPickerAction == PICKER_ACTION_QUEUE)
                        ? LOC(STR_SONG_MULTI_QUEUE)
                        : LOC(STR_SONG_MULTI_PICK);
                    drawText(textBuf, 58.0f, 7.5f, 0.44f, clrCursor, sTitle, 0.9f);
                } else {
                    drawText(textBuf, 58.0f, 7.5f, 0.44f, clrWhite, LOC(STR_SONG_SELECT_TITLE), 0.9f);
                }

                if (searchKeyboardOpen) {
                    std::string fBadge = songMgr.searchQuery.empty() ? std::string(LOC(STR_SEARCH)) + "..." : truncateString(songMgr.searchQuery, 12);
                    Ui::pill(210.0f, 6.0f, 130.0f, 18.0f, 0.32f, Ui::rgba(255, 102, 171, 230));
                    drawCenteredTextWithShadow(textBuf, 275.0f, 15.0f, 0.34f, clrWhite, fBadge.c_str(), 0.9f);
                } else if (!songMgr.searchQuery.empty()) {
                    std::string fBadge = std::string(LOC(STR_SONG_FILTER_BADGE)) + ": " + truncateString(songMgr.searchQuery, 10);
                    Ui::pill(236.0f, 6.0f, 104.0f, 18.0f, 0.32f, Ui::rgba(0, 180, 220, 230));
                    drawCenteredTextWithShadow(textBuf, 288.0f, 15.0f, 0.34f, clrWhite, fBadge.c_str(), 0.9f);
                } else {
                    char sortBadge[64];
                    std::snprintf(sortBadge, sizeof(sortBadge), "%s: %s", LOC(STR_MODE), SongManager::getSortModeName(songMgr.sortMode));
                    Ui::pill(236.0f, 6.0f, 104.0f, 18.0f, 0.32f, Ui::rgba(45, 38, 72, 230));
                    drawCenteredTextWithShadow(textBuf, 288.0f, 15.0f, 0.34f, clrCursor, sortBadge, 0.9f);
                }

                char headerInfo[96];
                std::snprintf(headerInfo, sizeof(headerInfo), "%d / %d",
                    songMgr.sets.empty() ? 0 : (selectedSet + 1),
                    static_cast<int>(songMgr.sets.size()));
                Ui::pill(346.0f, 6.0f, 46.0f, 18.0f, 0.32f, Ui::rgba(36, 30, 54, 230));
                drawCenteredText(textBuf, 369.0f, 15.0f, 0.36f, clrWhite, headerInfo, 0.9f);

                if (songMgr.sets.empty()) {
                    float dEmpty = depthShift(2.5f);
                    Ui::panel(40.0f + dEmpty, 70.0f, 320.0f, 100.0f, 0.3f, Ui::rgba(30, 27, 44, 235), clrMiss);
                    if (!songMgr.searchQuery.empty()) {
                        drawText(textBuf, 56.0f + dEmpty, 82.0f, 0.48f, clrCursor, LOC(STR_SONG_NO_SEARCH_RESULTS));
                        drawText(textBuf, 56.0f + dEmpty, 108.0f, 0.46f, clrWhite, truncateString("\"" + songMgr.searchQuery + "\"", 34).c_str());
                        drawText(textBuf, 56.0f + dEmpty, 134.0f, 0.40f, clr300, LOC(STR_SONG_TAP_SEARCH_CLEAR));
                    } else {
                        drawText(textBuf, 56.0f + dEmpty, 82.0f, 0.5f, clrMiss, LOC(STR_SONG_NO_BEATMAPS));
                        drawText(textBuf, 56.0f + dEmpty, 110.0f, 0.43f, clrWhite, LOC(STR_SONG_PUT_FILES));
                        drawText(textBuf, 56.0f + dEmpty, 130.0f, 0.45f, clr300, "sdmc:/3ds/osu-3ds/songs/");
                    }
                } else {
                    const int nSets = static_cast<int>(songMgr.sets.size());

                    // ---- Carrusel de canciones (derecha), estilo tarjetas de sala multijugador ----
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
                        float dCard = depthShift(0.8f + focus * 3.2f - dist * 0.45f);
                        float cardH = 38.0f + focus * 12.0f;
                        float cy = 122.0f + rel * 46.0f;
                        float cx = 206.0f - focus * 36.0f - (sel ? beatPulse * 4.0f : 0.0f) + std::max(0.0f, dist - 1.0f) * 6.0f + dCard;
                        float cardY = cy - cardH * 0.5f;
                        float cardW = 400.0f - (cx - dCard) + 10.0f;
                        if (cardY > 240.0f || cardY + cardH < 30.0f) continue;

                        float maxStars = cs.difficulties.back().starRating;
                        u32 starClr = getStarRatingColor(getModAdjustedStars(maxStars, settings.activeMods));
                        u8 fade = static_cast<u8>(255.0f * std::max(0.35f, 1.0f - dist * 0.18f));

                        u32 bgClr = sel ? Ui::rgba(36, 26, 54, 250) : Ui::rgba(22, 19, 36, fade);
                        u32 borderClr = sel ? starClr : Ui::rgba(50, 44, 72, fade);

                        // Sombra suave proyectada estilo tarjeta multijugador
                        C2D_DrawRectSolid(cx + 2.0f, cardY + 3.0f, 0.08f, cardW, cardH, Ui::rgba(0, 0, 0, 85));

                        // Tarjeta rectangular con degradado, sombra y borde superior
                        Ui::panel(cx, cardY, cardW, cardH, 0.10f, bgClr, borderClr);

                        if (sel) {
                            // Brillo suave y pastilla vertical de acento iluminada como en salas multi
                            Ui::glow(cx + 10.0f, cardY + cardH * 0.5f, 0.09f, 26.0f, Ui::withAlpha(starClr, 100), 3);
                            Ui::pill(cx + 4.0f, cardY + 4.0f, 4.0f, cardH - 8.0f, 0.14f, starClr);
                        } else {
                            // Barra vertical de acento sutil
                            C2D_DrawRectSolid(cx, cardY, 0.12f, 3.5f, cardH, Ui::withAlpha(starClr, fade));
                        }

                        u32 titleClr = sel ? clrWhite : Ui::rgba(215, 210, 235, fade);
                        drawTextWithShadow(textBuf, cx + 14.0f, cardY + 4.0f, sel ? 0.46f : 0.40f, titleClr, truncateString(cs.title, sel ? 24 : 22).c_str(), 0.15f);
                        drawText(textBuf, cx + 14.0f, cardY + (sel ? 21.0f : 18.0f), 0.32f, Ui::rgba(185, 180, 210, fade), truncateString(cs.artist, 26).c_str(), 0.15f);

                        if (sel) {
                            int dIdx = std::min(selectedDiff, (int)cs.difficulties.size() - 1);
                            std::string diffName = cs.difficulties[dIdx].version;
                            std::string creatorName = cs.difficulties[dIdx].creator;
                            drawText(textBuf, cx + 14.0f, cardY + 33.0f, 0.28f, Ui::rgba(255, 224, 120, 255),
                                     truncateString("[" + diffName + "] " + std::string(LOC(STR_BY)) + " " + creatorName, 28).c_str(), 0.15f);
                        }

                        // Pastilla de estrellas a la derecha estilo tarjeta de sala multijugador
                        char sTag[16];
                        std::snprintf(sTag, sizeof(sTag), "*%.2f", getModAdjustedStars(maxStars, settings.activeMods));
                        float pw = 42.0f;
                        Ui::pill(400.0f - pw - 8.0f + dCard, cardY + cardH * 0.5f - 7.5f, pw, 15.0f, 0.16f, Ui::withAlpha(starClr, fade));
                        drawCenteredText(textBuf, 400.0f - pw * 0.5f - 8.0f + dCard, cardY + cardH * 0.5f, 0.28f, Ui::rgba(0, 0, 0, 245), sTag, 0.18f);
                    }

                    // ---- Panel de informacion (izquierda) flotando hacia adelante en 3D ----
                    const auto& curSet = songMgr.sets[selectedSet];
                    const int totalDiffs = static_cast<int>(curSet.difficulties.size());
                    const int dIdx = std::max(0, std::min(selectedDiff, totalDiffs - 1));
                    float dInfo = depthShift(2.2f);

                    Ui::panel(8.0f + dInfo, 36.0f, 166.0f, 74.0f, 0.3f, Ui::rgba(24, 21, 40, 225), clrPrimary);
                    drawText(textBuf, 16.0f + dInfo, 40.0f, 0.50f, clrWhite, truncateString(curSet.title, 22).c_str(), 0.6f);
                    drawText(textBuf, 16.0f + dInfo, 58.0f, 0.40f, clrCursor, truncateString(curSet.artist, 26).c_str(), 0.6f);
                    if (totalDiffs > 0) {
                        const auto& dd = curSet.difficulties[dIdx];
                        drawText(textBuf, 16.0f + dInfo, 73.0f, 0.34f, clrMuted, truncateString(std::string(LOC(STR_SONG_MAP_BY)) + " " + dd.creator, 28).c_str(), 0.6f);
                        char cnt[64];
                        float liveBpm = dd.getBPM(getModsSpeedMultiplier(settings.activeMods));
                        std::snprintf(cnt, sizeof(cnt), "%.0f BPM | %dc %ds%s",
                            liveBpm, dd.circleCount, dd.sliderCount,
                            curSet.hasVideo ? " | [VIDEO]" : "");
                        drawText(textBuf, 16.0f + dInfo, 88.0f, 0.34f, curSet.hasVideo ? clr100 : Ui::rgba(150, 215, 255), cnt, 0.6f);
                    }

                    // ---- Burbujas de dificultad (estilo lazer) con latido BPM ----
                    if (totalDiffs > 0) {
                        Ui::panel(8.0f + dInfo, 118.0f, 166.0f, 86.0f, 0.3f, Ui::rgba(24, 21, 40, 225));
                        const int maxB = 8;
                        int bStart = std::max(0, std::min(dIdx - maxB / 2, totalDiffs - maxB));
                        int bEnd = std::min(totalDiffs, bStart + maxB);
                        for (int i = bStart; i < bEnd; ++i) {
                            float bx = 22.0f + (i - bStart) * 19.0f + dInfo;
                            float by = 136.0f;
                            bool isSel = (i == dIdx);
                            float st = getModAdjustedStars(curSet.difficulties[i].starRating, settings.activeMods);
                            u32 sc = getStarRatingColor(st);
                            if (isSel) {
                                float bR = 9.2f + beatPulse * 1.4f;
                                float dBub = depthShift(1.0f);
                                Ui::glow(bx + dBub, by, 0.35f, 14.5f + beatPulse * 3.0f, Ui::withAlpha(sc, static_cast<u8>(130 + beatPulse * 50)), 4);
                                C2D_DrawCircleSolid(bx + dBub, by, 0.4f, bR, sc);
                                Ui::ring(bx + dBub, by, 0.45f, bR + 2.0f, 1.5f, clrWhite, 18);
                            } else {
                                C2D_DrawCircleSolid(bx, by, 0.4f, 6.0f, Ui::withAlpha(sc, 190));
                            }
                        }
                        const auto& sd = curSet.difficulties[dIdx];
                        float selStars = getModAdjustedStars(sd.starRating, settings.activeMods);
                        u32 selClr = getStarRatingColor(selStars);
                        drawText(textBuf, 14.0f + dInfo, 154.0f, 0.42f, clrWhite, truncateString(sd.version, 24).c_str(), 0.6f);
                        char sl[48];
                        std::snprintf(sl, sizeof(sl), "*%.2f", selStars);
                        Ui::pill(14.0f + dInfo, 172.0f, 46.0f, 15.0f, 0.5f, selClr);
                        drawCenteredText(textBuf, 37.0f + dInfo, 179.5f, 0.34f, Ui::rgba(20, 16, 30), sl, 0.7f);
                        int curSelMode = (settings.gameMode == MODE_TAIKO || sd.mode == 1) ? 1 : (settings.gameMode == MODE_CATCH || sd.mode == 2) ? 2 : (settings.gameMode == MODE_MANIA || sd.mode == 3) ? 3 : 0;
                        const auto* rec = scoreMgr.getRecord(sd.osuPath, curSelMode);
                        if (rec) {
                            std::snprintf(sl, sizeof(sl), "%s  %.1f%%", rec->bestGrade.c_str(), rec->bestAcc);
                            drawText(textBuf, 68.0f + dInfo, 172.0f, 0.38f, clr100, sl, 0.6f);
                            std::snprintf(sl, sizeof(sl), "%.0f pp", rec->bestPP);
                            drawText(textBuf, 68.0f + dInfo, 186.0f, 0.34f, clr300, sl, 0.6f);
                        } else {
                            drawText(textBuf, 68.0f + dInfo, 174.0f, 0.34f, clrMuted, LOC(STR_SONG_NO_RECORD), 0.6f);
                        }
                    }
                }

                // Barra inferior de atajos estilo multijugador
                Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(14, 11, 26, 140), Ui::rgba(14, 11, 26, 250));
                if (searchKeyboardOpen) {
                    float byY = 219.0f;
                    drawKeyBadge(textBuf, 10.0f, byY, "B", LOC(STR_DONE), Ui::rgba(50, 165, 255));
                    drawKeyBadge(textBuf, 76.0f, byY, "X", LOC(STR_CLEAR), Ui::rgba(255, 180, 40));
                    drawKeyBadge(textBuf, 152.0f, byY, "Y", LOC(STR_DELETE), Ui::rgba(255, 75, 80));
                    drawTextWithShadow(textBuf, 224.0f, byY, 0.33f, clrCursor, LOC(STR_SEARCH_KEYBOARD_HINT), 0.35f);
                } else if (multiMgr.inMultiplayer) {
                    float byY = 219.0f;
                    drawKeyBadge(textBuf, 10.0f, byY, "A", LOC(STR_MULTI_JOIN), Ui::rgba(50, 165, 255));
                    drawKeyBadge(textBuf, 78.0f, byY, "B", LOC(STR_BACK), Ui::rgba(255, 75, 80));
                    drawKeyBadge(textBuf, 146.0f, byY, "SELECT", "Mods", Ui::rgba(255, 102, 171));
                    drawKeyBadge(textBuf, 230.0f, byY, "< >", LOC(STR_DIFFICULTY), Ui::rgba(50, 215, 115));
                } else {
                    float byY = 219.0f;
                    drawKeyBadge(textBuf, 8.0f, byY, "A", LOC(STR_MENU_PLAY), Ui::rgba(50, 165, 255));
                    drawKeyBadge(textBuf, 76.0f, byY, "START", LOC(STR_MODE), Ui::rgba(165, 120, 255));
                    drawKeyBadge(textBuf, 156.0f, byY, "SELECT", "Mods", Ui::rgba(255, 102, 171));
                    drawKeyBadge(textBuf, 238.0f, byY, "Y", "Wi-Fi", Ui::rgba(255, 180, 40));
                    drawKeyBadge(textBuf, 308.0f, byY, "X", LOC(STR_MENU_SETTINGS), Ui::rgba(50, 215, 115));
                }
                Ui::drawTransition(400.0f, 240.0f, screenFade, clrPrimary);
            }

            // --- PANTALLA INFERIOR: DETALLES, ESTRELLAS, PP Y RECORD O SELECTOR DE MODS ---
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), clrPrimary, 0.02f);
            if (bgTex.loaded) {
                bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.03f, 65);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 320.0f, 240.0f, Ui::rgba(14, 11, 24, 175));
            }
            if (searchKeyboardOpen) {
                // Fondo oscuro estilizado para el teclado
                Ui::panel(2.0f, 2.0f, 316.0f, 236.0f, 0.50f, Ui::rgba(14, 10, 22, 255), clrPrimary);

                // =========================================================
                // 1. BARRA DE BÚSQUEDA Y ENTRADA DE TEXTO (y: 3..31)
                // =========================================================
                Ui::panel(4.0f, 3.0f, 226.0f, 28.0f, 0.55f, Ui::rgba(24, 18, 36, 255), clrPrimary);
                // Icono de lupa estilizada
                drawRing(14.0f, 17.0f, 5.0f, 1.4f, clrCursor, 14, 0.58f);
                C2D_DrawLine(18.0f, 21.0f, clrCursor, 22.0f, 25.0f, clrCursor, 2.0f, 0.58f);

                if (!songMgr.searchQuery.empty()) {
                    bool showCursor = (animTick / 20) % 2 == 0;
                    std::string dispQuery = truncateString(songMgr.searchQuery, 22);
                    if (showCursor) dispQuery += "|";
                    drawTextWithShadow(textBuf, 27.0f, 7.0f, 0.40f, clrWhite, dispQuery.c_str(), 0.62f);
                } else {
                    bool showCursor = (animTick / 20) % 2 == 0;
                    drawTextWithShadow(textBuf, 27.0f, 7.0f, 0.35f, Ui::rgba(160, 150, 185),
                        showCursor ? (std::string(LOC(STR_SEARCH_PLACEHOLDER)) + "|").c_str() : LOC(STR_SEARCH_PLACEHOLDER), 0.62f);
                }

                // Botón Limpiar [X]
                Ui::panel(232.0f, 3.0f, 26.0f, 28.0f, 0.55f, Ui::rgba(52, 22, 38, 255), Ui::rgba(255, 80, 100));
                drawCenteredTextWithShadow(textBuf, 245.0f, 17.0f, 0.38f, Ui::rgba(255, 140, 160), "X", 0.62f);

                // Botón Listo [LISTO]
                Ui::panel(262.0f, 3.0f, 54.0f, 28.0f, 0.55f, Ui::rgba(22, 68, 42, 255), clr100);
                drawCenteredTextWithShadow(textBuf, 289.0f, 17.0f, 0.36f, clrWhite, LOC(STR_DONE), 0.62f);

                // =========================================================
                // 2. FILA DE FILTROS RÁPIDOS / CHIPS (y: 34..52)
                // =========================================================
                bool hasStd = (songMgr.searchQuery.find("mode:std") != std::string::npos);
                bool hasTaiko = (songMgr.searchQuery.find("mode:taiko") != std::string::npos);
                bool hasCatch = (songMgr.searchQuery.find("mode:catch") != std::string::npos);
                bool hasMania = (songMgr.searchQuery.find("mode:mania") != std::string::npos);
                bool hasStar4 = (songMgr.searchQuery.find("★4") != std::string::npos || songMgr.searchQuery.find("stars>4") != std::string::npos);
                bool hasStar5 = (songMgr.searchQuery.find("★5") != std::string::npos || songMgr.searchQuery.find("stars>5") != std::string::npos);

                // Chip TODOS
                bool noFilter = songMgr.searchQuery.empty();
                Ui::panel(4.0f, 34.0f, 40.0f, 17.0f, 0.55f,
                    noFilter ? Ui::rgba(75, 45, 110, 240) : Ui::rgba(24, 20, 38, 230),
                    noFilter ? clrCursor : Ui::rgba(60, 52, 80));
                drawCenteredTextWithShadow(textBuf, 24.0f, 42.5f, 0.28f, noFilter ? clrWhite : Ui::rgba(195, 185, 220), LOC(STR_SONG_ALL), 0.62f);

                // Chip STD
                Ui::panel(47.0f, 34.0f, 32.0f, 17.0f, 0.55f,
                    hasStd ? Ui::rgba(75, 25, 60, 240) : Ui::rgba(24, 20, 38, 230),
                    hasStd ? Ui::rgba(255, 102, 171) : Ui::rgba(60, 52, 80));
                drawCenteredTextWithShadow(textBuf, 63.0f, 42.5f, 0.28f, hasStd ? Ui::rgba(255, 150, 205) : Ui::rgba(195, 185, 220), "STD", 0.62f);

                // Chip TAIKO
                Ui::panel(82.0f, 34.0f, 40.0f, 17.0f, 0.55f,
                    hasTaiko ? Ui::rgba(75, 25, 32, 240) : Ui::rgba(24, 20, 38, 230),
                    hasTaiko ? Ui::rgba(255, 75, 80) : Ui::rgba(60, 52, 80));
                drawCenteredTextWithShadow(textBuf, 102.0f, 42.5f, 0.28f, hasTaiko ? Ui::rgba(255, 140, 150) : Ui::rgba(195, 185, 220), "TAIKO", 0.62f);

                // Chip CATCH
                Ui::panel(125.0f, 34.0f, 40.0f, 17.0f, 0.55f,
                    hasCatch ? Ui::rgba(20, 70, 40, 240) : Ui::rgba(24, 20, 38, 230),
                    hasCatch ? Ui::rgba(45, 215, 115) : Ui::rgba(60, 52, 80));
                drawCenteredTextWithShadow(textBuf, 145.0f, 42.5f, 0.28f, hasCatch ? Ui::rgba(140, 255, 180) : Ui::rgba(195, 185, 220), "CATCH", 0.62f);

                // Chip MANIA
                Ui::panel(168.0f, 34.0f, 40.0f, 17.0f, 0.55f,
                    hasMania ? Ui::rgba(20, 60, 85, 240) : Ui::rgba(24, 20, 38, 230),
                    hasMania ? Ui::rgba(70, 195, 255) : Ui::rgba(60, 52, 80));
                drawCenteredTextWithShadow(textBuf, 188.0f, 42.5f, 0.28f, hasMania ? Ui::rgba(155, 235, 255) : Ui::rgba(195, 185, 220), "MANIA", 0.62f);

                // Chip *4+
                Ui::panel(211.0f, 34.0f, 32.0f, 17.0f, 0.55f,
                    hasStar4 ? Ui::rgba(75, 62, 20, 240) : Ui::rgba(24, 20, 38, 230),
                    hasStar4 ? Ui::rgba(255, 215, 0) : Ui::rgba(60, 52, 80));
                drawCenteredTextWithShadow(textBuf, 227.0f, 42.5f, 0.28f, hasStar4 ? Ui::rgba(255, 235, 95) : Ui::rgba(195, 185, 220), "*4+", 0.62f);

                // Chip *5+
                Ui::panel(246.0f, 34.0f, 32.0f, 17.0f, 0.55f,
                    hasStar5 ? Ui::rgba(75, 45, 20, 240) : Ui::rgba(24, 20, 38, 230),
                    hasStar5 ? Ui::rgba(255, 140, 50) : Ui::rgba(60, 52, 80));
                drawCenteredTextWithShadow(textBuf, 262.0f, 42.5f, 0.28f, hasStar5 ? Ui::rgba(255, 180, 105) : Ui::rgba(195, 185, 220), "*5+", 0.62f);

                // Contador de resultados
                char cntBadge[16];
                std::snprintf(cntBadge, sizeof(cntBadge), "%d", static_cast<int>(songMgr.sets.size()));
                Ui::panel(281.0f, 34.0f, 35.0f, 17.0f, 0.55f, Ui::rgba(30, 24, 48, 240), clrCursor);
                drawCenteredTextWithShadow(textBuf, 298.5f, 42.5f, 0.28f, clrCursor, cntBadge, 0.62f);

                // =========================================================
                // 3. TECLADO QWERTY TÁCTIL (y: 54..238)
                // =========================================================
                auto drawSearchKey = [&](int keyId, float kx, float ky, float kw, float kh, const char* label, float fontScale, u32 customFill = 0, u32 customBorder = 0) {
                    bool isFlashed = (searchKeyFlashTimer > 0 && searchLastKeyIndex == keyId);
                    u32 bg = isFlashed ? Ui::rgba(255, 102, 171, 255)
                           : (customFill != 0 ? customFill : Ui::rgba(32, 26, 48, 255));
                    u32 bdr = isFlashed ? clrWhite
                            : (customBorder != 0 ? customBorder : Ui::rgba(75, 65, 105, 255));
                    u32 textClr = isFlashed ? clrWhite : Ui::rgba(250, 248, 255);

                    Ui::panel(kx, ky, kw, kh, 0.60f, bg, bdr);
                    C2D_DrawRectSolid(kx + 1.0f, ky + 1.0f, 0.62f, kw - 2.0f, 1.2f, isFlashed ? clrWhite : Ui::rgba(255, 255, 255, 50));
                    drawCenteredTextWithShadow(textBuf, kx + kw * 0.5f, ky + kh * 0.5f, fontScale, textClr, label, 0.68f);
                };

                // Fila 0: 1 2 3 4 5 6 7 8 9 0 + Borrar (y: 54..86)
                const char* numLabels[] = {"1","2","3","4","5","6","7","8","9","0"};
                for (int i = 0; i < 10; ++i) {
                    float kx = 4.0f + i * 26.5f;
                    drawSearchKey(i, kx, 54.0f, 24.5f, 32.0f, numLabels[i], 0.44f);
                }
                drawSearchKey(10, 271.0f, 54.0f, 45.0f, 32.0f, LOC(STR_DELETE), 0.32f, Ui::rgba(56, 24, 42, 255), Ui::rgba(255, 100, 130));

                // Fila 1: Q W E R T Y U I O P (y: 89..121)
                const char* r1Labels[] = {"Q","W","E","R","T","Y","U","I","O","P"};
                for (int i = 0; i < 10; ++i) {
                    float kx = 4.0f + i * 31.0f;
                    drawSearchKey(11 + i, kx, 89.0f, 29.0f, 32.0f, r1Labels[i], 0.44f);
                }

                // Fila 2: A S D F G H J K L - (y: 124..156)
                const char* r2Labels[] = {"A","S","D","F","G","H","J","K","L","-"};
                for (int i = 0; i < 10; ++i) {
                    float kx = 8.0f + i * 30.2f;
                    drawSearchKey(21 + i, kx, 124.0f, 28.2f, 32.0f, r2Labels[i], 0.44f);
                }

                // Fila 3: Z X C V B N M . / (y: 159..191)
                const char* r3Labels[] = {"Z","X","C","V","B","N","M",".","/"};
                for (int i = 0; i < 9; ++i) {
                    float kx = 16.0f + i * 32.0f;
                    drawSearchKey(31 + i, kx, 159.0f, 29.8f, 32.0f, r3Labels[i], 0.44f);
                }

                // Fila 4: Acciones inferiores (y: 194..236)
                drawSearchKey(40, 4.0f, 194.0f, 56.0f, 42.0f, LOC(STR_CLEAR), 0.30f, Ui::rgba(52, 22, 38, 255), Ui::rgba(255, 90, 110));
                drawSearchKey(41, 63.0f, 194.0f, 142.0f, 42.0f, LOC(STR_SPACE), 0.35f, Ui::rgba(36, 30, 56, 255), Ui::rgba(90, 80, 125));
                drawSearchKey(42, 208.0f, 194.0f, 52.0f, 42.0f, LOC(STR_DELETE), 0.30f, Ui::rgba(56, 24, 42, 255), Ui::rgba(255, 120, 140));
                drawSearchKey(43, 263.0f, 194.0f, 53.0f, 42.0f, "OK", 0.36f, Ui::rgba(22, 68, 42, 255), clr100);
            } else if (modMenuOpen) {
                Ui::panel(6.0f, 6.0f, 308.0f, 228.0f, 0.10f, Ui::rgba(22, 18, 34, 250), clrPrimary);

                // Cabecera del menú de Mods
                Ui::pill(12.0f, 10.0f, 44.0f, 15.0f, 0.14f, Ui::rgba(255, 102, 171, 220));
                drawCenteredText(textBuf, 34.0f, 17.5f, 0.35f, clrWhite, "MODS", 0.16f);
                drawText(textBuf, 60.0f, 10.0f, 0.42f, clrWhite, LOC(STR_MODS_INFO), 0.16f);

                // Multiplicador total de score
                float mult = getModsScoreMultiplier(settings.activeMods);
                char multStr[32];
                if (settings.activeMods & (MOD_AUTO | MOD_RX | MOD_AP)) {
                    std::snprintf(multStr, sizeof(multStr), "Unranked");
                } else {
                    std::snprintf(multStr, sizeof(multStr), "Score: %.2fx", mult);
                }
                u32 multClr = (mult > 1.0f) ? clr100 : ((mult == 1.0f) ? clr300 : clrMiss);
                Ui::pill(222.0f, 9.0f, 88.0f, 16.0f, 0.14f, Ui::rgba(30, 24, 48, 220));
                drawCenteredText(textBuf, 266.0f, 17.0f, 0.34f, multClr, multStr, 0.16f);

                drawText(textBuf, 12.0f, 28.0f, 0.28f, clrMuted, LOC(STR_MODS_CONTROLS_HINT), 0.16f);

                // Cuadrícula 5x3 de los 15 Modificadores
                for (int m = 0; m < 15; ++m) {
                    int col = m % 5;
                    int row = m / 5;
                    float mx = 8.0f + col * 62.0f;
                    float my = 43.0f + row * 43.0f;
                    bool isDisallowedInMulti = multiMgr.inMultiplayer && (modTable[m].bit == MOD_AUTO || modTable[m].bit == MOD_RX || modTable[m].bit == MOD_AP);
                    bool active = !isDisallowedInMulti && ((settings.activeMods & modTable[m].bit) != 0);
                    bool cursorOn = (modCursor == m);
                    u32 mClr = isDisallowedInMulti ? Ui::rgba(80, 70, 95) : modTable[m].color;

                    u32 cardBg = isDisallowedInMulti
                        ? Ui::rgba(18, 14, 24, 210)
                        : (active
                            ? Ui::lerpColor(Ui::rgba(26, 22, 40, 245), mClr, 0.38f)
                            : (cursorOn ? Ui::rgba(42, 36, 62, 240) : Ui::rgba(28, 24, 42, 230)));

                    Ui::panel(mx, my, 56.0f, 39.0f, 0.20f, cardBg, cursorOn ? clrCursor : (active ? mClr : Ui::rgba(55, 48, 80)));
                    if (active) {
                        C2D_DrawRectSolid(mx + 2.0f, my, 0.22f, 52.0f, 2.5f, mClr);
                    }
                    drawRectOutline(mx, my, 56.0f, 39.0f, cursorOn ? 1.8f : 1.0f, cursorOn ? clrCursor : (active ? mClr : Ui::rgba(55, 48, 80)), 0.24f);
                    drawCenteredText(textBuf, mx + 28.0f, my + 13.0f, 0.48f, isDisallowedInMulti ? clrMiss : (active ? clrWhite : Ui::brighten(mClr, 1.25f)), modTable[m].code, 0.25f);
                    drawCenteredText(textBuf, mx + 28.0f, my + 28.0f, 0.29f, isDisallowedInMulti ? clrMiss : (active ? clrWhite : clrMuted), isDisallowedInMulti ? LOC(STR_NOT_AVAIL) : Localization::getIndexed(Localization::STR_MOD_NAME_0, m), 0.25f);
                }

                // Panel inferior con descripción enriquecida del mod seleccionado
                int selM = std::max(0, std::min(14, modCursor));
                const auto& curDef = modTable[selM];
                bool curActive = (settings.activeMods & curDef.bit) != 0;

                Ui::panel(8.0f, 174.0f, 304.0f, 56.0f, 0.20f, Ui::rgba(18, 14, 28, 248), curDef.color);
                C2D_DrawRectSolid(8.0f, 174.0f, 0.23f, 304.0f, 2.0f, curDef.color);

                // Badge y nombre del mod seleccionado
                Ui::pill(14.0f, 179.0f, 28.0f, 14.0f, 0.24f, curDef.color);
                drawCenteredText(textBuf, 28.0f, 186.0f, 0.33f, clrWhite, curDef.code, 0.26f);
                drawText(textBuf, 46.0f, 179.0f, 0.40f, clrWhite, curDef.fullName, 0.26f);

                // Insignia de estado ACTIVO / INACTIVO
                bool isSelDisallowed = multiMgr.inMultiplayer && (curDef.bit == MOD_AUTO || curDef.bit == MOD_RX || curDef.bit == MOD_AP);
                Ui::pill(242.0f, 179.0f, 62.0f, 14.0f, 0.24f, isSelDisallowed ? Ui::rgba(60, 20, 25, 235) : (curActive ? Ui::rgba(25, 75, 45, 235) : Ui::rgba(35, 30, 50, 235)));
                drawCenteredText(textBuf, 273.0f, 186.0f, 0.29f, isSelDisallowed ? clrMiss : (curActive ? clr100 : clrMuted), isSelDisallowed ? LOC(STR_NOT_AVAIL) : (curActive ? LOC(STR_ON) : LOC(STR_OFF)), 0.26f);

                // Texto descriptivo del efecto del mod
                drawText(textBuf, 14.0f, 196.0f, 0.33f, Ui::rgba(220, 215, 240), Localization::getIndexed(Localization::STR_MOD_DESC_0, selM), 0.26f);
                if (isSelDisallowed) {
                    drawText(textBuf, 14.0f, 213.0f, 0.28f, clrMiss, LOC(STR_MODS_NOT_ONLINE), 0.26f);
                } else {
                    drawText(textBuf, 14.0f, 213.0f, 0.28f, clr300, LOC(STR_MODS_CONTROLS_HINT), 0.26f);
                }
            } else {
                if (!songMgr.sets.empty() && !songMgr.sets[selectedSet].difficulties.empty()) {
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
                    int curDiffMode = (settings.gameMode == MODE_TAIKO || d.mode == 1) ? 1 : (settings.gameMode == MODE_CATCH || d.mode == 2) ? 2 : (settings.gameMode == MODE_MANIA || d.mode == 3) ? 3 : 0;
                    const auto* rec = scoreMgr.getRecord(d.osuPath, curDiffMode);

                    // =========================================================================
                    // 1. HEADER CARD ESTILO OSU!LAZER: TITULO, ARTISTA Y METRICAS (y: 6..54)
                    // =========================================================================
                    Ui::panel(8.0f, 6.0f, 304.0f, 48.0f, 0.15f, Ui::rgba(20, 16, 32, 245), Ui::rgba(50, 42, 75));

                    // Badge [RANKED] estilo osu!lazer + Título
                    Ui::pill(14.0f, 9.0f, 48.0f, 13.0f, 0.22f, Ui::rgba(25, 95, 45, 230));
                    drawCenteredText(textBuf, 38.0f, 15.5f, 0.29f, Ui::rgba(110, 235, 110), "RANKED");
                    drawText(textBuf, 66.0f, 8.5f, 0.42f, clrWhite, truncateString(curSet.title, 22).c_str(), 0.30f);

                    // Botón pequeño [ BORRAR ] en el header (x: 250, y: 8.5, w: 56, h: 14)
                    Ui::pill(250.0f, 8.5f, 56.0f, 14.0f, 0.22f, Ui::rgba(140, 25, 38, 230));
                    drawRectOutline(250.0f, 8.5f, 56.0f, 14.0f, 1.0f, Ui::rgba(255, 75, 95), 0.24f);
                    drawCenteredText(textBuf, 278.0f, 15.5f, 0.28f, Ui::rgba(255, 220, 225), LOC(STR_DELETE), 0.32f);

                    // Artista y Creador
                    std::string artistCreator = curSet.artist + "  //  " + d.creator;
                    drawText(textBuf, 14.0f, 23.5f, 0.32f, Ui::rgba(185, 180, 210), truncateString(artistCreator, 36).c_str(), 0.30f);

                    // Rating de estrellas estilo lazer + Dificultad + Specs inline
                    u32 starClr = getStarRatingColor(modStars);
                    char starBadge[24];
                    std::snprintf(starBadge, sizeof(starBadge), "* %.2f", modStars);
                    Ui::pill(14.0f, 37.0f, 46.0f, 14.0f, 0.22f, Ui::withAlpha(starClr, 60));
                    drawCenteredText(textBuf, 37.0f, 44.0f, 0.32f, starClr, starBadge);

                    drawText(textBuf, 64.0f, 37.5f, 0.36f, starClr, truncateString(d.version, 13).c_str(), 0.30f);

                    char specsStr[64];
                    std::snprintf(specsStr, sizeof(specsStr), "CS %.1f  AR %.1f  OD %.1f  HP %.1f", dispCS, dispAR, dispOD, dispHP);
                    Ui::pill(152.0f, 36.0f, 154.0f, 16.0f, 0.22f, Ui::rgba(28, 22, 44, 235));
                    drawCenteredTextWithShadow(textBuf, 229.0f, 44.0f, 0.32f, Ui::rgba(235, 230, 255), specsStr, 0.30f);

                    // =========================================================================
                    // 2. PESTAÑAS: RESUMEN / RANKING LOCAL (y: 56..70)
                    // =========================================================================
                    static std::vector<ScoreRecord> cachedMapRecords;
                    static std::string cachedMapOsuPath = "";
                    static int cachedMapMode = -999;
                    if (d.osuPath != cachedMapOsuPath || curDiffMode != cachedMapMode) {
                        cachedMapOsuPath = d.osuPath;
                        cachedMapMode = curDiffMode;
                        cachedMapRecords = scoreMgr.getTopRecords(d.osuPath, 10, curDiffMode);
                    }
                    const std::vector<ScoreRecord>& mapRecords = cachedMapRecords;
                    int numRecs = static_cast<int>(mapRecords.size());
                    if (rankingScroll >= numRecs && numRecs > 0) rankingScroll = numRecs - 1;
                    if (rankingScroll < 0) rankingScroll = 0;

                    // Tab 0: [ 📊 RESUMEN ] (x: 8..118)
                    bool tab0Active = (songSelectTab == 0);
                    u32 tab0Bg = tab0Active ? Ui::rgba(55, 30, 75, 245) : Ui::rgba(22, 18, 32, 220);
                    u32 tab0Border = tab0Active ? clrPrimary : Ui::rgba(45, 38, 62);
                    Ui::panel(8.0f, 56.0f, 110.0f, 15.0f, 0.22f, tab0Bg, tab0Border);
                    drawCenteredTextWithShadow(textBuf, 63.0f, 62.5f, 0.30f, tab0Active ? clrWhite : Ui::rgba(210, 205, 230), LOC(STR_SUMMARY));

                    // Tab 1: [ 🏆 RANKING LOCAL (N) ] (x: 122..244)
                    bool tab1Active = (songSelectTab == 1);
                    u32 tab1Bg = tab1Active ? Ui::rgba(65, 35, 80, 245) : Ui::rgba(22, 18, 32, 220);
                    u32 tab1Border = tab1Active ? clrCursor : Ui::rgba(45, 38, 62);
                    Ui::panel(122.0f, 56.0f, 122.0f, 15.0f, 0.22f, tab1Bg, tab1Border);
                    char rnkTabStr[32];
                    std::snprintf(rnkTabStr, sizeof(rnkTabStr), "%s (%d)", LOC(STR_LOCAL_RANKING), numRecs);
                    drawCenteredTextWithShadow(textBuf, 183.0f, 62.5f, 0.30f, tab1Active ? clrCursor : Ui::rgba(210, 205, 230), rnkTabStr);

                    // Botón de acción lateral (x: 248..312)
                    if (songSelectTab == 1) {
                        u32 btnBg = rankingExpanded ? Ui::rgba(180, 40, 60, 230) : Ui::rgba(45, 75, 135, 230);
                        Ui::pill(248.0f, 56.0f, 64.0f, 15.0f, 0.23f, btnBg);
                        drawCenteredTextWithShadow(textBuf, 280.0f, 62.5f, 0.28f, clrWhite, rankingExpanded ? LOC(STR_COLLAPSE) : LOC(STR_EXPAND));
                    } else {
                        Ui::pill(248.0f, 56.0f, 64.0f, 15.0f, 0.22f, Ui::rgba(45, 32, 60, 220));
                        drawCenteredTextWithShadow(textBuf, 280.0f, 62.5f, 0.28f, Ui::rgba(255, 175, 215), LOC(STR_SONG_VIEW_TOP));
                    }

                    if (songSelectTab == 0) {
                        // =====================================================================
                        // 2A. PERSONAL BEST CARD ESTILO OSU!LAZER (y: 74..112)
                        // =====================================================================
                        Ui::gradH(8.0f, 74.0f, 304.0f, 38.0f, 0.20f, Ui::rgba(46, 22, 60, 235), Ui::rgba(24, 18, 38, 245));
                        drawRectOutline(8.0f, 74.0f, 304.0f, 38.0f, 1.0f, Ui::rgba(255, 102, 171, 95), 0.25f);

                        // Chip "MEJOR RECORD"
                        Ui::pill(14.0f, 77.0f, 68.0f, 12.0f, 0.25f, Ui::rgba(75, 32, 95, 235));
                        drawCenteredTextWithShadow(textBuf, 48.0f, 83.0f, 0.28f, Ui::rgba(255, 140, 205), LOC(STR_SONG_BEST_RECORD));

                        if (rec) {
                            // Puntuación numérica con formato
                            drawTextWithShadow(textBuf, 86.0f, 75.0f, 0.44f, clrWhite, formatScoreThousands(rec->bestScore).c_str(), 0.30f);

                            // Precisión y combo máximo
                            char accCombo[64];
                            std::snprintf(accCombo, sizeof(accCombo), "ACC: %.1f%%  COMBO: %dx", rec->bestAcc, rec->bestCombo);
                            drawTextWithShadow(textBuf, 14.0f, 95.0f, 0.30f, Ui::rgba(110, 235, 180), accCombo, 0.30f);

                            // Pill de PP ganado
                            char ppStr[32];
                            std::snprintf(ppStr, sizeof(ppStr), "%.1f pp", rec->bestPP);
                            Ui::pill(152.0f, 93.0f, 48.0f, 14.0f, 0.25f, Ui::rgba(45, 38, 80, 220));
                            drawCenteredTextWithShadow(textBuf, 176.0f, 99.5f, 0.29f, Ui::rgba(215, 205, 255), ppStr);

                            // Pill de Mods jugados (si los hubo)
                            if (rec->bestMods != MOD_NONE) {
                                std::string mStr = "+" + formatModsString(rec->bestMods);
                                Ui::pill(204.0f, 93.0f, 40.0f, 14.0f, 0.25f, Ui::rgba(85, 40, 105, 220));
                                drawCenteredTextWithShadow(textBuf, 224.0f, 99.5f, 0.28f, clrCursor, mStr.c_str());
                            }

                            // Insignia de Rango estilo Lazer (SS, S, A, etc.)
                            u32 gClr = getScoreGradeColor(rec->bestGrade, clrCursor);
                            Ui::panel(248.0f, 76.0f, 58.0f, 33.0f, 0.25f, Ui::rgba(35, 18, 48, 235), gClr);
                            drawCenteredTextWithShadow(textBuf, 277.0f, 89.0f, 0.46f, gClr, rec->bestGrade.c_str(), 0.32f);
                            drawCenteredTextWithShadow(textBuf, 277.0f, 102.0f, 0.22f, Ui::rgba(255, 180, 225), "GRADE");
                        } else {
                            drawCenteredTextWithShadow(textBuf, 160.0f, 85.0f, 0.38f, clrWhite, LOC(STR_SONG_NO_RECORDS_YET));
                            char ssMaxStr[64];
                            std::snprintf(ssMaxStr, sizeof(ssMaxStr), "Max SS: %.0f pp  •  %d %s  •  %s", maxSSPP, d.totalObjects(), LOC(STR_OBJECTS), LOC(STR_SONG_PLAY_NOW));
                            drawCenteredTextWithShadow(textBuf, 160.0f, 99.0f, 0.30f, Ui::rgba(255, 225, 95), ssMaxStr);
                        }

                        // =====================================================================
                        // 2B. CURVA DE ESFUERZO / STRAIN GRAPH ESTILO OSU!LAZER (y: 114..142)
                        // =====================================================================
                        Ui::panel(8.0f, 114.0f, 304.0f, 28.0f, 0.18f, Ui::rgba(20, 16, 32, 240), starClr);
                        drawTextWithShadow(textBuf, 14.0f, 115.5f, 0.29f, clrWhite, LOC(STR_SONG_DIFF_DENSITY), 0.28f);
                        char objCntStr[32];
                        std::snprintf(objCntStr, sizeof(objCntStr), "%d %s", d.totalObjects(), LOC(STR_OBJECTS));
                        drawTextWithShadow(textBuf, 232.0f, 115.5f, 0.29f, Ui::rgba(225, 220, 245), objCntStr, 0.28f);

                        float playhead = -1.0f;
                        if (audio.getIsPlaying() && d.lastObjectTime > d.firstObjectTime) {
                            int curMs = audio.getTimeMs();
                            if (curMs >= d.firstObjectTime && curMs <= d.lastObjectTime) {
                                playhead = static_cast<float>(curMs - d.firstObjectTime) / static_cast<float>(d.lastObjectTime - d.firstObjectTime);
                            }
                        }
                        Ui::strainGraph(12.0f, 122.5f, 296.0f, 18.0f, 0.22f, d.strainGraph, starClr, playhead);
                    } else {
                        // =====================================================================
                        // 2C. RANKING LOCAL: TABLA DE MEJORES PUNTUACIONES
                        // =====================================================================
                        if (numRecs == 0) {
                            float emptyH = rankingExpanded ? 150.0f : 68.0f;
                            Ui::panel(8.0f, 74.0f, 304.0f, emptyH, 0.20f, Ui::rgba(24, 18, 38, 240), Ui::rgba(65, 40, 85));
                            drawCenteredText(textBuf, 160.0f, 74.0f + emptyH * 0.40f, 0.38f, clrWhite, LOC(STR_SONG_NO_RECORDS_YET));
                            drawCenteredText(textBuf, 160.0f, 74.0f + emptyH * 0.65f, 0.28f, clrCursor, LOC(STR_SONG_PLAY_TO_RECORD));
                        } else if (!rankingExpanded) {
                            // Modo compacto: 3 filas en y: 74..142
                            int displayCount = std::min(numRecs, 3);
                            for (int i = 0; i < displayCount; ++i) {
                                const auto& r = mapRecords[i];
                                float ry = 74.0f + i * 23.0f;
                                u32 gClr = getScoreGradeColor(r.bestGrade, clrCursor);

                                u32 rankClr = (i == 0) ? Ui::rgba(255, 215, 0)
                                            : ((i == 1) ? Ui::rgba(210, 220, 235)
                                            : ((i == 2) ? Ui::rgba(220, 145, 75)
                                                        : Ui::rgba(135, 125, 155)));

                                Ui::gradH(8.0f, ry, 304.0f, 21.0f, 0.20f, Ui::rgba(28, 20, 42, 245), Ui::rgba(18, 14, 28, 245));
                                drawRectOutline(8.0f, ry, 304.0f, 21.0f, 1.0f, (i == 0) ? Ui::withAlpha(rankClr, 140) : Ui::rgba(45, 38, 65, 180), 0.22f);
                                C2D_DrawRectSolid(8.0f, ry, 0.24f, 2.5f, 21.0f, rankClr);

                                // 1. Rango
                                char rnkStr[8];
                                std::snprintf(rnkStr, sizeof(rnkStr), "#%d", i + 1);
                                Ui::pill(12.0f, ry + 3.0f, 22.0f, 15.0f, 0.23f, (i == 0) ? rankClr : Ui::rgba(35, 28, 52, 220));
                                drawCenteredText(textBuf, 23.0f, ry + 10.5f, 0.28f, (i == 0) ? Ui::rgba(20, 16, 28) : rankClr, rnkStr);

                                // 2. Grado
                                Ui::pill(36.0f, ry + 3.0f, 22.0f, 15.0f, 0.23f, Ui::rgba(24, 18, 36, 230));
                                drawRectOutline(36.0f, ry + 3.0f, 22.0f, 15.0f, 1.0f, gClr, 0.24f);
                                drawCenteredText(textBuf, 47.0f, ry + 10.5f, 0.30f, gClr, r.bestGrade.c_str());

                                // 3. Score
                                drawText(textBuf, 62.0f, ry + 3.5f, 0.38f, clrWhite, formatScoreThousands(r.bestScore).c_str(), 0.26f);

                                // 4. Acc & Combo
                                char acBuf[32];
                                std::snprintf(acBuf, sizeof(acBuf), "%.1f%% %dx", r.bestAcc, r.bestCombo);
                                drawText(textBuf, 140.0f, ry + 4.5f, 0.27f, Ui::rgba(130, 240, 190), acBuf, 0.26f);

                                // 5. Mods
                                if (r.bestMods != MOD_NONE) {
                                    std::string mStr = "+" + formatModsString(r.bestMods);
                                    Ui::pill(210.0f, ry + 3.0f, 48.0f, 15.0f, 0.23f, Ui::rgba(75, 30, 95, 220));
                                    drawCenteredText(textBuf, 234.0f, ry + 10.5f, 0.27f, clrCursor, mStr.c_str());
                                } else {
                                    Ui::pill(210.0f, ry + 3.0f, 48.0f, 15.0f, 0.23f, Ui::rgba(34, 28, 48, 200));
                                    drawCenteredText(textBuf, 234.0f, ry + 10.5f, 0.26f, clrMuted, "None");
                                }

                                // 6. PP
                                Ui::pill(262.0f, ry + 3.0f, 46.0f, 15.0f, 0.23f, Ui::rgba(42, 34, 75, 220));
                                char ppBuf[16];
                                std::snprintf(ppBuf, sizeof(ppBuf), "%.0f pp", r.bestPP);
                                drawCenteredText(textBuf, 285.0f, ry + 10.5f, 0.28f, Ui::rgba(205, 190, 255), ppBuf);
                            }
                        } else {
                            // Modo expandido: hasta 6 filas en y: 74..224
                            int maxVisible = 6;
                            int startIdx = std::max(0, std::min(rankingScroll, std::max(0, numRecs - maxVisible)));
                            int endIdx = std::min(numRecs, startIdx + maxVisible);

                            for (int idx = startIdx; idx < endIdx; ++idx) {
                                const auto& r = mapRecords[idx];
                                float ry = 74.0f + (idx - startIdx) * 25.0f;
                                u32 gClr = getScoreGradeColor(r.bestGrade, clrCursor);

                                u32 rankClr = (idx == 0) ? Ui::rgba(255, 215, 0)
                                            : ((idx == 1) ? Ui::rgba(210, 220, 235)
                                            : ((idx == 2) ? Ui::rgba(220, 145, 75)
                                                          : Ui::rgba(135, 125, 155)));

                                Ui::gradH(8.0f, ry, 304.0f, 23.0f, 0.20f, Ui::rgba(28, 20, 42, 245), Ui::rgba(18, 14, 28, 245));
                                drawRectOutline(8.0f, ry, 304.0f, 23.0f, 1.0f, (idx == 0) ? Ui::withAlpha(rankClr, 150) : Ui::rgba(45, 38, 65, 180), 0.22f);
                                C2D_DrawRectSolid(8.0f, ry, 0.24f, 3.0f, 23.0f, rankClr);

                                // 1. Rango
                                char rnkStr[8];
                                std::snprintf(rnkStr, sizeof(rnkStr), "#%d", idx + 1);
                                Ui::pill(12.0f, ry + 3.5f, 24.0f, 16.0f, 0.23f, (idx == 0) ? rankClr : Ui::rgba(35, 28, 52, 220));
                                drawCenteredText(textBuf, 24.0f, ry + 11.5f, 0.30f, (idx == 0) ? Ui::rgba(20, 16, 28) : rankClr, rnkStr);

                                // 2. Grado
                                Ui::pill(38.0f, ry + 3.5f, 24.0f, 16.0f, 0.23f, Ui::rgba(24, 18, 36, 230));
                                drawRectOutline(38.0f, ry + 3.5f, 24.0f, 16.0f, 1.0f, gClr, 0.24f);
                                drawCenteredText(textBuf, 50.0f, ry + 11.5f, 0.32f, gClr, r.bestGrade.c_str());

                                // 3. Score
                                drawText(textBuf, 66.0f, ry + 4.5f, 0.40f, clrWhite, formatScoreThousands(r.bestScore).c_str(), 0.26f);

                                // 4. Acc & Combo
                                char acBuf[32];
                                std::snprintf(acBuf, sizeof(acBuf), "%.1f%%  %dx", r.bestAcc, r.bestCombo);
                                drawText(textBuf, 144.0f, ry + 5.5f, 0.28f, Ui::rgba(130, 240, 190), acBuf, 0.26f);

                                // 5. Mods
                                if (r.bestMods != MOD_NONE) {
                                    std::string mStr = "+" + formatModsString(r.bestMods);
                                    Ui::pill(210.0f, ry + 3.5f, 48.0f, 16.0f, 0.23f, Ui::rgba(75, 30, 95, 220));
                                    drawCenteredText(textBuf, 234.0f, ry + 11.5f, 0.28f, clrCursor, mStr.c_str());
                                } else {
                                    Ui::pill(210.0f, ry + 3.5f, 48.0f, 16.0f, 0.23f, Ui::rgba(34, 28, 48, 200));
                                    drawCenteredText(textBuf, 234.0f, ry + 11.5f, 0.27f, clrMuted, "None");
                                }

                                // 6. PP
                                Ui::pill(262.0f, ry + 3.5f, 46.0f, 16.0f, 0.23f, Ui::rgba(42, 34, 75, 220));
                                char ppBuf[16];
                                std::snprintf(ppBuf, sizeof(ppBuf), "%.0f pp", r.bestPP);
                                drawCenteredText(textBuf, 285.0f, ry + 11.5f, 0.29f, Ui::rgba(205, 190, 255), ppBuf);
                            }

                            // Barra de estado inferior en modo expandido
                            Ui::gradV(8.0f, 224.0f, 304.0f, 15.0f, 0.25f, Ui::rgba(18, 14, 28, 220), Ui::rgba(18, 14, 28, 250));
                            drawText(textBuf, 12.0f, 225.5f, 0.26f, clrMuted, LOC(STR_SONG_CLOSE_HINT), 0.30f);
                            if (startIdx > 0) {
                                drawText(textBuf, 276.0f, 225.5f, 0.28f, clrCursor, "[^]", 0.30f);
                            }
                            if (endIdx < numRecs) {
                                drawText(textBuf, 294.0f, 225.5f, 0.28f, clrCursor, "[v]", 0.30f);
                            }
                        }
                    }
                } else {
                    Ui::panel(8.0f, 6.0f, 304.0f, 136.0f, 0.15f, Ui::rgba(20, 16, 32, 245), Ui::rgba(50, 42, 75));
                    drawCenteredText(textBuf, 160.0f, 55.0f, 0.44f, clrCursor, LOC(STR_SONG_NO_FILTER_MATCH));
                    drawCenteredText(textBuf, 160.0f, 80.0f, 0.36f, clrWhite, LOC(STR_SONG_TAP_SEARCH_CLEAR));
                }

                // =========================================================================
                // 4. BARRA HORIZONTAL DE TARJETAS ESTILO OSU!LAZER (y: 146..222)
                // =========================================================================
                if (!rankingExpanded) {
                    const float cardW = 56.0f;
                    const float cardH = 76.0f;
                    const float baseCardY = 146.0f;

                for (int k = 0; k < 5; ++k) {
                    float lift = footerCardLift[k];
                    float bx = 8.0f + k * 62.0f;
                    float by = baseCardY - (lift * 9.0f);
                    float cx = bx + (cardW * 0.5f);
                    float cy = by + 23.0f; // Centro del icono grande
                    float cz = 0.20f;

                    // Colores temáticos y acentos por tarjeta
                    u32 accentClr = clrWhite;
                    u32 bgBase    = Ui::rgba(22, 18, 34, 245);
                    const char* cardTitle = "";
                    char cardSub[32] = "";
                    u32 subClr = clrMuted;

                    if (k == 0) { // MODO TACTIL / TABLET (TOGGLE UNIFICADO)
                        if (multiMgr.inMultiplayer) {
                            accentClr = clrCursor;
                            bgBase    = Ui::rgba(22, 45, 65, 245);
                            cardTitle = LOC(STR_CARD_ROOM);
                            std::snprintf(cardSub, sizeof(cardSub), "%s", multiMgr.isHost() ? LOC(STR_CARD_HOST) : LOC(STR_CARD_GUEST));
                            subClr = clrCursor;
                        } else {
                            bool bTactil = settings.playOnBottomScreen;
                            accentClr = bTactil ? clr100 : clrPrimary;
                            bgBase    = bTactil ? Ui::rgba(24, 52, 40, 245) : Ui::rgba(36, 26, 64, 245);
                            cardTitle = bTactil ? LOC(STR_CARD_TOUCH) : LOC(STR_CARD_TABLET);
                            std::snprintf(cardSub, sizeof(cardSub), "%s", bTactil ? LOC(STR_CARD_DOWN) : LOC(STR_CARD_UP));
                            subClr = bTactil ? Ui::rgba(140, 245, 140) : Ui::rgba(195, 175, 255);
                        }
                    } else if (k == 1) { // BUSCAR
                        bool hasFilter = !songMgr.searchQuery.empty();
                        accentClr = hasFilter ? clrCursor : Ui::rgba(0, 216, 240);
                        bgBase    = hasFilter ? Ui::rgba(22, 45, 65, 245) : Ui::rgba(18, 26, 42, 245);
                        cardTitle = LOC(STR_SEARCH);
                        if (hasFilter) {
                            std::snprintf(cardSub, sizeof(cardSub), "%s", truncateString(songMgr.searchQuery, 7).c_str());
                            subClr = clrCursor;
                        } else {
                            std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_FILTER));
                            subClr = clrMuted;
                        }
                    } else if (k == 2) { // MODS
                        bool hasMods = (settings.activeMods != MOD_NONE);
                        accentClr = hasMods ? Ui::rgba(255, 102, 171) : Ui::rgba(240, 110, 185);
                        bgBase    = (hasMods || modMenuOpen) ? Ui::rgba(54, 20, 50, 245) : Ui::rgba(26, 18, 36, 245);
                        cardTitle = "MODS";
                        if (hasMods) {
                            std::string mStr = "+" + formatModsString(settings.activeMods);
                            std::snprintf(cardSub, sizeof(cardSub), "%s", mStr.c_str());
                            subClr = clrCursor;
                        } else {
                            std::snprintf(cardSub, sizeof(cardSub), "SELECT");
                            subClr = clrMuted;
                        }
                    } else if (k == 3) { // ORDEN
                        accentClr = Ui::rgba(165, 120, 255);
                        bgBase    = Ui::rgba(26, 22, 46, 245);
                        cardTitle = LOC(STR_CARD_ORDER);
                        if (songMgr.sortMode == SORT_BY_TITLE)       std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_TITLE));
                        else if (songMgr.sortMode == SORT_BY_ARTIST) std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_ARTIST));
                        else if (songMgr.sortMode == SORT_BY_STARS)  std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_STARS));
                        else                                         std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_RECORD));
                        subClr = Ui::rgba(205, 185, 255);
                    } else if (k == 4) { // MODOS DE JUEGO (STD / TAIKO / CATCH / MANIA)
                        if (multiMgr.inMultiplayer) {
                            accentClr = Ui::rgba(180, 80, 240);
                            bgBase    = Ui::rgba(40, 20, 52, 245);
                            cardTitle = "ONLINE";
                            std::snprintf(cardSub, sizeof(cardSub), "Lobby");
                            subClr    = Ui::rgba(215, 160, 255);
                        } else {
                            cardTitle = LOC(STR_MODE);
                            int curM = settings.gameMode;
                            if (curM == MODE_TAIKO) {
                                accentClr = Ui::rgba(255, 75, 80);
                                bgBase    = Ui::rgba(44, 18, 24, 245);
                                std::snprintf(cardSub, sizeof(cardSub), "Taiko");
                                subClr    = Ui::rgba(255, 150, 160);
                            } else if (curM == MODE_CATCH) {
                                accentClr = Ui::rgba(45, 215, 115);
                                bgBase    = Ui::rgba(16, 42, 26, 245);
                                std::snprintf(cardSub, sizeof(cardSub), "Catch");
                                subClr    = Ui::rgba(130, 255, 180);
                            } else if (curM == MODE_MANIA) {
                                accentClr = Ui::rgba(70, 195, 255);
                                bgBase    = Ui::rgba(18, 36, 52, 245);
                                std::snprintf(cardSub, sizeof(cardSub), "Mania");
                                subClr    = Ui::rgba(160, 230, 255);
                            } else {
                                accentClr = Ui::rgba(255, 102, 171);
                                bgBase    = Ui::rgba(48, 18, 38, 245);
                                std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_STANDARD));
                                subClr    = Ui::rgba(255, 180, 215);
                            }
                        }
                    }

                    // Sombra inferior cuando la tarjeta se eleva
                    if (lift > 0.02f) {
                        C2D_DrawRectSolid(bx + 2.0f, by + cardH, 0.17f, cardW - 4.0f, 2.0f + lift * 5.0f, Ui::rgba(0, 0, 0, static_cast<u8>(80 * lift)));
                        Ui::glow(cx, by + (cardH * 0.5f), 0.18f, 32.0f, Ui::withAlpha(accentClr, static_cast<u8>(90 * lift)), 3);
                    }

                    // Fondo de la tarjeta con estilo button multijugador
                    u32 finalBg = (lift > 0.05f) ? Ui::brighten(bgBase, 1.0f + lift * 0.35f) : bgBase;
                    Ui::button(bx, by, cardW, cardH, cz, finalBg, accentClr, lift > 0.05f);

                    // =====================================================================
                    // DIBUJADO DE ICONOS VECTORIALES PROCEDURALES (GRANDES Y CENTRADOS)
                    // =====================================================================
                    float iz = cz + 0.03f;
                    u32 iconClr = (lift > 0.05f) ? clrWhite : accentClr;

                    if (k == 0) {
                        // Icono 0: Modo Táctil / Tablet (Consola 3DS con pantalla activa y stylus)
                        bool bTactil = settings.playOnBottomScreen;
                        if (bTactil) {
                            // Pantalla Superior (contorno sutil)
                            drawRectOutline(cx - 13.0f, cy - 13.0f, 26.0f, 11.0f, 1.2f, Ui::rgba(110, 105, 135, 180), iz);
                            // Pantalla Inferior (activa, brillante, táctil)
                            Ui::panel(cx - 11.0f, cy + 1.0f, 22.0f, 11.0f, iz, Ui::rgba(25, 95, 55, 245), clr100);
                            // Stylus tocando la pantalla táctil
                            C2D_DrawLine(cx + 14.0f, cy - 4.0f, clrWhite, cx + 3.0f, cy + 6.0f, clr100, 2.2f, iz + 0.01f);
                            C2D_DrawCircleSolid(cx + 3.0f, cy + 6.0f, iz + 0.02f, 1.8f, clrWhite);
                        } else {
                            // Pantalla Superior (activa, brillante, juego en 3D)
                            Ui::panel(cx - 13.0f, cy - 13.0f, 26.0f, 12.0f, iz, Ui::rgba(65, 45, 120, 245), clrPrimary);
                            // Pantalla Inferior (touchpad / tablet)
                            drawRectOutline(cx - 11.0f, cy + 2.0f, 22.0f, 10.0f, 1.2f, Ui::rgba(110, 105, 135, 180), iz);
                            // Stylus apuntando a la pantalla superior
                            C2D_DrawLine(cx - 4.0f, cy + 9.0f, clrWhite, cx + 5.0f, cy - 6.0f, clrPrimary, 2.2f, iz + 0.01f);
                            C2D_DrawCircleSolid(cx + 5.0f, cy - 6.0f, iz + 0.02f, 1.8f, clrWhite);
                        }
                    } else if (k == 1) {
                        // Icono 1: Buscar (Lupa estilizada grande)
                        float lcx = cx - 3.5f;
                        float lcy = cy - 3.5f;
                        float lRadius = 8.5f;
                        // Relleno de cristal
                        C2D_DrawCircleSolid(lcx, lcy, iz, lRadius, Ui::rgba(0, 180, 230, 45));
                        // Borde metálico de la lente
                        drawRing(lcx, lcy, lRadius, 2.2f, iconClr, 22, iz + 0.01f);
                        // Brillo especular
                        C2D_DrawCircleSolid(lcx - 3.5f, lcy - 3.5f, iz + 0.02f, 1.8f, clrWhite);
                        // Mango diagonal de la lupa
                        C2D_DrawLine(lcx + 6.0f, lcy + 6.0f, iconClr, lcx + 14.0f, lcy + 14.0f, iconClr, 3.2f, iz + 0.01f);
                        C2D_DrawCircleSolid(lcx + 14.0f, lcy + 14.0f, iz + 0.02f, 2.0f, iconClr);
                    } else if (k == 2) {
                        // Icono 2: Mods (Flechas dinámicas de intercambio estilo osu! Lazer)
                        // Flecha superior hacia la derecha
                        float aY1 = cy - 5.5f;
                        C2D_DrawLine(cx - 12.0f, aY1, iconClr, cx + 5.0f, aY1, iconClr, 2.4f, iz);
                        C2D_DrawTriangle(cx + 5.0f, aY1 - 5.0f, iconClr, cx + 12.0f, aY1, iconClr, cx + 5.0f, aY1 + 5.0f, iconClr, iz + 0.01f);
                        // Flecha inferior hacia la izquierda
                        float aY2 = cy + 5.5f;
                        C2D_DrawLine(cx + 12.0f, aY2, iconClr, cx - 5.0f, aY2, iconClr, 2.4f, iz);
                        C2D_DrawTriangle(cx - 5.0f, aY2 - 5.0f, iconClr, cx - 12.0f, aY2, iconClr, cx - 5.0f, aY2 + 5.0f, iconClr, iz + 0.01f);
                    } else if (k == 3) {
                        // Icono 3: Orden (Barras ascendentes de estadísticas + Flecha)
                        Ui::panel(cx - 12.0f, cy + 2.0f, 6.0f, 10.0f, iz, iconClr);
                        Ui::panel(cx - 3.0f,  cy - 4.0f, 6.0f, 16.0f, iz, iconClr);
                        Ui::panel(cx + 6.0f,  cy - 10.0f, 6.0f, 22.0f, iz, iconClr);
                        // Triángulo apuntando hacia arriba
                        C2D_DrawTriangle(cx + 5.0f, cy - 12.0f, clrWhite, cx + 13.0f, cy - 12.0f, clrWhite, cx + 9.0f, cy - 17.0f, clrWhite, iz + 0.01f);
                    } else if (k == 4) {
                        // Icono 4: Modos de juego con icono dinámico según el modo activo
                        int curM = settings.gameMode;
                        if (curM == MODE_TAIKO) {
                            // Icono TAIKO: Tambor Wadaiko estilizado con cara roja, aro azul y bachi
                            drawRing(cx, cy, 11.5f, 2.4f, Ui::rgba(55, 140, 255), 24, iz); // Aro Katsu (azul)
                            C2D_DrawCircleSolid(cx, cy, iz + 0.01f, 8.5f, Ui::rgba(255, 60, 65)); // Centro Don (rojo)
                            drawRing(cx, cy, 8.5f, 1.4f, clrWhite, 20, iz + 0.015f);
                            // Palillos de tambor (bachi) cruzados sutiles
                            C2D_DrawLine(cx - 7.0f, cy + 8.5f, clrWhite, cx + 2.5f, cy - 2.5f, clrWhite, 2.0f, iz + 0.02f);
                            C2D_DrawLine(cx + 7.0f, cy + 8.5f, clrWhite, cx - 2.5f, cy - 2.5f, clrWhite, 2.0f, iz + 0.02f);
                        } else if (curM == MODE_CATCH) {
                            // Icono CATCH: Bandeja receptora y fruta jugosa con hoja
                            // Bandeja de Catcher
                            C2D_DrawLine(cx - 11.0f, cy + 9.0f, clrWhite, cx + 11.0f, cy + 9.0f, clrWhite, 2.4f, iz);
                            C2D_DrawLine(cx - 11.0f, cy + 9.0f, clrWhite, cx - 13.0f, cy + 5.5f, clrWhite, 2.0f, iz);
                            C2D_DrawLine(cx + 11.0f, cy + 9.0f, clrWhite, cx + 13.0f, cy + 5.5f, clrWhite, 2.0f, iz);
                            // Manzana estilizada
                            C2D_DrawCircleSolid(cx - 3.8f, cy - 0.5f, iz + 0.01f, 6.2f, Ui::rgba(255, 50, 70));
                            C2D_DrawCircleSolid(cx + 3.8f, cy - 0.5f, iz + 0.01f, 6.2f, Ui::rgba(255, 50, 70));
                            C2D_DrawCircleSolid(cx - 4.0f, cy - 3.0f, iz + 0.02f, 1.8f, Ui::rgba(255, 200, 200, 230)); // Brillo
                            // Tallo y hoja verde
                            C2D_DrawLine(cx, cy - 5.5f, Ui::rgba(140, 80, 45), cx + 2.0f, cy - 10.5f, Ui::rgba(140, 80, 45), 1.8f, iz + 0.02f);
                            C2D_DrawTriangle(cx + 2.0f, cy - 8.5f, Ui::rgba(45, 215, 80), cx + 7.5f, cy - 11.5f, Ui::rgba(45, 215, 80), cx + 6.0f, cy - 7.0f, Ui::rgba(45, 215, 80), iz + 0.03f);
                        } else if (curM == MODE_MANIA) {
                            // Icono MANIA: Teclado de 4 columnas (4K) estilizado
                            float colW = 4.2f;
                            float colH = 20.0f;
                            float startX = cx - 10.5f;
                            float topY = cy - 10.0f;
                            for (int col = 0; col < 4; ++col) {
                                float kx = startX + col * 5.5f;
                                u32 kColor = (col == 0 || col == 3) ? clrWhite : Ui::rgba(60, 205, 255);
                                Ui::panel(kx, topY, colW, colH, iz, Ui::rgba(20, 26, 40, 240), kColor);
                                // Destello de pulsación inferior
                                C2D_DrawRectSolid(kx + 0.5f, topY + colH - 4.5f, iz + 0.01f, colW - 1.0f, 4.0f, kColor);
                            }
                        } else {
                            // Icono STD: Círculo de golpe concéntrico con approach circle
                            drawRing(cx, cy, 12.0f, 1.6f, iconClr, 24, iz); // Approach circle
                            C2D_DrawCircleSolid(cx, cy, iz + 0.01f, 8.0f, Ui::rgba(255, 102, 171, 230)); // Hit circle
                            drawRing(cx, cy, 8.0f, 1.8f, clrWhite, 20, iz + 0.02f); // Borde blanco
                            C2D_DrawCircleSolid(cx, cy, iz + 0.03f, 2.5f, clrWhite); // Punto central "1"
                        }
                    }

                    // Títulos limpios de la tarjeta (texto legible en ASCII sin emojis ni '?')
                    drawCenteredTextWithShadow(textBuf, cx, by + 49.5f, 0.40f, clrWhite, cardTitle, cz + 0.05f);
                    Ui::pill(bx + 4.0f, by + 59.5f, cardW - 8.0f, 13.5f, cz + 0.04f, Ui::rgba(14, 11, 22, 230));
                    drawCenteredTextWithShadow(textBuf, cx, by + 66.0f, 0.31f, subClr, cardSub, cz + 0.06f);
                }
                }
            }

            if (deleteConfirmOpen) {
                C2D_DrawRectSolid(0.0f, 0.0f, 0.85f, 320.0f, 240.0f, Ui::rgba(8, 6, 14, 235));
                Ui::panel(16.0f, 38.0f, 288.0f, 156.0f, 0.88f, Ui::rgba(26, 18, 34, 252), Ui::rgba(255, 64, 96));
                Ui::pill(24.0f, 46.0f, 96.0f, 18.0f, 0.90f, Ui::rgba(180, 30, 55, 230));
                drawCenteredText(textBuf, 72.0f, 55.0f, 0.36f, clrWhite, LOC(STR_DELETE), 0.95f);

                std::string delTitle = (!songMgr.sets.empty() && selectedSet >= 0 && selectedSet < static_cast<int>(songMgr.sets.size()))
                    ? songMgr.sets[selectedSet].title : "";
                drawCenteredText(textBuf, 160.0f, 82.0f, 0.44f, clrWhite, truncateString(delTitle, 28).c_str(), 0.95f);
                drawCenteredText(textBuf, 160.0f, 108.0f, 0.35f, Ui::rgba(255, 160, 175), LOC(STR_DELETE_CONFIRM_TITLE), 0.95f);
                drawCenteredText(textBuf, 160.0f, 126.0f, 0.32f, clrMuted, LOC(STR_DELETE_CONFIRM_DESC), 0.95f);

                // Boton Cancelar
                Ui::button(28.0f, 148.0f, 126.0f, 34.0f, 0.90f, Ui::rgba(36, 32, 50, 250), Ui::rgba(85, 75, 110), false);
                Ui::pill(34.0f, 155.0f, 22.0f, 20.0f, 0.92f, Ui::rgba(65, 58, 85));
                drawCenteredText(textBuf, 45.0f, 165.0f, 0.32f, clrWhite, "B", 0.95f);
                drawTextWithShadow(textBuf, 62.0f, 157.0f, 0.38f, clrWhite, LOC(STR_BACK), 0.95f);

                // Boton Borrar
                Ui::button(166.0f, 148.0f, 126.0f, 34.0f, 0.90f, Ui::rgba(165, 28, 48, 250), Ui::rgba(255, 80, 110), true);
                Ui::pill(172.0f, 155.0f, 22.0f, 20.0f, 0.92f, Ui::rgba(255, 80, 110));
                drawCenteredText(textBuf, 183.0f, 165.0f, 0.32f, clrWhite, "A", 0.95f);
                drawTextWithShadow(textBuf, 200.0f, 157.0f, 0.38f, clrWhite, LOC(STR_DELETE), 0.95f);
            }
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);

            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 4. PANTALLA DE RESULTADOS (Ranking Final + PP + Récords)
        // =========================================================
        if (appState == STATE_RESULTS) {
            if (multiMgr.inMultiplayer) {
                appState = STATE_MULTI_RESULTS;
                continue;
            }
            if (kDown & KEY_A) {
                startSelectedBeatmap();
                continue;
            }
            if (kDown & (KEY_B | KEY_START | KEY_SELECT)) {
                audio.playHitSound(0);
                applyActiveSkinAudio();
                appState = STATE_SONG_SELECT;
                updateSongPreview();
                continue;
            }

            static int resultsBottomTab = 0;
            if (kDown & (KEY_X | KEY_Y)) {
                resultsBottomTab = 1 - resultsBottomTab;
                audio.playHitSound(0);
            }
            if (touchJustDown) {
                if (touch.py >= 46 && touch.py <= 68) {
                    if (touch.px < 160) resultsBottomTab = 0;
                    else resultsBottomTab = 1;
                    audio.playHitSound(0);
                } else if (touch.py >= 200 && touch.py <= 236) {
                    if (touch.px >= 8 && touch.px <= 106) {
                        resultsBottomTab = 1 - resultsBottomTab;
                        audio.playHitSound(0);
                    } else if (touch.px >= 111 && touch.px <= 211) {
                        audio.playHitSound(2);
                        startSelectedBeatmap();
                        continue;
                    } else if (touch.px >= 216 && touch.px <= 314) {
                        audio.playHitSound(0);
                        applyActiveSkinAudio();
                        appState = STATE_SONG_SELECT;
                        updateSongPreview();
                        continue;
                    }
                }
            }

            bool isTaikoPlay = (settings.gameMode == MODE_TAIKO || beatmap.mode == 1);
            bool isCatchPlay = (settings.gameMode == MODE_CATCH || beatmap.mode == 2);
            bool isManiaPlay = (settings.gameMode == MODE_MANIA || beatmap.mode == 3);
            int currentPlayMode = isTaikoPlay ? 1 : (isCatchPlay ? 2 : (isManiaPlay ? 3 : 0));

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

            char buf[96];
            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);

                float dBg    = depthShift(-3.5f);
                float dGrade = depthShift(4.2f);
                float dPanel = depthShift(2.0f);

                if (bgTex.loaded) {
                    bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 12, 22, 190));
                } else {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), gradeClr, 0.02f);
                }

                Ui::gradV(0.0f, 0.0f, 400.0f, 30.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 150));
                C2D_DrawRectSolid(0.0f, 29.0f, 0.31f, 400.0f, 1.5f, Ui::withAlpha(gradeClr, 220));
                const char* resTitle = stageFailed ? LOC(STR_RESULTS_FAILED) : (newRecordAchieved ? LOC(STR_RESULTS_NEW_RECORD) : LOC(STR_RESULTS_TITLE));
                std::string taikoFailTitle = std::string(LOC(STR_RESULTS_FAILED)) + " (ざんねん...)";
                if (isTaikoPlay) {
                    if (stageFailed || hp < 0.70f) resTitle = taikoFailTitle.c_str();
                    else if (countMiss == 0) resTitle = "DON-DERFUL! FULL COMBO! (フルコンボ!)";
                    else resTitle = "CLEAR! (クリア!)";
                }
                drawText(textBuf, 12.0f, 6.0f, 0.55f, clrWhite, resTitle);

                // Rango gigante animado o Corona oficial estilo Taiko
                float gPulse = 1.0f + std::sin(animTick * 0.08f) * 0.03f;
                float gIntro = std::min(1.0f, static_cast<float>(animTick) / 20.0f);
                float gScale = (gIntro < 1.0f) ? (1.0f + 0.35f * std::sin(gIntro * static_cast<float>(M_PI))) : 1.0f;

                if (!isTaikoPlay) {
                    // Rayos de sol rotativos dorados para SS y S
                    if (std::string(grade) == "SS" || std::string(grade) == "S") {
                        for (int ray = 0; ray < 8; ++ray) {
                            float rAng = ray * (static_cast<float>(M_PI) / 4.0f) + animTick * 0.025f;
                            float rLen = 58.0f * gPulse;
                            C2D_DrawLine(88.0f + dGrade, 124.0f, Ui::rgba(255, 220, 60, 65),
                                         88.0f + dGrade + std::cos(rAng) * rLen, 124.0f + std::sin(rAng) * rLen,
                                         Ui::rgba(255, 220, 60, 0), 2.2f, 0.16f);
                        }
                    }

                    Ui::glow(88.0f + dGrade, 124.0f, 0.15f, 78.0f * gPulse * gScale, Ui::withAlpha(gradeClr, 120), 6);
                    C2D_DrawCircleSolid(90.0f + dGrade, 128.0f, 0.18f, 52.0f * gScale, Ui::rgba(0, 0, 0, 110));
                    C2D_DrawCircleSolid(88.0f + dGrade, 124.0f, 0.2f, 52.0f * gScale, Ui::rgba(24, 20, 40, 245));
                    Ui::ring(88.0f + dGrade, 124.0f, 0.25f, 52.0f * gPulse * gScale, 4.0f, gradeClr, 40);
                    drawCenteredText(textBuf, 88.0f + dGrade, 120.0f, 1.55f * gScale, gradeClr, grade);

                    // Badge de FULL COMBO debajo de la medalla de grado
                    if (countMiss == 0 && !stageFailed) {
                        Ui::pill(48.0f + dGrade, 182.0f, 80.0f, 15.0f, 0.28f, Ui::rgba(255, 215, 60, 230));
                        drawCenteredText(textBuf, 88.0f + dGrade, 189.5f, 0.30f, Ui::rgba(20, 16, 28), "FULL COMBO", 0.30f);
                    }
                } else {
                    // Corona o Medalla oficial estilo Taiko no Tatsujin
                    bool isFC = (countMiss == 0 && !stageFailed);
                    bool isClear = (hp >= 0.70f && !stageFailed);
                    u32 crownClr = isFC ? Ui::rgba(255, 218, 55) : (isClear ? Ui::rgba(215, 235, 255) : Ui::rgba(110, 30, 75));
                    Ui::glow(88.0f + dGrade, 112.0f, 0.15f, 76.0f * gPulse, Ui::withAlpha(crownClr, 150), 6);

                    // Base de la Corona
                    float crownX = 88.0f + dGrade;
                    float crownY = 108.0f;
                    C2D_DrawRectSolid(crownX - 32.0f, crownY + 8.0f, 0.20f, 64.0f, 12.0f, crownClr);
                    C2D_DrawRectSolid(crownX - 30.0f, crownY + 16.0f, 0.21f, 60.0f, 4.0f, Ui::rgba(255, 255, 255, 200));

                    // 5 Picos dorados de la corona
                    for (int p = 0; p < 5; ++p) {
                        float px = crownX - 28.0f + p * 14.0f;
                        float peakH = (p == 2) ? 24.0f : ((p == 1 || p == 3) ? 17.0f : 13.0f);
                        C2D_DrawTriangle(px - 6.0f, crownY + 8.0f, crownClr, px + 6.0f, crownY + 8.0f, crownClr, px, crownY + 8.0f - peakH, crownClr, 0.21f);
                        // Joya en la punta
                        u32 gemClr = isFC ? Ui::rgba(255, 45, 60) : (isClear ? Ui::rgba(45, 175, 255) : Ui::rgba(140, 45, 120));
                        C2D_DrawCircleSolid(px, crownY + 8.0f - peakH, 0.22f, 2.8f, gemClr);
                    }

                    // Cinta con título
                    const char* crownTxt = isFC ? "フルコンボ!" : (isClear ? "クリア!" : "ざんねん...");
                    Ui::pill(crownX - 40.0f, crownY + 24.0f, 80.0f, 17.0f, 0.24f, crownClr);
                    drawCenteredText(textBuf, crownX, crownY + 32.0f, 0.32f, Ui::rgba(20, 14, 26), crownTxt, 0.26f);

                    // Mini Don-chan de celebración animado bajo la corona
                    int donResEmotion = isFC ? 1 : (isClear ? 0 : 2);
                    float dMascotY = 175.0f;
                    C2D_DrawCircleSolid(crownX, dMascotY, 0.22f, 16.0f, Ui::rgba(255, 55, 65));
                    ModeTaiko::drawTaikoFace(crownX, dMascotY, 13.0f, false, true, 0.24f, donResEmotion);
                }

                // Panel de resultados
                Ui::panel(158.0f + dPanel, 38.0f, 234.0f, 62.0f, 0.3f, Ui::rgba(24, 21, 40, 230), gradeClr);
                drawText(textBuf, 168.0f + dPanel, 42.0f, 0.36f, clrMuted, LOC(STR_SCORE));

                float rollProgress = std::min(1.0f, static_cast<float>(animTick) / 30.0f);
                int displayScore = static_cast<int>(score * rollProgress);
                std::snprintf(buf, sizeof(buf), "%07d", displayScore);
                drawText(textBuf, 168.0f + dPanel, 54.0f, 0.78f, clrWhite, buf);
                float modStars = getModAdjustedStars(beatmap.starRating, settings.activeMods);
                std::snprintf(buf, sizeof(buf), "%.1f pp", lastEarnedPP);
                Ui::pill(318.0f + dPanel, 46.0f, 66.0f, 18.0f, 0.5f, Ui::rgba(70, 55, 110, 240));
                drawCenteredText(textBuf, 351.0f + dPanel, 55.0f, 0.42f, clr300, buf, 0.9f);
                std::snprintf(buf, sizeof(buf), "*%.2f", modStars);
                Ui::pill(318.0f + dPanel, 70.0f, 66.0f, 16.0f, 0.5f, getStarRatingColor(modStars));
                drawCenteredText(textBuf, 351.0f + dPanel, 78.0f, 0.36f, Ui::rgba(20, 16, 30), buf, 0.9f);

                // Precision con barra
                Ui::panel(158.0f + dPanel, 106.0f, 234.0f, 34.0f, 0.3f, Ui::rgba(24, 21, 40, 230));
                std::snprintf(buf, sizeof(buf), "%s %.2f%%", LOC(STR_ACCURACY), acc);
                drawText(textBuf, 168.0f + dPanel, 109.0f, 0.40f, clr300, buf);
                std::snprintf(buf, sizeof(buf), "%s %dx", LOC(STR_MAX_COMBO), maxCombo);
                drawText(textBuf, 320.0f + dPanel, 109.0f, 0.40f, clrCursor, buf);
                Ui::statBar(168.0f + dPanel, 128.0f, 214.0f, 6.0f, 0.4f, (acc / 100.0f) * rollProgress, clr300);

                // Contadores de golpes adaptados a cada modo de juego
                const char* hitLbl[4] = { "300", "100", "50", "X" };
                const char* taikoHitLbl[4] = { "良 GOOD", "可 OK", "不可 MISS", "連打 ROLLS" };
                const char* catchHitLbl[4] = { LOC(STR_CATCH_FRUITS), LOC(STR_CATCH_DROPS), LOC(STR_CATCH_DROPLETS), "MISS" };
                const char* maniaHitLbl[4] = { "MAX 300", "100", "50", "MISS" };

                int hitCnt[4] = { count300, count100, count50, countMiss };
                u32 hitClr[4] = { clr300, clr100, clr50, clrMiss };
                u32 taikoHitClr[4] = { Ui::rgba(240, 50, 55), Ui::rgba(45, 160, 230), Ui::rgba(180, 50, 140), Ui::rgba(255, 215, 55) };
                u32 catchHitClr[4] = { ModeCatch::CLR_APPLE, ModeCatch::CLR_DROPLET, Ui::rgba(180, 220, 255), clrMiss };
                u32 maniaHitClr[4] = { ModeMania::CLR_COL_INNER, ModeMania::CLR_COL_OUTER, clr50, clrMiss };

                if (isTaikoPlay) {
                    hitCnt[0] = count300;
                    hitCnt[1] = count100;
                    hitCnt[2] = countMiss;
                    hitCnt[3] = taikoEngine.totalDrumrollHits;
                } else if (isCatchPlay) {
                    hitCnt[0] = count300;
                    hitCnt[1] = count100;
                    hitCnt[2] = count50;
                    hitCnt[3] = countMiss;
                }

                for (int i = 0; i < 4; ++i) {
                    float hx = 158.0f + i * 59.5f + dPanel;
                    const char* curLbl = isTaikoPlay ? taikoHitLbl[i] : (isCatchPlay ? catchHitLbl[i] : (isManiaPlay ? maniaHitLbl[i] : hitLbl[i]));
                    u32 curClr = isTaikoPlay ? taikoHitClr[i] : (isCatchPlay ? catchHitClr[i] : (isManiaPlay ? maniaHitClr[i] : hitClr[i]));

                    Ui::panel(hx, 146.0f, 55.0f, 44.0f, 0.3f, Ui::rgba(24, 21, 40, 230));
                    C2D_DrawRectSolid(hx, 146.0f, 0.4f, 55.0f, 3.0f, curClr);
                    drawCenteredText(textBuf, hx + 27.5f, 160.0f, (isTaikoPlay || isCatchPlay || isManiaPlay) ? 0.26f : 0.42f, curClr, curLbl, 0.9f);
                    int rollCnt = static_cast<int>(hitCnt[i] * rollProgress);
                    std::snprintf(buf, sizeof(buf), "%d", rollCnt);
                    drawCenteredText(textBuf, hx + 27.5f, 177.0f, 0.50f, clrWhite, buf, 0.9f);
                }
                char urLine[96];
                std::snprintf(urLine, sizeof(urLine), "Mods: %s  |  UR: %.1f (%+.0f ms)",
                    formatModsString(settings.activeMods).c_str(), lastUnstableRate, lastMeanHitError);
                drawText(textBuf, 160.0f + dPanel, 196.0f, 0.35f, clrMuted, urLine);

                Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
                drawCenteredText(textBuf, 200.0f, 227.0f, 0.42f, clr300, LOC(STR_RESULTS_RETRY_HINT));
                Ui::drawTransition(400.0f, 240.0f, screenFade, gradeClr);
            }

            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), gradeClr, 0.02f);

            // 1. Cabecera con metadatos del mapa y PP ganado
            Ui::panel(8.0f, 6.0f, 304.0f, 38.0f, 0.15f, Ui::rgba(20, 16, 32, 245), gradeClr);
            drawText(textBuf, 16.0f, 8.0f, 0.42f, clrWhite, truncateString(beatmap.title, 26).c_str(), 0.22f);
            char subMeta[64];
            std::snprintf(subMeta, sizeof(subMeta), "[%s]  //  Por %s",
                          truncateString(beatmap.version, 16).c_str(), truncateString(beatmap.creator, 14).c_str());
            drawText(textBuf, 16.0f, 24.5f, 0.28f, clrMuted, subMeta, 0.22f);

            float modStars = getModAdjustedStars(beatmap.starRating, settings.activeMods);
            u32 stClr = getStarRatingColor(modStars);
            char stBadge[24];
            std::snprintf(stBadge, sizeof(stBadge), "* %.2f", modStars);
            Ui::pill(248.0f, 8.0f, 56.0f, 15.0f, 0.20f, stClr);
            drawCenteredText(textBuf, 276.0f, 15.5f, 0.33f, Ui::rgba(20, 16, 30), stBadge, 0.25f);

            std::snprintf(buf, sizeof(buf), "%.1f pp", lastEarnedPP);
            Ui::pill(248.0f, 25.0f, 56.0f, 15.0f, 0.20f, Ui::rgba(50, 40, 80, 230));
            drawCenteredText(textBuf, 276.0f, 32.5f, 0.33f, clr300, buf, 0.25f);

            // 2. Pestañas interactivas de Selección (Curva de Esfuerzo vs Ranking Local)
            float tabW = 148.0f;
            u32 tab0Bg = (resultsBottomTab == 0) ? Ui::rgba(52, 42, 88, 245) : Ui::rgba(20, 16, 32, 210);
            u32 tab1Bg = (resultsBottomTab == 1) ? Ui::rgba(52, 42, 88, 245) : Ui::rgba(20, 16, 32, 210);
            u32 tab0Brd = (resultsBottomTab == 0) ? clrCursor : Ui::rgba(55, 45, 80);
            u32 tab1Brd = (resultsBottomTab == 1) ? clrCursor : Ui::rgba(55, 45, 80);

            Ui::panel(8.0f, 48.0f, tabW, 19.0f, 0.18f, tab0Bg, tab0Brd);
            drawCenteredText(textBuf, 8.0f + tabW * 0.5f, 57.5f, 0.30f, (resultsBottomTab == 0) ? clrWhite : clrMuted, LOC(STR_RESULTS_TAB_STRAIN), 0.22f);

            Ui::panel(164.0f, 48.0f, tabW, 19.0f, 0.18f, tab1Bg, tab1Brd);
            drawCenteredText(textBuf, 164.0f + tabW * 0.5f, 57.5f, 0.30f, (resultsBottomTab == 1) ? clrWhite : clrMuted, LOC(STR_RESULTS_TAB_RANKING), 0.22f);

            if (resultsBottomTab == 0) {
                // VISTA 1: CURVA DE ESFUERZO (Strain Graph) + Historial de Fallos
                Ui::panel(8.0f, 70.0f, 304.0f, 96.0f, 0.18f, Ui::rgba(18, 14, 28, 250), clrPrimary);

                // Leyenda de marcas
                if (isTaikoPlay) {
                    drawText(textBuf, 184.0f, 73.0f, 0.26f, Ui::rgba(45, 160, 230), "o 可", 0.22f);
                    drawText(textBuf, 240.0f, 73.0f, 0.26f, clrMiss, "x 不可", 0.22f);
                } else {
                    drawText(textBuf, 184.0f, 73.0f, 0.26f, clr100, "o 100", 0.22f);
                    drawText(textBuf, 224.0f, 73.0f, 0.26f, clr50,  "o 50", 0.22f);
                    drawText(textBuf, 260.0f, 73.0f, 0.26f, clrMiss,"x Miss", 0.22f);
                }

                float graphX = 12.0f, graphY = 82.0f, graphW = 296.0f, graphH = 68.0f;
                int firstT = beatmap.firstObjectTime;
                int lastT  = beatmap.lastObjectTime;
                if (lastT <= firstT) lastT = firstT + 1000;
                float totalSpan = static_cast<float>(lastT - firstT);

                // Tramos de descanso sombreados en el gráfico
                for (const auto& brk : beatmap.breaks) {
                    if (brk.endTime > brk.startTime && brk.startTime >= firstT && brk.startTime <= lastT) {
                        float bx0 = graphX + (std::max(0.0f, static_cast<float>(brk.startTime - firstT)) / totalSpan) * graphW;
                        float bx1 = graphX + (std::min(totalSpan, static_cast<float>(brk.endTime - firstT)) / totalSpan) * graphW;
                        if (bx1 > bx0) {
                            C2D_DrawRectSolid(bx0, graphY, 0.19f, bx1 - bx0, graphH, Ui::rgba(40, 32, 65, 140));
                            drawCenteredText(textBuf, (bx0 + bx1) * 0.5f, graphY + graphH * 0.5f, 0.24f, Ui::rgba(160, 150, 190), "BREAK", 0.20f);
                        }
                    }
                }

                // Gráfico Strain Graph relleno
                Ui::strainGraph(graphX, graphY, graphW, graphH, 0.20f, beatmap.strainGraph, gradeClr, -1.0f);

                // Dibujar marcas de fallos e imperfecciones sobre la línea temporal
                for (const auto& ev : judgementHistory) {
                    if (ev.state == HIT_300) continue;
                    float rx = graphX + (std::max(0.0f, std::min(totalSpan, static_cast<float>(ev.time - firstT))) / totalSpan) * (graphW - 2.0f);
                    if (ev.state == HIT_MISS) {
                        float my = graphY + graphH - 4.0f;
                        C2D_DrawLine(rx, graphY + 1.0f, Ui::rgba(240, 70, 70, 50), rx, my, Ui::rgba(240, 70, 70, 100), 1.0f, 0.23f);
                        C2D_DrawLine(rx - 2.5f, my - 2.5f, clrMiss, rx + 2.5f, my + 2.5f, clrMiss, 1.8f, 0.26f);
                        C2D_DrawLine(rx - 2.5f, my + 2.5f, clrMiss, rx + 2.5f, my - 2.5f, clrMiss, 1.8f, 0.26f);
                    } else if (ev.state == HIT_50) {
                        C2D_DrawCircleSolid(rx, graphY + graphH - 6.0f, 0.25f, 1.8f, clr50);
                    } else if (ev.state == HIT_100) {
                        C2D_DrawCircleSolid(rx, graphY + graphH - 10.0f, 0.24f, 1.4f, clr100);
                    }
                }

                // Marcas de tiempo al pie del gráfico (0:00 y duración final)
                drawText(textBuf, graphX + 2.0f, graphY + graphH - 8.0f, 0.22f, Ui::rgba(180, 175, 205), "0:00", 0.27f);
                char durStr[16];
                int totalSec = std::max(0, lastT / 1000);
                std::snprintf(durStr, sizeof(durStr), "%d:%02d", totalSec / 60, totalSec % 60);
                drawText(textBuf, graphX + graphW - 26.0f, graphY + graphH - 8.0f, 0.22f, Ui::rgba(180, 175, 205), durStr, 0.27f);
            } else {
                // VISTA 2: RANKING LOCAL (Top 5 en tarjeta SD)
                Ui::panel(8.0f, 70.0f, 304.0f, 96.0f, 0.18f, Ui::rgba(18, 14, 28, 250), clrPrimary);
                auto topScores = scoreMgr.getTopRecords(beatmap.filePath, 5, currentPlayMode);

                if (topScores.empty()) {
                    drawCenteredText(textBuf, 160.0f, 118.0f, 0.36f, clrMuted, LOC(STR_RESULTS_NO_SCORES_SD), 0.25f);
                } else {
                    for (size_t s = 0; s < topScores.size() && s < 5; ++s) {
                        const auto& r = topScores[s];
                        float ry = 73.0f + s * 18.2f;
                        bool isCur = (r.bestScore == score && r.bestCombo == maxCombo && std::abs(r.bestAcc - acc) < 0.05f);
                        u32 rowBg = isCur ? Ui::rgba(65, 45, 95, 230) : ((s % 2 == 0) ? Ui::rgba(24, 20, 36, 200) : Ui::rgba(18, 15, 28, 200));
                        u32 rkClr = (s == 0) ? Ui::rgba(255, 215, 60) : ((s == 1) ? Ui::rgba(220, 230, 245) : ((s == 2) ? Ui::rgba(215, 145, 80) : clrMuted));

                        Ui::pill(12.0f, ry, 296.0f, 16.5f, 0.20f, rowBg);
                        if (isCur) {
                            drawRectOutline(12.0f, ry, 296.0f, 16.5f, 1.0f, Ui::rgba(255, 215, 60, 220), 0.22f);
                        }

                        char rkStr[8];
                        std::snprintf(rkStr, sizeof(rkStr), "#%d", static_cast<int>(s + 1));
                        drawText(textBuf, 16.0f, ry + 2.0f, 0.28f, rkClr, rkStr, 0.25f);

                        char scStr[32];
                        std::snprintf(scStr, sizeof(scStr), "%07d", r.bestScore);
                        drawText(textBuf, 44.0f, ry + 2.0f, 0.30f, clrWhite, scStr, 0.25f);

                        char cmbStr[24];
                        std::snprintf(cmbStr, sizeof(cmbStr), "%dx", r.bestCombo);
                        drawText(textBuf, 114.0f, ry + 2.0f, 0.27f, clrCursor, cmbStr, 0.25f);

                        char accStr[24];
                        std::snprintf(accStr, sizeof(accStr), "%.1f%%", r.bestAcc);
                        drawText(textBuf, 166.0f, ry + 2.0f, 0.27f, clr300, accStr, 0.25f);

                        if (r.bestMods != MOD_NONE) {
                            std::string mStr = formatModsString(r.bestMods);
                            drawText(textBuf, 218.0f, ry + 2.0f, 0.24f, clrMuted, mStr.c_str(), 0.25f);
                        }

                        Ui::pill(282.0f, ry + 1.5f, 20.0f, 13.5f, 0.22f, Ui::rgba(35, 28, 52));
                        drawCenteredText(textBuf, 292.0f, ry + 8.0f, 0.28f, clrWhite, r.bestGrade.c_str(), 0.26f);
                    }
                }
            }

            // 3. Tarjeta inferior de Logro / Estado
            if (newRecordAchieved) {
                float flash = 0.70f + 0.30f * std::sin(animTick * 0.18f);
                Ui::gradH(8.0f, 168.0f, 304.0f, 32.0f, 0.20f,
                          Ui::rgba(90, 32, 110, static_cast<u8>(240.0f * flash)),
                          Ui::rgba(30, 18, 55, 245));
                drawRectOutline(8.0f, 168.0f, 304.0f, 32.0f, 1.2f, Ui::withAlpha(clrCursor, static_cast<u8>(255.0f * flash)), 0.25f);
                Ui::pill(14.0f, 172.0f, 126.0f, 15.0f, 0.26f, Ui::rgba(255, 102, 171, 230));
                drawCenteredText(textBuf, 77.0f, 179.5f, 0.30f, clrWhite, LOC(STR_RESULTS_NEW_RECORD), 0.28f);

                char recSub[64];
                std::snprintf(recSub, sizeof(recSub), "%s: %d  •  UR: %.1f", LOC(STR_MAX_COMBO), maxCombo, lastUnstableRate);
                drawTextWithShadow(textBuf, 148.0f, 173.0f, 0.30f, clr100, recSub, 0.28f);
            } else {
                Ui::panel(8.0f, 168.0f, 304.0f, 32.0f, 0.20f, Ui::rgba(22, 18, 34, 240), gradeClr);
                char statSummary[96];
                std::snprintf(statSummary, sizeof(statSummary), "%s: %d / %d   •   UR: %.1f",
                              LOC(STR_MAX_COMBO), maxCombo, beatmap.maxPossibleCombo, lastUnstableRate);
                drawTextWithShadow(textBuf, 16.0f, 172.0f, 0.32f, clrWhite, statSummary, 0.25f);
                drawTextWithShadow(textBuf, 16.0f, 186.0f, 0.26f, clrMuted,
                         stageFailed ? LOC(STR_RESULTS_FAILED_DESC) : LOC(STR_RESULTS_CLEARED_DESC), 0.25f);
            }

            // 4. Tres botones de acción interactivos estilo multijugador (táctiles y con atajo físico)
            // Botón 1: Vista [X]
            Ui::button(8.0f, 204.0f, 96.0f, 30.0f, 0.22f, Ui::rgba(20, 36, 60, 240), Ui::rgba(40, 180, 240), false);
            Ui::pill(14.0f, 210.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(40, 180, 240));
            drawCenteredText(textBuf, 23.0f, 219.0f, 0.28f, Ui::rgba(0, 0, 0, 240), "X", 0.28f);
            drawTextWithShadow(textBuf, 36.0f, 211.0f, 0.32f, clrWhite, (resultsBottomTab == 0) ? LOC(STR_RESULTS_VIEW_TOP) : LOC(STR_RESULTS_STRAIN_CURVE), 0.28f);

            // Botón 2: Reintentar [A] (resaltado con brillo verde como en multijugador)
            Ui::button(111.0f, 204.0f, 98.0f, 30.0f, 0.22f, Ui::rgba(22, 52, 32, 245), Ui::rgba(30, 200, 85), true);
            Ui::pill(117.0f, 210.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(30, 200, 85));
            drawCenteredText(textBuf, 126.0f, 219.0f, 0.28f, Ui::rgba(0, 0, 0, 240), "A", 0.28f);
            drawTextWithShadow(textBuf, 140.0f, 211.0f, 0.32f, clrWhite, LOC(STR_RETRY), 0.28f);

            // Botón 3: Salir a Canciones [B]
            Ui::button(216.0f, 204.0f, 96.0f, 30.0f, 0.22f, Ui::rgba(42, 20, 52, 240), clrPrimary, false);
            Ui::pill(222.0f, 210.0f, 18.0f, 18.0f, 0.24f, clrPrimary);
            drawCenteredText(textBuf, 231.0f, 219.0f, 0.28f, clrWhite, "B", 0.28f);
            drawTextWithShadow(textBuf, 244.0f, 211.0f, 0.32f, clrWhite, LOC(STR_SONGS), 0.28f);

            Ui::drawTransition(320.0f, 240.0f, screenFade, gradeClr);
            C3D_FrameEnd(0);
            continue;
        }

        auto findLocalMapFile = [&](const std::string& folder, const std::string& file) -> std::string {
            std::string res = resolveFilePath(folder, file);
            if (!res.empty()) return res;

            std::string bFile = extractBaseFilename(file);
            std::string bFolder = extractFolderName(folder);

            // 1. Probar rutas alternativas estándar directas y en subcarpetas
            const char* testDirs[] = {
                "sdmc:/3ds/osu-3ds/songs/",
                "sdmc:/osu-3ds/songs/",
                "sdmc:/3ds/osu/Songs/",
                "sdmc:/3ds/osu/songs/",
                "romfs:/songs/",
                "romfs:/"
            };
            for (const char* dir : testDirs) {
                std::string p1 = std::string(dir) + bFolder + "/" + bFile;
                FILE* f1 = std::fopen(p1.c_str(), "rb");
                if (f1) { std::fclose(f1); return p1; }

                std::string p2 = std::string(dir) + bFile;
                FILE* f2 = std::fopen(p2.c_str(), "rb");
                if (f2) { std::fclose(f2); return p2; }
            }

            // 1b. Si en la subcarpeta existe algún archivo .osu, intentar abrirlo
            if (!bFolder.empty()) {
                for (const char* dir : testDirs) {
                    std::string fullDir = std::string(dir) + bFolder;
                    DIR* d = opendir(fullDir.c_str());
                    if (d) {
                        struct dirent* ent;
                        while ((ent = readdir(d)) != nullptr) {
                            std::string fn = ent->d_name;
                            if (fn.size() >= 5 && fn.substr(fn.size() - 4) == ".osu") {
                                if (fn == bFile) {
                                    std::string cand = fullDir + "/" + fn;
                                    closedir(d);
                                    return cand;
                                }
                            }
                        }
                        closedir(d);
                    }
                }
            }

            // 2. Buscar en la biblioteca de SongManager por nombre de archivo exacto
            if (!bFile.empty()) {
                for (const auto& s : songMgr.sets) {
                    for (const auto& d : s.difficulties) {
                        if (extractBaseFilename(d.osuPath) == bFile) {
                            return d.osuPath;
                        }
                    }
                }
            }

            // 3. Buscar en la biblioteca por Título y Dificultad/Versión exacta
            if (!multiMgr.activeBeatmap.title.empty() && !multiMgr.activeBeatmap.version.empty()) {
                for (const auto& s : songMgr.sets) {
                    if (s.title == multiMgr.activeBeatmap.title) {
                        for (const auto& d : s.difficulties) {
                            if (d.version == multiMgr.activeBeatmap.version) {
                                return d.osuPath;
                            }
                        }
                    }
                }
            }

            return "";
        };

        // =========================================================
        // 4. BROWSER DE SALAS MULTIJUGADOR LOCAL (LISTA LAN)
        // =========================================================
        if (appState == STATE_MULTI_BROWSER) {
            audio.update();
            if (settings.songPreview && !audio.isPreviewLoading() && audio.isSongFinished()) {
                rotateMenuSong();
            }
            multiMgr.updateBrowser();

            // 1. Controles Físicos
            if (kDown & KEY_B) {
                audio.playHitSound(0);
                audio.setLoop(false);
                multiMgr.stop();
                appState = STATE_MAIN_MENU;
                continue;
            }
            if (kDown & KEY_SELECT) {
                audio.playHitSound(1);
                multiMgr.toggleNetworkMode();
            }
            if (kDown & KEY_Y) {
                audio.playHitSound(1);
                multiMgr.refreshBrowser();
            }
            if (kDown & KEY_X) {
                audio.playHitSound(2);
                QueuedBeatmap initMap;
                if (!songMgr.sets.empty()) {
                    const auto& s = songMgr.sets[selectedSet % songMgr.sets.size()];
                    if (!s.difficulties.empty()) {
                        const auto& d = s.difficulties[selectedDiff % s.difficulties.size()];
                        initMap = QueuedBeatmap(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, 0);
                    }
                }
                multiMgr.startHost(sysUser, initMap);
                appState = STATE_MULTI_LOBBY;
                continue;
            }
            if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
                if (multiMgr.selectedRoomIndex > 0) {
                    multiMgr.selectedRoomIndex--;
                    audio.playHitSound(0);
                }
            }
            if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
                if (multiMgr.selectedRoomIndex + 1 < static_cast<int>(multiMgr.discoveredRooms.size())) {
                    multiMgr.selectedRoomIndex++;
                    audio.playHitSound(0);
                }
            }
            if (kDown & KEY_A) {
                if (!multiMgr.discoveredRooms.empty()) {
                    if (multiMgr.joinSelectedRoom()) {
                        audio.playHitSound(2);
                        appState = STATE_MULTI_LOBBY;
                        continue;
                    }
                }
            }

            // 2. Entradas Táctiles
            if (touchJustDown) {
                // Botón Modo de Red (UDS vs LAN) en la barra superior táctil
                if (touch.px >= 135.0f && touch.px <= 220.0f && touch.py >= 4.0f && touch.py <= 28.0f) {
                    audio.playHitSound(1);
                    multiMgr.toggleNetworkMode();
                }

                // Botón Refrescar en la esquina superior derecha
                if (touch.px >= 225.0f && touch.px <= 312.0f && touch.py >= 4.0f && touch.py <= 28.0f) {
                    audio.playHitSound(1);
                    multiMgr.refreshBrowser();
                }

                // Tocar tarjetas de salas en la lista
                int startIdx = 0;
                if (multiMgr.selectedRoomIndex >= 3) startIdx = multiMgr.selectedRoomIndex - 2;
                for (int k = 0; k < 3 && (startIdx + k) < static_cast<int>(multiMgr.discoveredRooms.size()); ++k) {
                    float cy = 34.0f + k * 48.0f;
                    if (touch.px >= 10.0f && touch.px <= 310.0f && touch.py >= cy && touch.py <= cy + 44.0f) {
                        int idx = startIdx + k;
                        if (multiMgr.selectedRoomIndex == idx) {
                            if (multiMgr.joinSelectedRoom()) {
                                audio.playHitSound(2);
                                appState = STATE_MULTI_LOBBY;
                                break;
                            }
                        } else {
                            multiMgr.selectedRoomIndex = idx;
                            audio.playHitSound(1);
                        }
                    }
                }
                if (appState == STATE_MULTI_LOBBY) continue;

                // Botón Volver [B] (inferior izquierdo)
                if (touch.px >= 10.0f && touch.px <= 74.0f && touch.py >= 186.0f && touch.py <= 234.0f) {
                    audio.playHitSound(0);
                    multiMgr.stop();
                    appState = STATE_MAIN_MENU;
                    continue;
                }

                // Botón + CREAR SALA NUEVA [X] (inferior derecho)
                if (touch.px >= 82.0f && touch.px <= 310.0f && touch.py >= 186.0f && touch.py <= 234.0f) {
                    audio.playHitSound(2);
                    QueuedBeatmap initMap;
                    if (!songMgr.sets.empty()) {
                        const auto& s = songMgr.sets[selectedSet % songMgr.sets.size()];
                        if (!s.difficulties.empty()) {
                            const auto& d = s.difficulties[selectedDiff % s.difficulties.size()];
                            initMap = QueuedBeatmap(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, 0);
                        }
                    }
                    multiMgr.startHost(sysUser, initMap);
                    appState = STATE_MULTI_LOBBY;
                    continue;
                }
            }

            // 3. Renderizado Gráfico (Top & Bottom Screens)
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);

                // Barra Superior
                float dBar = depthShift(1.8f);
                Ui::pill(10.0f + dBar, 8.0f, 62.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
                drawCenteredText(textBuf, 41.0f + dBar, 16.0f, 0.36f, clrWhite, "osu!3DS", 0.30f);

                const char* topTitle = (multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS)
                    ? LOC(STR_MULTI_ROOMS_UDS)
                    : LOC(STR_MULTI_ROOMS_LAN);
                drawText(textBuf, 78.0f + dBar, 10.0f, 0.44f, clrWhite, topTitle, 0.30f);

                char roomBadge[32];
                std::snprintf(roomBadge, sizeof(roomBadge), LOC(STR_MULTI_ROOMS_COUNT), static_cast<int>(multiMgr.discoveredRooms.size()));
                Ui::pill(280.0f + dBar, 8.0f, 58.0f, 18.0f, 0.25f, Ui::rgba(40, 215, 235));
                drawCenteredText(textBuf, 309.0f + dBar, 16.0f, 0.30f, Ui::rgba(0, 0, 0, 240), roomBadge, 0.30f);

                Ui::pill(344.0f + dBar, 8.0f, 48.0f, 18.0f, 0.25f, Ui::rgba(30, 25, 45, 220));
                renderWifiBars(358.0f + dBar, 10.0f, 3, Ui::rgba(40, 215, 235), Ui::rgba(80, 80, 100));

                // Si no hay salas descubiertas
                if (multiMgr.discoveredRooms.empty()) {
                    float radAnim = 16.0f + 12.0f * (0.5f + 0.5f * std::sin(animTick * 0.08f));
                    float cx = 200.0f + depthShift(1.5f);
                    float cy = 88.0f;
                    Ui::glow(cx, cy, 0.15f, radAnim * 2.2f, Ui::rgba(40, 215, 235, 60), 4);
                    Ui::panel(cx - 24.0f, cy - 24.0f, 48.0f, 48.0f, 0.20f, Ui::rgba(25, 22, 42, 235), Ui::rgba(40, 215, 235));
                    renderWifiBars(cx - 9.0f, cy - 6.0f, 3, Ui::rgba(40, 215, 235), Ui::rgba(70, 70, 95));

                    if (multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS) {
                        drawCenteredText(textBuf, cx, 126.0f, 0.42f, clrWhite, LOC(STR_MULTI_SEARCH_UDS), 0.25f);
                        drawCenteredText(textBuf, cx, 146.0f, 0.32f, clrMuted, LOC(STR_MULTI_NO_NET_REQ), 0.25f);
                        drawCenteredText(textBuf, cx, 166.0f, 0.33f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_PRESS_X_CREATE), 0.25f);
                        drawCenteredText(textBuf, cx, 182.0f, 0.29f, clrMuted, LOC(STR_MULTI_ANY_JOIN), 0.25f);
                    } else {
                        drawCenteredText(textBuf, cx, 126.0f, 0.42f, clrWhite, LOC(STR_MULTI_SEARCH_LAN), 0.25f);
                        drawCenteredText(textBuf, cx, 146.0f, 0.32f, clrMuted, LOC(STR_MULTI_NO_ACTIVE_ROOMS), 0.25f);
                        drawCenteredText(textBuf, cx, 166.0f, 0.33f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_PRESS_X_CREATE), 0.25f);
                        drawCenteredText(textBuf, cx, 182.0f, 0.29f, clrMuted, LOC(STR_MULTI_ROOM_WILL_APPEAR), 0.25f);
                    }
                } else {
                    // Columna Izquierda: Tarjetas de Salas (hasta 3 visibles)
                    int startIdx = 0;
                    if (multiMgr.selectedRoomIndex >= 3) startIdx = multiMgr.selectedRoomIndex - 2;
                    for (int r = 0; r < 3 && (startIdx + r) < static_cast<int>(multiMgr.discoveredRooms.size()); ++r) {
                        int idx = startIdx + r;
                        const auto& rm = multiMgr.discoveredRooms[idx];
                        bool isSel = (idx == multiMgr.selectedRoomIndex);
                        float ry = 32.0f + r * 56.0f;
                        float rx = 10.0f + depthShift(isSel ? 3.0f : 1.5f);
                        float rw = 224.0f;
                        float rh = 52.0f;

                        u32 borderClr = isSel ? Ui::rgba(255, 102, 170) : Ui::rgba(50, 44, 72);
                        u32 bgClr = isSel ? Ui::rgba(36, 26, 54, 245) : Ui::rgba(22, 19, 36, 230);
                        Ui::panel(rx, ry, rw, rh, 0.20f, bgClr, borderClr);

                        if (isSel) {
                            Ui::pill(rx + 4.0f, ry + 6.0f, 4.0f, rh - 12.0f, 0.25f, Ui::rgba(255, 102, 170));
                        }

                        drawText(textBuf, rx + 14.0f, ry + 6.0f, 0.38f, clrWhite, truncateString(rm.roomTitle, 22).c_str(), 0.25f);

                        char subLine[64];
                        std::snprintf(subLine, sizeof(subLine), "Host: %s . %s", rm.hostName.c_str(), truncateString(rm.mapTitle, 16).c_str());
                        drawText(textBuf, rx + 14.0f, ry + 22.0f, 0.30f, clrMuted, subLine, 0.25f);

                        char starsBuf[16];
                        std::snprintf(starsBuf, sizeof(starsBuf), "%.2f*", rm.mapStars);
                        u32 sClr = getStarRatingColor(rm.mapStars);
                        Ui::pill(rx + 14.0f, ry + 36.0f, 40.0f, 11.0f, 0.26f, sClr);
                        drawCenteredText(textBuf, rx + 34.0f, ry + 40.0f, 0.24f, Ui::rgba(0, 0, 0, 240), starsBuf, 0.28f);

                        char plyBuf[16];
                        std::snprintf(plyBuf, sizeof(plyBuf), "%d/%d", rm.playerCount, rm.maxPlayers);
                        u32 pClr = (rm.playerCount >= rm.maxPlayers) ? Ui::rgba(215, 50, 60) : Ui::rgba(30, 195, 80);
                        Ui::pill(rx + rw - 44.0f, ry + 10.0f, 36.0f, 15.0f, 0.26f, pClr);
                        drawCenteredText(textBuf, rx + rw - 26.0f, ry + 16.5f, 0.28f, clrWhite, plyBuf, 0.28f);
                    }

                    // Columna Derecha: Panel de Detalles de la Sala Seleccionada
                    float px = 240.0f + depthShift(2.0f);
                    float py = 32.0f;
                    float pw = 150.0f;
                    float ph = 168.0f;
                    Ui::panel(px, py, pw, ph, 0.20f, Ui::rgba(22, 19, 36, 245), Ui::rgba(40, 215, 235));

                    const auto& selRm = multiMgr.discoveredRooms[multiMgr.selectedRoomIndex];
                    drawText(textBuf, px + 8.0f, py + 8.0f, 0.36f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_DETAILS), 0.25f);
                    drawText(textBuf, px + 8.0f, py + 26.0f, 0.30f, clrWhite, ("Host: " + truncateString(selRm.hostName, 12)).c_str(), 0.25f);

                    const char* wcStr = (selRm.winCondition == WIN_SCORE) ? LOC(STR_SCORE) :
                                        ((selRm.winCondition == WIN_ACCURACY) ? LOC(STR_ACCURACY) : LOC(STR_MAX_COMBO));
                    drawText(textBuf, px + 8.0f, py + 42.0f, 0.28f, clrMuted, (std::string(LOC(STR_MULTI_RULE)) + ": " + std::string(wcStr)).c_str(), 0.25f);

                    const char* mmStr = (selRm.modMode == MODS_HOST_ONLY) ? "Host Only" : "FreeMod";
                    drawText(textBuf, px + 8.0f, py + 56.0f, 0.28f, clrMuted, (std::string(LOC(STR_MULTI_MODS_LABEL)) + ": " + std::string(mmStr)).c_str(), 0.25f);

                    drawText(textBuf, px + 8.0f, py + 74.0f, 0.30f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_MAP_LABEL), 0.25f);
                    drawText(textBuf, px + 8.0f, py + 88.0f, 0.29f, clrWhite, truncateString(selRm.mapTitle, 15).c_str(), 0.25f);
                    drawText(textBuf, px + 8.0f, py + 102.0f, 0.26f, clrMuted, truncateString(selRm.mapArtist, 17).c_str(), 0.25f);
                    drawText(textBuf, px + 8.0f, py + 116.0f, 0.26f, clrMuted, truncateString(selRm.mapVersion, 17).c_str(), 0.25f);

                    char pingStr[24];
                    std::snprintf(pingStr, sizeof(pingStr), (selRm.mode == MultiNetworkMode::NET_MODE_UDS) ? "Ping UDS: %d ms" : "Ping LAN: %d ms", selRm.pingMs);
                    drawText(textBuf, px + 8.0f, py + 138.0f, 0.27f, Ui::rgba(70, 220, 120), pingStr, 0.25f);

                    const char* joinHint = (selRm.playerCount >= selRm.maxPlayers) ? LOC(STR_MULTI_ROOM_FULL) : LOC(STR_MULTI_JOIN);
                    u32 hintClr = (selRm.playerCount >= selRm.maxPlayers) ? Ui::rgba(215, 50, 60) : Ui::rgba(40, 215, 235);
                    drawCenteredText(textBuf, px + pw * 0.5f, py + 154.0f, 0.30f, hintClr, joinHint, 0.25f);
                }

                // Barra Inferior de Atajos o Toast
                if (multiMgr.hasToastNotice()) {
                    float dToast = depthShift(4.0f);
                    Ui::glow(200.0f + dToast, 218.0f, 0.88f, 160.0f, Ui::rgba(255, 60, 80, 110), 3);
                    Ui::pill(30.0f + dToast, 206.0f, 340.0f, 24.0f, 0.90f, Ui::rgba(35, 12, 22, 245));
                    drawRectOutline(30.0f + dToast, 206.0f, 340.0f, 24.0f, 1.2f, Ui::rgba(255, 80, 100), 0.92f);
                    drawCenteredTextWithShadow(textBuf, 200.0f + dToast, 217.0f, 0.35f, Ui::rgba(255, 230, 230), multiMgr.toastNotice.c_str(), 0.95f);
                } else {
                    float byY = 219.0f;
                    drawKeyBadge(textBuf, 12.0f + dBar, byY, "A", LOC(STR_MULTI_JOIN), Ui::rgba(50, 165, 255));
                    drawKeyBadge(textBuf, 88.0f + dBar, byY, "X", LOC(STR_MULTI_CREATE_ROOM), Ui::rgba(50, 215, 115));
                    drawKeyBadge(textBuf, 156.0f + dBar, byY, "Y", LOC(STR_SEARCH), Ui::rgba(255, 180, 40));
                    drawKeyBadge(textBuf, 228.0f + dBar, byY, "B", LOC(STR_BACK), Ui::rgba(255, 75, 80));
                }
            }

            // Renderizado Pantalla Inferior (Táctil)
            C2D_TargetClear(bottomScreen, clrBgDark);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);

            const char* bottomTitle = (multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS)
                ? LOC(STR_MULTI_ROOMS_UDS)
                : LOC(STR_MULTI_ROOMS_LAN);
            drawText(textBuf, 10.0f, 8.0f, 0.36f, clrWhite, bottomTitle, 0.25f);

            // Botón Alternar Modo (UDS / LAN)
            bool isModeUds = (multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS);
            Ui::panel(148.0f, 4.0f, 74.0f, 22.0f, 0.20f, Ui::rgba(28, 24, 44, 240), isModeUds ? Ui::rgba(40, 215, 235) : Ui::rgba(255, 150, 40));
            drawCenteredText(textBuf, 185.0f, 13.0f, 0.28f, isModeUds ? Ui::rgba(40, 215, 235) : Ui::rgba(255, 150, 40), isModeUds ? (std::string(LOC(STR_MODE)) + ": UDS").c_str() : (std::string(LOC(STR_MODE)) + ": LAN").c_str(), 0.25f);

            // Botón Refrescar
            Ui::panel(226.0f, 4.0f, 84.0f, 22.0f, 0.20f, Ui::rgba(30, 26, 46, 240), Ui::rgba(40, 215, 235));
            drawCenteredText(textBuf, 268.0f, 13.0f, 0.29f, Ui::rgba(40, 215, 235), LOC(STR_SEARCH), 0.25f);

            // Contenedor de la lista de salas
            if (multiMgr.discoveredRooms.empty()) {
                Ui::panel(10.0f, 36.0f, 300.0f, 138.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(45, 40, 65));
                drawCenteredText(textBuf, 160.0f, 75.0f, 0.38f, clrWhite, LOC(STR_MULTI_NO_ROOMS_FOUND), 0.25f);
                drawCenteredText(textBuf, 160.0f, 98.0f, 0.31f, clrMuted, LOC(STR_MULTI_CREATE_ROOM_TOUCH), 0.25f);
                drawCenteredText(textBuf, 160.0f, 116.0f, 0.31f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_ANY_3DS_JOIN), 0.25f);
            } else {
                int startIdx = 0;
                if (multiMgr.selectedRoomIndex >= 3) startIdx = multiMgr.selectedRoomIndex - 2;
                for (int k = 0; k < 3 && (startIdx + k) < static_cast<int>(multiMgr.discoveredRooms.size()); ++k) {
                    int idx = startIdx + k;
                    const auto& rm = multiMgr.discoveredRooms[idx];
                    bool isSel = (idx == multiMgr.selectedRoomIndex);
                    float cy = 34.0f + k * 48.0f;
                    float cx = 10.0f;
                    float cw = 300.0f;
                    float ch = 44.0f;

                    u32 bClr = isSel ? Ui::rgba(255, 102, 170) : Ui::rgba(50, 44, 72);
                    u32 bg = isSel ? Ui::rgba(38, 28, 56, 245) : Ui::rgba(22, 19, 36, 230);
                    Ui::panel(cx, cy, cw, ch, 0.20f, bg, bClr);

                    if (isSel) {
                        Ui::pill(cx + 4.0f, cy + 5.0f, 4.0f, ch - 10.0f, 0.25f, Ui::rgba(255, 102, 170));
                    }

                    drawText(textBuf, cx + 14.0f, cy + 5.0f, 0.38f, clrWhite, truncateString(rm.roomTitle, 24).c_str(), 0.25f);

                    char subTxt[64];
                    std::snprintf(subTxt, sizeof(subTxt), "%s Host: %s . %s", (rm.mode == MultiNetworkMode::NET_MODE_UDS) ? "[UDS]" : "[LAN]", rm.hostName.c_str(), truncateString(rm.mapTitle, 16).c_str());
                    drawText(textBuf, cx + 14.0f, cy + 22.0f, 0.30f, clrMuted, subTxt, 0.25f);

                    char pTxt[16];
                    std::snprintf(pTxt, sizeof(pTxt), "%d/%d", rm.playerCount, rm.maxPlayers);
                    Ui::pill(cx + cw - 52.0f, cy + 12.0f, 42.0f, 20.0f, 0.26f, isSel ? Ui::rgba(255, 102, 170) : Ui::rgba(40, 215, 235));
                    drawCenteredText(textBuf, cx + cw - 31.0f, cy + 20.0f, 0.30f, isSel ? clrWhite : Ui::rgba(0, 0, 0, 240), isSel ? LOC(STR_MULTI_JOIN) : pTxt, 0.28f);
                }
            }

            // Barra de Acción Inferior: VOLVER y + CREAR SALA NUEVA estilo multijugador
            Ui::button(10.0f, 186.0f, 68.0f, 44.0f, 0.20f, Ui::rgba(32, 26, 44, 240), Ui::rgba(65, 55, 85), false);
            Ui::pill(16.0f, 192.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(65, 55, 85));
            drawCenteredText(textBuf, 25.0f, 201.0f, 0.28f, clrWhite, "B", 0.28f);
            drawTextWithShadow(textBuf, 38.0f, 193.0f, 0.32f, clrMuted, LOC(STR_BACK), 0.28f);
            drawTextWithShadow(textBuf, 38.0f, 207.0f, 0.26f, clrMuted, LOC(STR_MENU_SHORT), 0.28f);

            Ui::button(84.0f, 186.0f, 226.0f, 44.0f, 0.20f, Ui::rgba(25, 140, 65, 250), Ui::rgba(140, 255, 180), true);
            Ui::pill(92.0f, 196.0f, 24.0f, 22.0f, 0.25f, Ui::rgba(140, 255, 180));
            drawCenteredText(textBuf, 104.0f, 207.0f, 0.34f, Ui::rgba(10, 45, 20, 245), "X", 0.30f);
            drawTextWithShadow(textBuf, 124.0f, 197.0f, 0.44f, clrWhite, LOC(STR_MULTI_CREATE_ROOM), 0.30f);

            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 4A. SALA MULTIJUGADOR LOCAL (LOBBY HASTA 4 JUGADORES)
        // =========================================================
        if (appState == STATE_MULTI_LOBBY) {
            audio.update();
            if (settings.songPreview && !audio.isPreviewLoading() && audio.isSongFinished()) {
                rotateMenuSong();
            }
            multiMgr.update(false);

            if (!multiMgr.inMultiplayer) {
                multiMgr.startBrowser(sysUser);
                appState = STATE_MULTI_BROWSER;
                continue;
            }

            // Verificación y estado del beatmap para jugadores locales (con caché para evitar lecturas de SD cada frame)
            static std::string s_lastCheckedSongKey = "";
            static bool s_cachedHasLocalMap = true;

            // Si se acaba de descargar un mapa por UDS o TCP, indexarlo de inmediato en SongManager
            if (!multiMgr.lastDownloadedOsuPath.empty()) {
                songMgr.addSingleBeatmap(multiMgr.lastDownloadedOsuPath, false, &scoreMgr);
                multiMgr.lastDownloadedOsuPath.clear();
                s_lastCheckedSongKey = "";
            }

            std::string curSongKey = multiMgr.activeBeatmap.folderPath + "::" + multiMgr.activeBeatmap.osuFilename + "::" + multiMgr.activeBeatmap.version;
            if (curSongKey != s_lastCheckedSongKey || 
                ((multiMgr.players[multiMgr.localSlot].status == STATUS_NOT_READY || multiMgr.players[multiMgr.localSlot].status == STATUS_MISSING_MAP) && !s_cachedHasLocalMap)) {
                s_lastCheckedSongKey = curSongKey;
                if (!multiMgr.activeBeatmap.osuFilename.empty()) {
                    std::string existingPath = findLocalMapFile(multiMgr.activeBeatmap.folderPath, multiMgr.activeBeatmap.osuFilename);
                    s_cachedHasLocalMap = !existingPath.empty();
                } else {
                    s_cachedHasLocalMap = true;
                }
            }
            bool hasLocalMap = s_cachedHasLocalMap;

            if (!multiMgr.activeBeatmap.osuFilename.empty()) {
                if (!hasLocalMap) {
                    if (multiMgr.players[multiMgr.localSlot].status != STATUS_DOWNLOADING &&
                        multiMgr.players[multiMgr.localSlot].status != STATUS_MISSING_MAP) {
                        multiMgr.players[multiMgr.localSlot].status = STATUS_MISSING_MAP;
                        if (multiMgr.isHost()) {
                            multiMgr.broadcastLobbyState();
                        } else {
                            multiMgr.sendPlayerUpdate();
                        }
                    }
                } else {
                    if (multiMgr.players[multiMgr.localSlot].status == STATUS_MISSING_MAP) {
                        multiMgr.players[multiMgr.localSlot].status = multiMgr.isHost() ? STATUS_READY : STATUS_NOT_READY;
                        if (multiMgr.isHost()) {
                            multiMgr.broadcastLobbyState();
                        } else {
                            multiMgr.sendPlayerUpdate();
                        }
                    }
                }
            }

            // Inicio de partida sincronizado: transición al estado de sincronización y carga
            if (multiMgr.matchStarted) {
                multiSyncLoaded = false;
                multiMgr.matchGo = false;
                appState = STATE_MULTI_SYNC;
                continue;
            }

            // Controles físicos
            if (kDown & KEY_B) {
                if (multiQueuePickerOpen) {
                    multiQueuePickerOpen = false;
                    audio.playHitSound(0);
                } else {
                    audio.playHitSound(0);
                    if (multiMgr.isHost()) {
                        multiMgr.notifyRoomClosed();
                    } else {
                        multiMgr.notifyPlayerLeave();
                    }
                    multiMgr.stop();
                    multiMgr.startBrowser(sysUser);
                    appState = STATE_MULTI_BROWSER;
                    continue;
                }
            }
            if (kDown & KEY_A) {
                audio.playHitSound(2);
                if (multiQueuePickerOpen) {
                    if (!songMgr.sets.empty()) {
                        const auto& s = songMgr.sets[multiQueuePickerIndex % songMgr.sets.size()];
                        if (!s.difficulties.empty()) {
                            int diffIdx = multiQueuePickerDiff % s.difficulties.size();
                            const auto& d = s.difficulties[diffIdx];
                            QueuedBeatmap qm(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, multiMgr.localSlot);
                            if (multiMgr.isHost()) {
                                if (multiPickerAction == PICKER_ACTION_QUEUE) {
                                    multiMgr.enqueueBeatmap(qm);
                                } else {
                                    multiMgr.setActiveBeatmap(qm);
                                }
                            } else {
                                multiMgr.requestQueueMap(qm);
                            }
                        }
                    }
                    multiQueuePickerOpen = false;
                } else {
                    if (!hasLocalMap || multiMgr.players[multiMgr.localSlot].status == STATUS_MISSING_MAP) {
                        if (multiMgr.players[multiMgr.localSlot].status != STATUS_DOWNLOADING) {
                            multiMgr.startMapDownload(multiMgr.connectedHostIp);
                        }
                    } else if (multiMgr.isHost()) {
                        if (multiMgr.areAllPlayersReady()) {
                            multiMgr.sendStartMatch();
                        }
                    } else {
                        multiMgr.toggleLocalReady();
                    }
                }
            }
            if (kDown & KEY_Y) {
                audio.playHitSound(0);
                appState = STATE_MODS_SELECT;
                continue;
            }
            if (kDown & KEY_X) {
                audio.playHitSound(0);
                multiPickerAction = PICKER_ACTION_QUEUE;
                appState = STATE_SONG_SELECT;
                continue;
            }
            // Roles fijos: KEY_SELECT no alterna roles para garantizar estabilidad absoluta de conexión
            if (multiQueuePickerOpen) {
                if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
                    if (multiQueuePickerIndex > 0) {
                        multiQueuePickerIndex--;
                        multiQueuePickerDiff = 0;
                    }
                    audio.playHitSound(0);
                }
                if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
                    if (multiQueuePickerIndex + 1 < static_cast<int>(songMgr.sets.size())) {
                        multiQueuePickerIndex++;
                        multiQueuePickerDiff = 0;
                    }
                    audio.playHitSound(0);
                }
                if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
                    if (!songMgr.sets.empty()) {
                        const auto& s = songMgr.sets[multiQueuePickerIndex % songMgr.sets.size()];
                        if (multiQueuePickerDiff > 0) {
                            multiQueuePickerDiff--;
                            audio.playHitSound(0);
                        } else if (!s.difficulties.empty()) {
                            multiQueuePickerDiff = static_cast<int>(s.difficulties.size()) - 1;
                            audio.playHitSound(0);
                        }
                    }
                }
                if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                    if (!songMgr.sets.empty()) {
                        const auto& s = songMgr.sets[multiQueuePickerIndex % songMgr.sets.size()];
                        if (multiQueuePickerDiff + 1 < static_cast<int>(s.difficulties.size())) {
                            multiQueuePickerDiff++;
                            audio.playHitSound(0);
                        } else {
                            multiQueuePickerDiff = 0;
                            audio.playHitSound(0);
                        }
                    }
                }
            }

            // Entradas táctiles
            if (touchJustDown) {
                if (multiQueuePickerOpen) {
                    bool touchedSong = false;
                    for (int k = 0; k < 4; ++k) {
                        float ry = 38.0f + k * 40.0f;
                        if (touch.py >= ry && touch.py <= ry + 36.0f) {
                            int targetIdx = multiQueuePickerIndex - (multiQueuePickerIndex % 4) + k;
                            if (targetIdx < static_cast<int>(songMgr.sets.size())) {
                                multiQueuePickerIndex = targetIdx;
                                const auto& s = songMgr.sets[targetIdx];
                                if (!s.difficulties.empty()) {
                                    int diffIdx = (targetIdx == multiQueuePickerIndex) ? (multiQueuePickerDiff % s.difficulties.size()) : 0;
                                    const auto& d = s.difficulties[diffIdx];
                                    QueuedBeatmap qm(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, multiMgr.localSlot);
                                    if (multiMgr.isHost()) {
                                        multiMgr.hostChangingSong = false;
                                        if (multiPickerAction == PICKER_ACTION_QUEUE) {
                                            multiMgr.enqueueBeatmap(qm);
                                        } else {
                                            multiMgr.setActiveBeatmap(qm);
                                        }
                                    } else {
                                        multiMgr.requestQueueMap(qm);
                                    }
                                    audio.playHitSound(2);
                                }
                            }
                            multiQueuePickerOpen = false;
                            touchedSong = true;
                            break;
                        }
                    }
                    if (!touchedSong && touch.py >= 196.0f && touch.py <= 234.0f) {
                        if (!songMgr.sets.empty()) {
                            const auto& s = songMgr.sets[multiQueuePickerIndex % songMgr.sets.size()];
                            if (!s.difficulties.empty()) {
                                int diffIdx = multiQueuePickerDiff % s.difficulties.size();
                                const auto& d = s.difficulties[diffIdx];
                                QueuedBeatmap qm(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, multiMgr.localSlot);

                                if (multiMgr.isHost()) {
                                    multiMgr.hostChangingSong = false;

                                    if (touch.px >= 14.0f && touch.px <= 154.0f) {
                                        // Tocar botón [A] CAMBIAR MAPA
                                        multiMgr.setActiveBeatmap(qm);
                                        audio.playHitSound(2);
                                        multiQueuePickerOpen = false;
                                    } else if (touch.px >= 164.0f && touch.px <= 306.0f) {
                                        // Tocar botón [X] + A LA COLA
                                        multiMgr.enqueueBeatmap(qm);
                                        audio.playHitSound(2);
                                        multiQueuePickerOpen = false;
                                    }
                                } else {
                                    // Cliente: sugerir a la cola
                                    multiMgr.requestQueueMap(qm);
                                    audio.playHitSound(2);
                                    multiQueuePickerOpen = false;
                                }
                            }
                        }
                    }
                } else {
                    // Roles estrictamente fijos: no se permite alternar modo Host/Cliente tocando la pantalla

                    // Pestañas de Criterio de Victoria (WinCondition)
                    if (touch.py >= 46.0f && touch.py <= 72.0f) {
                        if (touch.px >= 14.0f && touch.px <= 104.0f) {
                            if (multiMgr.isHost()) multiMgr.setWinConditionExplicit(WIN_SCORE);
                            audio.playHitSound(0);
                        } else if (touch.px >= 112.0f && touch.px <= 202.0f) {
                            if (multiMgr.isHost()) multiMgr.setWinConditionExplicit(WIN_ACCURACY);
                            audio.playHitSound(0);
                        } else if (touch.px >= 210.0f && touch.px <= 304.0f) {
                            if (multiMgr.isHost()) multiMgr.setWinConditionExplicit(WIN_COMBO);
                            audio.playHitSound(0);
                        }
                    }

                    if (multiMgr.isHost()) {
                        // Tarjeta 1: Modificadores (x: 10..104, y: 82..150)
                        if (touch.px >= 10.0f && touch.px <= 104.0f && touch.py >= 82.0f && touch.py <= 150.0f) {
                            audio.playHitSound(0);
                            appState = STATE_MODS_SELECT;
                            continue;
                        }
                        // Tarjeta 2: Cambiar Canción Actual (x: 113..207, y: 82..150)
                        if (touch.px >= 113.0f && touch.px <= 207.0f && touch.py >= 82.0f && touch.py <= 150.0f) {
                            if (multiMgr.isAnyClientDownloading()) {
                                audio.playHitSound(4); // Bloqueado: jugador descargando
                                continue;
                            }
                            audio.playHitSound(0);
                            multiPickerAction = PICKER_ACTION_ACTIVE;
                            multiMgr.hostChangingSong = true;
                            multiMgr.broadcastLobbyState();
                            appState = STATE_SONG_SELECT;
                            continue;
                        }
                        // Tarjeta 3: + Añadir a la Cola (x: 216..310, y: 82..150)
                        if (touch.px >= 216.0f && touch.px <= 310.0f && touch.py >= 82.0f && touch.py <= 150.0f) {
                            audio.playHitSound(0);
                            multiPickerAction = PICKER_ACTION_QUEUE;
                            appState = STATE_SONG_SELECT;
                            continue;
                        }
                    } else {
                        // Cliente: 2 tarjetas
                        // Tarjeta 1: Modificadores (x: 12..154, y: 82..150)
                        if (touch.px >= 12.0f && touch.px <= 154.0f && touch.py >= 82.0f && touch.py <= 150.0f) {
                            audio.playHitSound(0);
                            appState = STATE_MODS_SELECT;
                            continue;
                        }
                        // Tarjeta 2: + Añadir Canción (Cola) (x: 166..308, y: 82..150)
                        if (touch.px >= 166.0f && touch.px <= 308.0f && touch.py >= 82.0f && touch.py <= 150.0f) {
                            audio.playHitSound(0);
                            multiPickerAction = PICKER_ACTION_QUEUE;
                            appState = STATE_SONG_SELECT;
                            continue;
                        }
                    }

                    // Botón Inferior Grande: LISTO / INICIAR PARTIDA / DESCARGAR MAPA
                    if (touch.px >= 12.0f && touch.px <= 308.0f && touch.py >= 176.0f && touch.py <= 230.0f) {
                        audio.playHitSound(2);
                        if (!hasLocalMap || multiMgr.players[multiMgr.localSlot].status == STATUS_MISSING_MAP) {
                            if (multiMgr.players[multiMgr.localSlot].status != STATUS_DOWNLOADING) {
                                multiMgr.startMapDownload(multiMgr.connectedHostIp);
                            }
                        } else if (multiMgr.isHost()) {
                            if (multiMgr.areAllPlayersReady()) {
                                multiMgr.sendStartMatch();
                            }
                        } else {
                            multiMgr.toggleLocalReady();
                        }
                    }
                }
            }

            // RENDERIZADO DEL LOBBY (PANTALLA SUPERIOR E INFERIOR)
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                // float dBg = depthShift(-3.0f);
                Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);

                // 1. Barra Superior del Lobby
                float dBar = depthShift(1.8f);
                Ui::pill(10.0f + dBar, 9.0f, 62.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
                drawCenteredText(textBuf, 41.0f + dBar, 17.0f, 0.36f, clrWhite, "osu!3DS", 0.30f);

                const char* lobbyTitle = (multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS)
                    ? LOC(STR_MULTI_ROOMS_UDS)
                    : LOC(STR_MULTI_ROOMS_LAN);
                drawText(textBuf, 78.0f + dBar, 11.0f, 0.44f, clrWhite, lobbyTitle, 0.30f);

                char slotBadge[16];
                std::snprintf(slotBadge, sizeof(slotBadge), "[%d Slots]", multiMgr.getActivePlayerCount());
                Ui::pill(240.0f + dBar, 9.0f, 52.0f, 18.0f, 0.25f, Ui::rgba(20, 160, 210, 220));
                drawCenteredText(textBuf, 266.0f + dBar, 17.0f, 0.32f, clrWhite, slotBadge, 0.30f);

                // Regla de sala en la esquina superior derecha
                const char* winCritName = (multiMgr.winCondition == WIN_SCORE) ? LOC(STR_SCORE) :
                                        ((multiMgr.winCondition == WIN_ACCURACY) ? LOC(STR_ACCURACY) : LOC(STR_MAX_COMBO));
                char ruleLabel[64];
                std::snprintf(ruleLabel, sizeof(ruleLabel), "%s: %s", LOC(STR_MULTI_RULE), winCritName);
                Ui::pill(296.0f + dBar, 9.0f, 96.0f, 18.0f, 0.25f, Ui::rgba(45, 38, 20, 220));
                drawRectOutline(296.0f + dBar, 9.0f, 96.0f, 18.0f, 1.2f, Ui::rgba(255, 195, 45), 0.26f);
                drawCenteredText(textBuf, 344.0f + dBar, 17.0f, 0.30f, Ui::rgba(255, 205, 50), ruleLabel, 0.30f);

                // 2. Columna Izquierda: 4 Tarjetas de Jugadores (Slots 0..3)
                float dCards = depthShift(2.5f);
                for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
                    float cy = 34.0f + i * 44.0f;
                    float cx = 10.0f + dCards;
                    float cw = 196.0f;
                    float ch = 40.0f;

                    const auto& p = multiMgr.players[i];
                    if (p.active) {
                        u32 borderClr = p.isHost ? Ui::rgba(255, 195, 45) :
                                        (p.isLocal ? Ui::rgba(40, 215, 235) :
                                        ((i == 2) ? Ui::rgba(180, 80, 240) : Ui::rgba(60, 200, 120)));

                        Ui::panel(cx, cy, cw, ch, 0.20f, Ui::rgba(22, 19, 36, 240), borderClr);

                        // Nombre de jugador
                        drawText(textBuf, cx + 8.0f, cy + 5.0f, 0.44f, clrWhite, p.name, 0.25f);

                        // Píldora de rol: HOST o TÚ
                        if (p.isHost) {
                            Ui::pill(cx + 96.0f, cy + 6.0f, 36.0f, 13.0f, 0.26f, Ui::rgba(255, 130, 20));
                            drawCenteredText(textBuf, cx + 114.0f, cy + 12.0f, 0.28f, clrWhite, "HOST", 0.30f);
                        } else if (p.isLocal) {
                            Ui::pill(cx + 96.0f, cy + 6.0f, 26.0f, 13.0f, 0.26f, Ui::rgba(40, 215, 235));
                            drawCenteredText(textBuf, cx + 109.0f, cy + 12.0f, 0.28f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_YOU), 0.30f);
                        }

                        // Subtítulo: Mods equipados y Ping
                        char subStr[48];
                        std::string modStr = (p.activeMods != MOD_NONE) ? formatModsString(p.activeMods) : "None";
                        std::snprintf(subStr, sizeof(subStr), "+%s . %ums", modStr.c_str(), p.pingMs);
                        drawText(textBuf, cx + 8.0f, cy + 22.0f, 0.32f, clrMuted, subStr, 0.25f);

                        // Píldora de estado en la derecha de la tarjeta
                        if (p.status == STATUS_READY) {
                            Ui::pill(cx + cw - 52.0f, cy + 12.0f, 44.0f, 17.0f, 0.26f, Ui::rgba(30, 165, 80));
                            drawCenteredText(textBuf, cx + cw - 30.0f, cy + 20.0f, 0.32f, clrWhite, LOC(STR_READY), 0.30f);
                        } else if (p.status == STATUS_DOWNLOADING) {
                            char dlPill[16];
                            std::snprintf(dlPill, sizeof(dlPill), "%d%%", p.downloadProgress);
                            Ui::pill(cx + cw - 62.0f, cy + 12.0f, 54.0f, 17.0f, 0.26f, Ui::rgba(220, 120, 20));
                            drawCenteredText(textBuf, cx + cw - 35.0f, cy + 20.0f, 0.30f, clrWhite, dlPill, 0.30f);
                        } else if (p.status == STATUS_MISSING_MAP) {
                            Ui::pill(cx + cw - 62.0f, cy + 12.0f, 54.0f, 17.0f, 0.26f, Ui::rgba(215, 50, 60));
                            drawCenteredText(textBuf, cx + cw - 35.0f, cy + 20.0f, 0.30f, clrWhite, "NO MAP", 0.30f);
                        } else {
                            Ui::pill(cx + cw - 62.0f, cy + 12.0f, 54.0f, 17.0f, 0.26f, Ui::rgba(45, 40, 60));
                            drawCenteredText(textBuf, cx + cw - 35.0f, cy + 20.0f, 0.30f, clrMuted, LOC(STR_MULTI_WAITING), 0.30f);
                        }
                    } else {
                        Ui::panel(cx, cy, cw, ch, 0.18f, Ui::rgba(16, 14, 26, 170));
                        drawRectOutline(cx, cy, cw, ch, 1.0f, Ui::rgba(40, 36, 56), 0.20f);
                        drawCenteredText(textBuf, cx + cw * 0.5f, cy + 19.0f, 0.34f, clrMuted, LOC(STR_MULTI_SLOT_OPEN), 0.25f);
                    }
                }

                // 3. Columna Derecha: Mapa Actual y Fila de Canciones (Map Queue)
                float dRight = depthShift(2.0f);
                float rx = 214.0f + dRight;
                float rw = 176.0f;

                // Tarjeta 1: SIGUIENTE MAPA
                Ui::panel(rx, 34.0f, rw, 80.0f, 0.20f, Ui::rgba(20, 18, 34, 240), multiMgr.hostChangingSong ? Ui::rgba(255, 185, 45) : Ui::rgba(40, 215, 235));
                if (multiMgr.hostChangingSong) {
                    Ui::glow(rx + rw * 0.5f, 42.0f, 0.22f, 70.0f, Ui::rgba(255, 185, 45, 90), 2);
                    drawText(textBuf, rx + 8.0f, 38.0f, 0.32f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_CHANGING_MAP), 0.25f);
                    drawCenteredTextWithShadow(textBuf, rx + rw * 0.5f, 70.0f, 0.38f, Ui::rgba(255, 215, 60), LOC(STR_MULTI_HOST_CHOOSING), 0.25f);
                } else {
                    drawText(textBuf, rx + 8.0f, 38.0f, 0.32f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_NEXT_MAP), 0.25f);

                    char lenStr[16];
                    std::snprintf(lenStr, sizeof(lenStr), "%d:%02d", multiMgr.activeBeatmap.lengthSec / 60, multiMgr.activeBeatmap.lengthSec % 60);
                    drawText(textBuf, rx + rw - 36.0f, 38.0f, 0.32f, clrMuted, lenStr, 0.25f);

                    std::string dispTitle = multiMgr.activeBeatmap.title;
                    drawText(textBuf, rx + 8.0f, 52.0f, 0.44f, clrWhite, truncateString(dispTitle, 16).c_str(), 0.25f);

                    std::string dispArtist = multiMgr.activeBeatmap.artist;
                    drawText(textBuf, rx + 8.0f, 68.0f, 0.34f, clrMuted, truncateString(dispArtist, 20).c_str(), 0.25f);

                    char starsStr[24];
                    std::snprintf(starsStr, sizeof(starsStr), "* %.2f", multiMgr.activeBeatmap.starRating);
                    drawText(textBuf, rx + 8.0f, 86.0f, 0.36f, Ui::rgba(255, 205, 50), starsStr, 0.25f);

                    char bpmStr[16];
                    std::snprintf(bpmStr, sizeof(bpmStr), "%.0f BPM", multiMgr.activeBeatmap.bpm);
                    drawText(textBuf, rx + rw - 54.0f, 86.0f, 0.34f, clrMuted, bpmStr, 0.25f);
                }

                // Tarjeta 2: EN COLA (Map Queue)
                Ui::panel(rx, 120.0f, rw, 90.0f, 0.20f, Ui::rgba(18, 16, 30, 240), Ui::rgba(85, 75, 110));
                char qHeader[24];
                std::snprintf(qHeader, sizeof(qHeader), "%s (%d)", LOC(STR_MULTI_ENQUEUE), static_cast<int>(multiMgr.mapQueue.size()));
                drawText(textBuf, rx + 8.0f, 124.0f, 0.32f, Ui::rgba(160, 150, 190), qHeader, 0.25f);

                if (multiMgr.mapQueue.empty()) {
                    drawCenteredTextWithShadow(textBuf, rx + rw * 0.5f, 150.0f, 0.36f, Ui::rgba(220, 215, 240), LOC(STR_MULTI_QUEUE_EMPTY), 0.25f);
                    Ui::pill(rx + 14.0f, 168.0f, rw - 28.0f, 22.0f, 0.24f, Ui::rgba(20, 50, 75, 240));
                    drawRectOutline(rx + 14.0f, 168.0f, rw - 28.0f, 22.0f, 1.2f, Ui::rgba(40, 215, 235), 0.26f);
                    Ui::pill(rx + 20.0f, 172.0f, 14.0f, 14.0f, 0.27f, Ui::rgba(40, 215, 235));
                    drawCenteredText(textBuf, rx + 27.0f, 179.0f, 0.28f, Ui::rgba(0, 0, 0, 245), "X", 0.30f);
                    drawTextWithShadow(textBuf, rx + 38.0f, 172.0f, 0.32f, Ui::rgba(180, 240, 255), LOC(STR_MULTI_TAP_TO_ADD), 0.30f);
                } else {
                    for (size_t q = 0; q < multiMgr.mapQueue.size() && q < 3; ++q) {
                        float qy = 142.0f + q * 22.0f;
                        Ui::panel(rx + 6.0f, qy, rw - 12.0f, 18.0f, 0.22f, Ui::rgba(26, 22, 42, 220));
                        char qItemStr[48];
                        std::snprintf(qItemStr, sizeof(qItemStr), "#%u %s", static_cast<unsigned int>(q + 2), multiMgr.mapQueue[q].title.c_str());
                        drawTextWithShadow(textBuf, rx + 10.0f, qy + 2.0f, 0.32f, clrWhite, truncateString(qItemStr, 22).c_str(), 0.26f);
                    }
                }

                // Barra inferior de atajos de control o Toast
                Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
                if (multiMgr.hasToastNotice()) {
                    float dToast = depthShift(4.0f);
                    Ui::glow(200.0f + dToast, 224.0f, 0.88f, 160.0f, Ui::rgba(255, 60, 80, 110), 3);
                    Ui::pill(30.0f + dToast, 212.0f, 340.0f, 24.0f, 0.90f, Ui::rgba(35, 12, 22, 245));
                    drawRectOutline(30.0f + dToast, 212.0f, 340.0f, 24.0f, 1.2f, Ui::rgba(255, 80, 100), 0.92f);
                    drawCenteredTextWithShadow(textBuf, 200.0f + dToast, 224.0f, 0.34f, Ui::rgba(255, 230, 230), multiMgr.toastNotice.c_str(), 0.95f);
                } else {
                    float byY = 219.0f;
                    drawKeyBadge(textBuf, 10.0f + dBar, byY, "A", multiMgr.isHost() ? LOC(STR_MULTI_START_MATCH) : LOC(STR_READY), Ui::rgba(255, 75, 80));
                    drawKeyBadge(textBuf, 88.0f + dBar, byY, "Y", "Mods", Ui::rgba(50, 215, 115));
                    drawKeyBadge(textBuf, 158.0f + dBar, byY, "X", LOC(STR_MULTI_ENQUEUE), Ui::rgba(50, 165, 255));
                    drawKeyBadge(textBuf, 226.0f + dBar, byY, "B", LOC(STR_BACK), Ui::rgba(255, 195, 45));

                    Ui::pill(296.0f + dBar, byY - 1.0f, 32.0f, 15.0f, 0.30f, Ui::rgba(24, 60, 85));
                    drawCenteredText(textBuf, 312.0f + dBar, byY + 6.5f, 0.28f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_ROLE), 0.32f);
                    drawTextWithShadow(textBuf, 334.0f + dBar, byY, 0.34f, clrWhite, multiMgr.isHost() ? "Host" : LOC(STR_MULTI_CLIENT), 0.32f);
                }

                Ui::drawTransition(400.0f, 240.0f, screenFade, clrPrimary);
            }

            // PANTALLA INFERIOR (PANEL TÁCTIL DEL LOBBY)
            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.02f);

            if (multiQueuePickerOpen) {
                Ui::panel(10.0f, 8.0f, 300.0f, 224.0f, 0.15f, Ui::rgba(16, 14, 28, 250), clrPrimary);
                const char* pickerTitle = multiMgr.isHost()
                    ? ((multiPickerAction == PICKER_ACTION_QUEUE) ? LOC(STR_MULTI_ADD_QUEUE) : LOC(STR_MULTI_CHANGE_MAP))
                    : LOC(STR_MULTI_SUGGEST_MAP);
                drawCenteredText(textBuf, 160.0f, 18.0f, 0.40f, clrWhite, pickerTitle, 0.20f);

                for (int k = 0; k < 4; ++k) {
                    int idx = multiQueuePickerIndex - (multiQueuePickerIndex % 4) + k;
                    float qy = 38.0f + k * 40.0f;
                    if (idx < static_cast<int>(songMgr.sets.size())) {
                        const auto& s = songMgr.sets[idx];
                        bool isSel = (idx == multiQueuePickerIndex);
                        u32 bgCard = isSel ? Ui::rgba(45, 38, 70, 255) : Ui::rgba(24, 20, 38, 230);
                        Ui::panel(18.0f, qy, 284.0f, 36.0f, 0.20f, bgCard, isSel ? (multiMgr.isHost() ? Ui::rgba(255, 140, 20) : clrPrimary) : 0);
                        drawText(textBuf, 24.0f, qy + 3.0f, 0.38f, clrWhite, truncateString(s.title, 26).c_str(), 0.25f);

                        if (!s.difficulties.empty()) {
                            int diffIdx = isSel ? (multiQueuePickerDiff % s.difficulties.size()) : 0;
                            const auto& d = s.difficulties[diffIdx];
                            char diffInfo[64];
                            if (isSel && s.difficulties.size() > 1) {
                                std::snprintf(diffInfo, sizeof(diffInfo), "< %s > (* %.2f)", d.version.c_str(), d.starRating);
                            } else {
                                std::snprintf(diffInfo, sizeof(diffInfo), "[%s] * %.2f", d.version.c_str(), d.starRating);
                            }
                            drawText(textBuf, 24.0f, qy + 19.0f, 0.30f, isSel ? Ui::rgba(255, 205, 50) : clrMuted, truncateString(diffInfo, 30).c_str(), 0.25f);
                        }
                    }
                }

                if (multiMgr.isHost()) {
                    Ui::button(14.0f, 198.0f, 140.0f, 28.0f, 0.20f, Ui::rgba(255, 140, 20), Ui::rgba(255, 200, 100), true);
                    drawCenteredTextWithShadow(textBuf, 84.0f, 212.0f, 0.34f, clrWhite, LOC(STR_MULTI_CHANGE_MAP), 0.25f);

                    Ui::button(166.0f, 198.0f, 140.0f, 28.0f, 0.20f, Ui::rgba(40, 180, 220), Ui::rgba(160, 230, 255), false);
                    drawCenteredTextWithShadow(textBuf, 236.0f, 212.0f, 0.34f, clrWhite, LOC(STR_MULTI_ADD_QUEUE), 0.25f);
                } else {
                    Ui::button(20.0f, 198.0f, 280.0f, 28.0f, 0.20f, Ui::rgba(180, 80, 240), Ui::rgba(220, 160, 255), false);
                    drawCenteredTextWithShadow(textBuf, 160.0f, 212.0f, 0.34f, clrWhite, LOC(STR_MULTI_SUGGEST_MAP), 0.25f);
                }
            } else {
                drawTextWithShadow(textBuf, 12.0f, 10.0f, 0.42f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_TOUCH_PANEL), 0.20f);
                if (multiMgr.isHost()) {
                    Ui::pill(248.0f, 6.0f, 60.0f, 17.0f, 0.25f, Ui::rgba(255, 140, 20));
                    drawCenteredText(textBuf, 278.0f, 14.5f, 0.30f, clrWhite, "HOST", 0.30f);
                } else {
                    Ui::pill(242.0f, 6.0f, 66.0f, 17.0f, 0.25f, Ui::rgba(40, 215, 235));
                    drawCenteredText(textBuf, 275.0f, 14.5f, 0.30f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_CLIENT), 0.30f);
                }

                drawTextWithShadow(textBuf, 12.0f, 28.0f, 0.33f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_WIN_CRITERIA), 0.20f);
                const char* critText = (multiMgr.winCondition == WIN_SCORE) ? LOC(STR_SCORE) :
                                       ((multiMgr.winCondition == WIN_ACCURACY) ? LOC(STR_ACCURACY) : LOC(STR_MAX_COMBO));
                drawTextWithShadow(textBuf, 214.0f, 28.0f, 0.35f, Ui::rgba(40, 215, 235), critText, 0.20f);

                // 3 Pestañas interactivas de regla de victoria
                const char* tabNames[3] = { LOC(STR_MULTI_WIN_SCORE), LOC(STR_MULTI_WIN_ACC), LOC(STR_MULTI_WIN_COMBO) };
                for (int t = 0; t < 3; ++t) {
                    float tx = 14.0f + t * 98.0f;
                    float ty = 46.0f;
                    float tw = 92.0f;
                    float th = 26.0f;
                    bool activeTab = (static_cast<int>(multiMgr.winCondition) == t);

                    u32 tabBg = activeTab ? Ui::rgba(40, 215, 235) : Ui::rgba(26, 22, 40, 240);
                    u32 tabTxt = activeTab ? Ui::rgba(10, 8, 18, 255) : Ui::rgba(200, 195, 225);
                    Ui::panel(tx, ty, tw, th, 0.20f, tabBg, activeTab ? Ui::rgba(255, 255, 255) : 0);
                    if (activeTab) {
                        Ui::glow(tx + tw * 0.5f, ty + th * 0.5f, 0.18f, tw * 0.45f, Ui::rgba(40, 215, 235, 80), 2);
                    }
                    drawCenteredTextWithShadow(textBuf, tx + tw * 0.5f, ty + th * 0.5f, 0.34f, tabTxt, tabNames[t], 0.25f);
                }

                if (multiMgr.isHost()) {
                    // Tarjeta 1: Modificadores (x: 10..104, y: 82..148)
                    Ui::panel(10.0f, 82.0f, 94.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235));
                    drawTextWithShadow(textBuf, 14.0f, 88.0f, 0.34f, clrWhite, "MODS", 0.25f);
                    std::string myMods = (multiMgr.players[multiMgr.localSlot].activeMods != MOD_NONE)
                        ? ("+" + formatModsString(multiMgr.players[multiMgr.localSlot].activeMods)) : "None";
                    Ui::pill(14.0f, 108.0f, 86.0f, 16.0f, 0.26f, Ui::rgba(40, 215, 235));
                    drawCenteredText(textBuf, 57.0f, 116.0f, 0.28f, Ui::rgba(0, 0, 0, 240), truncateString(myMods, 10).c_str(), 0.30f);
                    drawCenteredTextWithShadow(textBuf, 57.0f, 134.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_MODS_MENU), 0.25f);

                    // Tarjeta 2: Cambiar Canción Actual (x: 113..207, y: 82..148)
                    bool clientDownloading = multiMgr.isAnyClientDownloading();
                    u32 c2Border = clientDownloading ? Ui::rgba(140, 50, 60) : Ui::rgba(255, 140, 20);
                    Ui::panel(113.0f, 82.0f, 94.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), c2Border);
                    drawTextWithShadow(textBuf, 117.0f, 88.0f, 0.34f, clientDownloading ? clrMiss : clrWhite, LOC(STR_MULTI_CHANGE_MAP), 0.25f);
                    Ui::pill(117.0f, 108.0f, 86.0f, 16.0f, 0.26f, c2Border);
                    drawCenteredText(textBuf, 160.0f, 116.0f, 0.28f, clrWhite, clientDownloading ? LOC(STR_MULTI_BLOCKED) : LOC(STR_MULTI_ACTIVE_MAP), 0.30f);
                    drawCenteredTextWithShadow(textBuf, 160.0f, 134.0f, 0.30f, clientDownloading ? clrMiss : Ui::rgba(215, 210, 235), clientDownloading ? LOC(STR_MULTI_DL_ACTIVE) : LOC(STR_MULTI_TAP_CHANGE), 0.25f);

                    // Tarjeta 3: + Añadir a la Cola (x: 216..310, y: 82..148)
                    Ui::panel(216.0f, 82.0f, 94.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(180, 80, 240));
                    drawTextWithShadow(textBuf, 220.0f, 88.0f, 0.34f, clrWhite, LOC(STR_MULTI_ADD_QUEUE), 0.25f);
                    Ui::pill(220.0f, 108.0f, 86.0f, 16.0f, 0.26f, Ui::rgba(180, 80, 240));
                    drawCenteredText(textBuf, 263.0f, 116.0f, 0.28f, clrWhite, LOC(STR_MULTI_ENQUEUE), 0.30f);
                    drawCenteredTextWithShadow(textBuf, 263.0f, 134.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_TAP_QUEUE), 0.25f);
                } else {
                    // Tarjeta 1: Modificadores (Cliente)
                    Ui::panel(12.0f, 82.0f, 142.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235));
                    drawText(textBuf, 20.0f, 88.0f, 0.36f, clrWhite, "MODS", 0.25f);
                    std::string myMods = (multiMgr.players[multiMgr.localSlot].activeMods != MOD_NONE)
                        ? ("+" + formatModsString(multiMgr.players[multiMgr.localSlot].activeMods)) : "None";
                    Ui::pill(92.0f, 86.0f, 56.0f, 15.0f, 0.26f, Ui::rgba(40, 215, 235));
                    drawCenteredText(textBuf, 120.0f, 93.0f, 0.26f, Ui::rgba(0, 0, 0, 240), truncateString(myMods, 7).c_str(), 0.30f);
                    drawTextWithShadow(textBuf, 20.0f, 114.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_TAP_OPEN), 0.25f);
                    drawTextWithShadow(textBuf, 20.0f, 128.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_MODS_MENU), 0.25f);

                    // Tarjeta 2: + Añadir Canción (Cola Cliente)
                    Ui::panel(166.0f, 82.0f, 142.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(180, 80, 240));
                    drawTextWithShadow(textBuf, 174.0f, 88.0f, 0.36f, clrWhite, LOC(STR_MULTI_ADD_QUEUE), 0.25f);
                    Ui::pill(268.0f, 86.0f, 34.0f, 15.0f, 0.26f, Ui::rgba(180, 80, 240));
                    drawCenteredText(textBuf, 285.0f, 93.0f, 0.26f, clrWhite, LOC(STR_MULTI_ENQUEUE), 0.30f);
                    drawTextWithShadow(textBuf, 174.0f, 114.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_SUGGEST_MAP), 0.25f);
                    drawTextWithShadow(textBuf, 174.0f, 128.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_FOR_ROOM), 0.25f);
                }

                char statusSummary[64];
                std::snprintf(statusSummary, sizeof(statusSummary), "%s: %d/%d", LOC(STR_READY), multiMgr.getReadyPlayerCount(), multiMgr.getActivePlayerCount());
                drawTextWithShadow(textBuf, 14.0f, 158.0f, 0.34f, clrWhite, statusSummary, 0.25f);

                const char* subStat = (multiMgr.hostChangingSong) ? LOC(STR_MULTI_CHANGING_MAP) :
                                      (!hasLocalMap || multiMgr.players[multiMgr.localSlot].status == STATUS_MISSING_MAP) ? LOC(STR_MULTI_DOWNLOAD_MAP) :
                                      (multiMgr.players[multiMgr.localSlot].status == STATUS_DOWNLOADING) ? LOC(STR_MULTI_DL_ACTIVE) :
                                      (multiMgr.areAllPlayersReady()) ? LOC(STR_MULTI_ALL_READY) :
                                      (multiMgr.isHost() ? LOC(STR_MULTI_WAITING_PLAYERS) : LOC(STR_MULTI_WAITING_HOST));
                drawTextWithShadow(textBuf, 168.0f, 158.0f, 0.30f, Ui::rgba(40, 215, 235), subStat, 0.25f);

                float btnX = 12.0f;
                float btnY = 176.0f;
                float btnW = 296.0f;
                float btnH = 50.0f;

                if (!hasLocalMap || multiMgr.players[multiMgr.localSlot].status == STATUS_MISSING_MAP) {
                    Ui::glow(btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.16f, btnW * 0.52f, Ui::rgba(20, 160, 230, 90), 3);
                    Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(20, 135, 215, 255), Ui::rgba(160, 230, 255));
                    drawCenteredText(textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.46f, clrWhite, LOC(STR_MULTI_DL_MAP_TAP), 0.30f);
                } else if (multiMgr.players[multiMgr.localSlot].status == STATUS_DOWNLOADING) {
                    Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(220, 110, 20, 255), Ui::rgba(255, 180, 60));
                    float prog = multiMgr.players[multiMgr.localSlot].downloadProgress / 100.0f;
                    if (prog > 0.02f) {
                        C2D_DrawRectSolid(btnX + 4.0f, btnY + 4.0f, 0.22f, (btnW - 8.0f) * std::min(1.0f, prog), btnH - 8.0f, Ui::rgba(255, 155, 30, 200));
                    }
                    char dlTxt[48];
                    std::snprintf(dlTxt, sizeof(dlTxt), "%s (%d%%)...", LOC(STR_MULTI_DL_ACTIVE), multiMgr.players[multiMgr.localSlot].downloadProgress);
                    drawCenteredText(textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.44f, clrWhite, dlTxt, 0.30f);
                } else if (multiMgr.hostChangingSong && !multiMgr.isHost()) {
                    Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(45, 38, 55, 240), Ui::rgba(180, 130, 40));
                    drawCenteredText(textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.42f, Ui::rgba(255, 205, 50), LOC(STR_MULTI_CHANGING_MAP), 0.30f);
                } else if (multiMgr.isHost()) {
                    if (multiMgr.areAllPlayersReady()) {
                        Ui::glow(btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.18f, btnW * 0.55f, Ui::rgba(30, 215, 90, 80), 4);
                        Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(30, 195, 80, 255), Ui::rgba(180, 255, 200));
                        drawCenteredText(textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.52f, clrWhite, LOC(STR_MULTI_START_MATCH), 0.30f);
                    } else {
                        Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(45, 40, 60, 240), Ui::rgba(85, 75, 110));
                        drawCenteredText(textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.44f, clrMuted, LOC(STR_MULTI_WAITING_PLAYERS), 0.30f);
                    }
                } else if (multiMgr.players[multiMgr.localSlot].status == STATUS_READY) {
                    Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(28, 75, 50, 240), Ui::rgba(40, 215, 120));
                    drawCenteredText(textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.46f, Ui::rgba(180, 255, 210), LOC(STR_MULTI_UNREADY), 0.30f);
                } else {
                    Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(30, 160, 75, 240), Ui::rgba(100, 255, 160));
                    drawCenteredText(textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.46f, clrWhite, LOC(STR_MULTI_READY_TAP), 0.30f);
                }
            }

            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 4B. SELECCIÓN DE MODIFICADORES (MODS HD, HR, EZ, NF)
        // =========================================================
        if (appState == STATE_MODS_SELECT) {
            audio.update();
            if (settings.songPreview && !audio.isPreviewLoading() && audio.isSongFinished()) {
                rotateMenuSong();
            }
            multiMgr.update(false);

            if (kDown & (KEY_B | KEY_START)) {
                audio.playHitSound(0);
                appState = STATE_MULTI_LOBBY;
                continue;
            }

            float currentMult = getModsScoreMultiplier(settings.activeMods);

            if (touchJustDown) {
                // Fila 1: Reducción de Dificultad (NF, EZ)
                if (touch.py >= 54.0f && touch.py <= 114.0f) {
                    if (touch.px >= 14.0f && touch.px <= 152.0f) {
                        settings.activeMods ^= MOD_NOFAIL;
                        audio.playHitSound(2);
                    } else if (touch.px >= 168.0f && touch.px <= 306.0f) {
                        settings.activeMods ^= MOD_EASY;
                        if (settings.activeMods & MOD_EASY) settings.activeMods &= ~MOD_HARDROCK;
                        audio.playHitSound(2);
                    }
                }
                // Fila 2: Aumento de Dificultad (HD, HR)
                if (touch.py >= 132.0f && touch.py <= 192.0f) {
                    if (touch.px >= 14.0f && touch.px <= 152.0f) {
                        settings.activeMods ^= MOD_HIDDEN;
                        audio.playHitSound(2);
                    } else if (touch.px >= 168.0f && touch.px <= 306.0f) {
                        settings.activeMods ^= MOD_HARDROCK;
                        if (settings.activeMods & MOD_HARDROCK) settings.activeMods &= ~MOD_EASY;
                        audio.playHitSound(2);
                    }
                }

                if (touch.py >= 198.0f && touch.py <= 234.0f) {
                    audio.playHitSound(0);
                    appState = STATE_MULTI_LOBBY;
                    continue;
                }

                multiMgr.players[multiMgr.localSlot].activeMods = settings.activeMods;
                multiMgr.sendPlayerUpdate();
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                float dTop = depthShift(2.0f);
                Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);

                Ui::pill(12.0f + dTop, 10.0f, 54.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
                drawCenteredText(textBuf, 39.0f + dTop, 18.0f, 0.36f, clrWhite, "MODS", 0.30f);

                drawText(textBuf, 72.0f + dTop, 11.0f, 0.44f, clrWhite, LOC(STR_MODS_IMPACT), 0.30f);

                char multStr[32];
                std::snprintf(multStr, sizeof(multStr), LOC(STR_MODS_MULTIPLIER), currentMult);
                drawText(textBuf, 230.0f + dTop, 12.0f, 0.36f, Ui::rgba(40, 215, 235), multStr, 0.30f);

                Ui::panel(12.0f + dTop, 36.0f, 376.0f, 150.0f, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(40, 215, 235));

                std::string bTitle = multiMgr.activeBeatmap.title + " [" + multiMgr.activeBeatmap.version + "]";
                drawText(textBuf, 22.0f + dTop, 44.0f, 0.46f, clrWhite, truncateString(bTitle, 36).c_str(), 0.25f);
                drawText(textBuf, 22.0f + dTop, 62.0f, 0.32f, clrMuted, LOC(STR_MODS_INFO), 0.25f);

                float curCS = beatmap.diff.cs;
                float curAR = beatmap.diff.ar;
                float curOD = beatmap.diff.od;
                float curHP = beatmap.diff.hp;

                if (settings.activeMods & MOD_EASY) {
                    curCS *= 0.5f; curAR *= 0.5f; curOD *= 0.5f; curHP *= 0.5f;
                }
                if (settings.activeMods & MOD_HARDROCK) {
                    curCS = std::min(10.0f, curCS * 1.3f);
                    curAR = std::min(10.0f, curAR * 1.4f);
                    curOD = std::min(10.0f, curOD * 1.4f);
                    curHP = std::min(10.0f, curHP * 1.4f);
                }

                drawStatBar(textBuf, 22.0f + dTop, 80.0f, "Circle Size (CS)", curCS, Ui::rgba(40, 215, 235), clrBgDark, clrWhite);
                drawStatBar(textBuf, 206.0f + dTop, 80.0f, "Approach Rate (AR)", curAR, Ui::rgba(255, 185, 40), clrBgDark, clrWhite);

                drawStatBar(textBuf, 22.0f + dTop, 108.0f, "Overall Diff (OD)", curOD, Ui::rgba(180, 80, 240), clrBgDark, clrWhite);
                drawStatBar(textBuf, 206.0f + dTop, 108.0f, "HP Drain Rate", curHP, Ui::rgba(60, 215, 120), clrBgDark, clrWhite);

                drawText(textBuf, 22.0f + dTop, 142.0f, 0.34f, clrMuted, LOC(STR_MODS_EQUIPPED), 0.25f);
                float pillX = 120.0f + dTop;
                if (settings.activeMods & MOD_HIDDEN) {
                    Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(255, 205, 50));
                    drawCenteredText(textBuf, pillX + 14.0f, 146.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "HD", 0.30f);
                    pillX += 34.0f;
                }
                if (settings.activeMods & MOD_NOFAIL) {
                    Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(40, 215, 235));
                    drawCenteredText(textBuf, pillX + 14.0f, 146.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "NF", 0.30f);
                    pillX += 34.0f;
                }
                if (settings.activeMods & MOD_HARDROCK) {
                    Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(240, 70, 70));
                    drawCenteredText(textBuf, pillX + 14.0f, 146.0f, 0.30f, clrWhite, "HR", 0.30f);
                    pillX += 34.0f;
                }
                if (settings.activeMods & MOD_EASY) {
                    Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(80, 200, 110));
                    drawCenteredText(textBuf, pillX + 14.0f, 146.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "EZ", 0.30f);
                    pillX += 34.0f;
                }

                Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
                drawText(textBuf, 12.0f, 221.0f, 0.34f, clrMuted, LOC(STR_MODS_CONTROLS_HINT));
                Ui::drawTransition(400.0f, 240.0f, screenFade, clrPrimary);
            }

            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.02f);

            drawTextWithShadow(textBuf, 14.0f, 8.0f, 0.40f, clrWhite, LOC(STR_MODS_TITLE), 0.20f);
            char bMult[16];
            std::snprintf(bMult, sizeof(bMult), "%.2fx", currentMult);
            Ui::pill(252.0f, 6.0f, 54.0f, 18.0f, 0.25f, Ui::rgba(30, 80, 150));
            drawCenteredText(textBuf, 279.0f, 14.0f, 0.34f, clrWhite, bMult, 0.30f);

            drawTextWithShadow(textBuf, 14.0f, 36.0f, 0.32f, Ui::rgba(215, 210, 235), LOC(STR_MODS_DIFF_REDUCTION), 0.20f);

            bool nfOn = (settings.activeMods & MOD_NOFAIL) != 0;
            Ui::button(14.0f, 54.0f, 138.0f, 58.0f, 0.20f, nfOn ? Ui::rgba(22, 42, 60, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235), nfOn);
            Ui::pill(22.0f, 62.0f, 32.0f, 32.0f, 0.25f, Ui::rgba(40, 215, 235));
            drawCenteredText(textBuf, 38.0f, 78.0f, 0.40f, Ui::rgba(0, 0, 0, 240), "NF", 0.30f);
            drawTextWithShadow(textBuf, 60.0f, 60.0f, 0.38f, clrWhite, "No Fail", 0.25f);
            drawTextWithShadow(textBuf, 60.0f, 74.0f, 0.30f, Ui::rgba(215, 210, 235), ("-50% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
            Ui::pill(60.0f, 88.0f, 48.0f, 16.0f, 0.26f, nfOn ? Ui::rgba(40, 215, 235) : Ui::rgba(45, 40, 60));
            drawCenteredText(textBuf, 84.0f, 96.0f, 0.28f, nfOn ? Ui::rgba(0, 0, 0, 240) : clrMuted, nfOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

            bool ezOn = (settings.activeMods & MOD_EASY) != 0;
            Ui::button(168.0f, 54.0f, 138.0f, 58.0f, 0.20f, ezOn ? Ui::rgba(22, 48, 36, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(80, 200, 110), ezOn);
            Ui::pill(176.0f, 62.0f, 32.0f, 32.0f, 0.25f, ezOn ? Ui::rgba(80, 200, 110) : Ui::rgba(45, 40, 60));
            drawCenteredText(textBuf, 192.0f, 78.0f, 0.40f, ezOn ? Ui::rgba(0, 0, 0, 240) : clrMuted, "EZ", 0.30f);
            drawTextWithShadow(textBuf, 214.0f, 60.0f, 0.38f, clrWhite, "Easy", 0.25f);
            drawTextWithShadow(textBuf, 214.0f, 74.0f, 0.30f, Ui::rgba(215, 210, 235), ("-50% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
            Ui::pill(214.0f, 88.0f, 48.0f, 16.0f, 0.26f, ezOn ? Ui::rgba(80, 200, 110) : Ui::rgba(45, 40, 60));
            drawCenteredText(textBuf, 238.0f, 96.0f, 0.28f, ezOn ? Ui::rgba(0, 0, 0, 240) : clrMuted, ezOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

            drawTextWithShadow(textBuf, 14.0f, 118.0f, 0.32f, Ui::rgba(215, 210, 235), LOC(STR_MODS_DIFF_INCREASE), 0.20f);

            bool hdOn = (settings.activeMods & MOD_HIDDEN) != 0;
            Ui::button(14.0f, 132.0f, 138.0f, 58.0f, 0.20f, hdOn ? Ui::rgba(48, 40, 22, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(255, 205, 50), hdOn);
            Ui::pill(22.0f, 140.0f, 32.0f, 32.0f, 0.25f, hdOn ? Ui::rgba(255, 205, 50) : Ui::rgba(45, 40, 60));
            drawCenteredText(textBuf, 38.0f, 156.0f, 0.40f, hdOn ? Ui::rgba(0, 0, 0, 240) : clrMuted, "HD", 0.30f);
            drawTextWithShadow(textBuf, 60.0f, 138.0f, 0.38f, clrWhite, "Hidden", 0.25f);
            drawTextWithShadow(textBuf, 60.0f, 152.0f, 0.30f, Ui::rgba(215, 210, 235), ("+6% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
            Ui::pill(60.0f, 166.0f, 48.0f, 16.0f, 0.26f, hdOn ? Ui::rgba(255, 205, 50) : Ui::rgba(45, 40, 60));
            drawCenteredText(textBuf, 84.0f, 174.0f, 0.28f, hdOn ? Ui::rgba(0, 0, 0, 240) : clrMuted, hdOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

            bool hrOn = (settings.activeMods & MOD_HARDROCK) != 0;
            Ui::button(168.0f, 132.0f, 138.0f, 58.0f, 0.20f, hrOn ? Ui::rgba(52, 24, 28, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(240, 70, 70), hrOn);
            Ui::pill(176.0f, 140.0f, 32.0f, 32.0f, 0.25f, hrOn ? Ui::rgba(240, 70, 70) : Ui::rgba(45, 40, 60));
            drawCenteredText(textBuf, 192.0f, 156.0f, 0.40f, clrWhite, "HR", 0.30f);
            drawTextWithShadow(textBuf, 214.0f, 138.0f, 0.38f, clrWhite, "Hard Rock", 0.25f);
            drawTextWithShadow(textBuf, 214.0f, 152.0f, 0.30f, Ui::rgba(215, 210, 235), ("+6% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
            Ui::pill(214.0f, 166.0f, 48.0f, 16.0f, 0.26f, hrOn ? Ui::rgba(240, 70, 70) : Ui::rgba(45, 40, 60));
            drawCenteredText(textBuf, 238.0f, 174.0f, 0.28f, hrOn ? clrWhite : clrMuted, hrOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

            Ui::button(20.0f, 198.0f, 280.0f, 34.0f, 0.20f, Ui::rgba(42, 20, 52, 240), clrPrimary, true);
            Ui::pill(28.0f, 204.0f, 22.0f, 22.0f, 0.25f, clrPrimary);
            drawCenteredText(textBuf, 39.0f, 215.0f, 0.32f, clrWhite, "B", 0.28f);
            drawCenteredTextWithShadow(textBuf, 160.0f, 215.0f, 0.42f, clrWhite, LOC(STR_MULTI_BACK_TO_ROOM), 0.28f);

            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 4C. RESULTADOS MULTIJUGADOR (PODIO Y ESTADÍSTICAS 1V1)
        // =========================================================
        if (appState == STATE_MULTI_RESULTS) {
            audio.update();
            multiMgr.update(false);

            if (!multiMgr.inMultiplayer) {
                multiMgr.startBrowser(sysUser);
                appState = STATE_MULTI_BROWSER;
                continue;
            }

            // Si el anfitrión envió la señal de retorno a la sala sincronizada
            if (!multiMgr.isHost() && multiMgr.shouldReturnToLobby) {
                multiMgr.shouldReturnToLobby = false;
                multiMgr.matchStarted = false;
                multiMgr.matchInProgressFlag = false;
                multiMgr.players[multiMgr.localSlot].status = STATUS_NOT_READY;
                multiMgr.sendPlayerUpdate();
                appState = STATE_MULTI_LOBBY;
                continue;
            }

            // Salir de la sala con [B]
            if (kDown & KEY_B) {
                audio.playHitSound(0);
                if (multiMgr.isHost()) {
                    multiMgr.notifyRoomClosed();
                } else {
                    multiMgr.notifyPlayerLeave();
                }
                multiMgr.stop();
                multiMgr.startBrowser(sysUser);
                appState = STATE_MULTI_BROWSER;
                continue;
            }

            bool allFinished = multiMgr.areAllPlayersFinished();
            bool touchedContinue = touchJustDown && (touch.px >= 12.0f && touch.px <= 308.0f && touch.py >= 188.0f && touch.py <= 232.0f);

            if ((kDown & KEY_A) || touchedContinue) {
                if (!allFinished) {
                    audio.playHitSound(4); // Bloquear avance si aún hay jugadores activos en partida
                } else if (multiMgr.isHost()) {
                    audio.playHitSound(2);
                    multiMgr.matchStarted = false;
                    multiMgr.matchInProgressFlag = false;
                    multiMgr.sendReturnToLobby();
                    multiMgr.advanceQueueAndRotateHost();
                    appState = STATE_MULTI_LOBBY;
                    continue;
                } else {
                    audio.playHitSound(0);
                }
            }

            if (kDown & (KEY_X | KEY_Y)) {
                audio.playHitSound(0);
                multiResultsTab = (multiResultsTab + 1) % 4;
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            auto ranked = multiMgr.getLeaderboard();

            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                float dTop = depthShift(2.0f);
                Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);

                Ui::pill(12.0f + dTop, 10.0f, 52.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
                drawCenteredText(textBuf, 38.0f + dTop, 18.0f, 0.36f, clrWhite, "osu!", 0.30f);

                drawText(textBuf, 70.0f + dTop, 11.0f, 0.44f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_RESULTS_TITLE), 0.30f);

                std::string rSongInfo = multiMgr.activeBeatmap.title + " [" + multiMgr.activeBeatmap.version + "]";
                drawText(textBuf, 250.0f + dTop, 12.0f, 0.32f, clrMuted, truncateString(rSongInfo, 22).c_str(), 0.30f);

                for (size_t r = 0; r < ranked.size() && r < 4; ++r) {
                    const auto& item = ranked[r];
                    float cardY = 34.0f + r * 44.0f;
                    float cardX = 12.0f + dTop;
                    float cardW = 376.0f;
                    float cardH = 40.0f;

                    u32 medalClr = item.player->isFailed ? Ui::rgba(235, 45, 45) :
                                   ((r == 0) ? Ui::rgba(255, 195, 45) :
                                   ((r == 1) ? Ui::rgba(40, 215, 235) :
                                   ((r == 2) ? Ui::rgba(180, 80, 240) : Ui::rgba(100, 100, 130))));

                    Ui::panel(cardX, cardY, cardW, cardH, 0.20f, Ui::rgba(22, 19, 36, 240), medalClr);

                    Ui::panel(cardX + 6.0f, cardY + 6.0f, 26.0f, 28.0f, 0.22f, medalClr);
                    char rankTxt[8];
                    if (item.player->isFailed) {
                        std::snprintf(rankTxt, sizeof(rankTxt), "FAIL");
                    } else {
                        std::snprintf(rankTxt, sizeof(rankTxt), "#%u", static_cast<unsigned int>(r + 1));
                    }
                    drawCenteredText(textBuf, cardX + 19.0f, cardY + 20.0f, 0.32f, Ui::rgba(0, 0, 0, 250), rankTxt, 0.28f);

                    drawText(textBuf, cardX + 38.0f, cardY + 6.0f, 0.44f, clrWhite, item.player->name, 0.25f);
                    if (item.player->isFailed) {
                        Ui::pill(cardX + 105.0f, cardY + 7.0f, 32.0f, 13.0f, 0.26f, Ui::rgba(235, 45, 45));
                        drawCenteredText(textBuf, cardX + 121.0f, cardY + 13.0f, 0.26f, clrWhite, LOC(STR_MULTI_FAIL), 0.30f);
                    }
                    if (item.player->isLocal) {
                        float tuX = item.player->isFailed ? (cardX + 142.0f) : (cardX + 140.0f);
                        Ui::pill(tuX, cardY + 7.0f, 24.0f, 13.0f, 0.26f, Ui::rgba(40, 215, 235));
                        drawCenteredText(textBuf, tuX + 12.0f, cardY + 13.0f, 0.26f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_YOU), 0.30f);
                    }

                    std::string mStr = (item.player->activeMods != MOD_NONE) ? ("+" + formatModsString(item.player->activeMods)) : "No Mod";
                    drawText(textBuf, cardX + 38.0f, cardY + 22.0f, 0.30f, Ui::rgba(40, 215, 235), mStr.c_str(), 0.25f);

                    std::string scStr = formatScoreThousands(item.player->score);
                    drawText(textBuf, cardX + 180.0f, cardY + 10.0f, 0.52f, clrWhite, scStr.c_str(), 0.25f);

                    char accComb[32];
                    std::snprintf(accComb, sizeof(accComb), "%.2f%%", item.player->accuracy);
                    drawText(textBuf, cardX + 276.0f, cardY + 6.0f, 0.34f, Ui::rgba(40, 215, 235), accComb, 0.25f);
                    char cmb[16];
                    std::snprintf(cmb, sizeof(cmb), "%dx", item.player->maxCombo);
                    drawText(textBuf, cardX + 282.0f, cardY + 22.0f, 0.30f, clrMuted, cmb, 0.25f);

                    u32 gClr = getScoreGradeColor(item.player->grade, clrCursor);
                    drawCenteredText(textBuf, cardX + cardW - 20.0f, cardY + 20.0f, 0.70f, gClr, item.player->grade, 0.28f);
                }

                Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
                if (multiMgr.hasToastNotice()) {
                    float dToast = depthShift(4.0f);
                    Ui::glow(200.0f + dToast, 224.0f, 0.88f, 160.0f, Ui::rgba(255, 60, 80, 110), 3);
                    Ui::pill(30.0f + dToast, 212.0f, 340.0f, 24.0f, 0.90f, Ui::rgba(35, 12, 22, 245));
                    drawRectOutline(30.0f + dToast, 212.0f, 340.0f, 24.0f, 1.2f, Ui::rgba(255, 80, 100), 0.92f);
                    drawCenteredTextWithShadow(textBuf, 200.0f + dToast, 224.0f, 0.34f, Ui::rgba(255, 230, 230), multiMgr.toastNotice.c_str(), 0.95f);
                } else if (!allFinished) {
                    drawCenteredTextWithShadow(textBuf, 200.0f + dTop, 222.0f, 0.33f, Ui::rgba(255, 205, 50), LOC(STR_MULTI_WAITING_ALL), 0.32f);
                } else {
                    float byY = 219.0f;
                    drawKeyBadge(textBuf, 12.0f + dTop, byY, "A", multiMgr.isHost() ? LOC(STR_MULTI_RETURN_LOBBY) : LOC(STR_MULTI_WAIT_HOST), Ui::rgba(255, 75, 80));
                    drawKeyBadge(textBuf, 160.0f + dTop, byY, "B", LOC(STR_BACK), Ui::rgba(255, 195, 45));
                    drawKeyBadge(textBuf, 240.0f + dTop, byY, "X", LOC(STR_MULTI_TABS), Ui::rgba(50, 165, 255));
                }

                Ui::drawTransition(400.0f, 240.0f, screenFade, clrPrimary);
            }

            C2D_TargetClear(bottomScreen, clrTabletBg);
            C2D_SceneBegin(bottomScreen);

            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.02f);

            const char* resTabs[4] = { LOC(STR_MULTI_TAB_HITS), "Hit Error", "1v1 vs #1", "Chat" };
            for (int t = 0; t < 4; ++t) {
                float tx = 10.0f + t * 74.0f;
                bool activeT = (multiResultsTab == t);
                Ui::panel(tx, 8.0f, 70.0f, 22.0f, 0.20f, activeT ? Ui::rgba(40, 215, 235) : Ui::rgba(24, 20, 36, 220));
                drawCenteredText(textBuf, tx + 35.0f, 19.0f, 0.30f, activeT ? Ui::rgba(0, 0, 0, 250) : clrMuted, resTabs[t], 0.25f);
            }

            const auto& winner = ranked.empty() ? multiMgr.players[multiMgr.localSlot] : *ranked[0].player;

            float boxW = 68.0f;
            float boxH = 50.0f;

            // Box 300
            Ui::panel(12.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(40, 215, 235));
            drawCenteredText(textBuf, 12.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(40, 215, 235), "300", 0.25f);
            char c3Str[16]; std::snprintf(c3Str, sizeof(c3Str), "%d", winner.count300);
            drawCenteredText(textBuf, 12.0f + boxW * 0.5f, 68.0f, 0.48f, clrWhite, c3Str, 0.25f);

            // Box 100
            Ui::panel(86.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(60, 215, 120));
            drawCenteredText(textBuf, 86.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(60, 215, 120), "100", 0.25f);
            char c1Str[16]; std::snprintf(c1Str, sizeof(c1Str), "%d", winner.count100);
            drawCenteredText(textBuf, 86.0f + boxW * 0.5f, 68.0f, 0.48f, clrWhite, c1Str, 0.25f);

            // Box 50
            Ui::panel(160.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(255, 185, 40));
            drawCenteredText(textBuf, 160.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(255, 185, 40), "50", 0.25f);
            char c5Str[16]; std::snprintf(c5Str, sizeof(c5Str), "%d", winner.count50);
            drawCenteredText(textBuf, 160.0f + boxW * 0.5f, 68.0f, 0.48f, clrWhite, c5Str, 0.25f);

            // Box MISS
            Ui::panel(234.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(240, 70, 70));
            drawCenteredText(textBuf, 234.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(240, 70, 70), "MISS", 0.25f);
            char cmStr[16]; std::snprintf(cmStr, sizeof(cmStr), "%d", winner.countMiss);
            drawCenteredText(textBuf, 234.0f + boxW * 0.5f, 68.0f, 0.48f, clrWhite, cmStr, 0.25f);

            Ui::panel(12.0f, 96.0f, 296.0f, 54.0f, 0.20f, Ui::rgba(22, 19, 36, 240));
            drawText(textBuf, 20.0f, 102.0f, 0.30f, clrMuted, LOC(STR_MULTI_SEL_PLAYER), 0.25f);
            drawText(textBuf, 20.0f, 116.0f, 0.42f, clrWhite, winner.name, 0.25f);

            char cmbLabel[32];
            std::snprintf(cmbLabel, sizeof(cmbLabel), "Combo: %d / %dx", winner.combo, winner.maxCombo);
            drawText(textBuf, 190.0f, 102.0f, 0.30f, clrMuted, cmbLabel, 0.25f);

            char pRel[32];
            std::snprintf(pRel, sizeof(pRel), "%.2f%%", winner.accuracy);
            drawText(textBuf, 20.0f, 134.0f, 0.30f, clrMuted, LOC(STR_MULTI_REL_ACC), 0.25f);
            drawText(textBuf, 250.0f, 134.0f, 0.32f, Ui::rgba(40, 215, 235), pRel, 0.25f);

            Ui::statBar(12.0f, 154.0f, 296.0f, 6.0f, 0.20f, winner.accuracy / 100.0f, Ui::rgba(40, 215, 235));

            if (!multiMgr.mapQueue.empty()) {
                char nextQMsg[64];
                std::snprintf(nextQMsg, sizeof(nextQMsg), LOC(STR_MULTI_NEXT_IN_QUEUE), multiMgr.mapQueue.front().title.c_str());
                drawCenteredText(textBuf, 160.0f, 172.0f, 0.34f, Ui::rgba(255, 205, 50), truncateString(nextQMsg, 36).c_str(), 0.25f);
            } else {
                drawCenteredText(textBuf, 160.0f, 172.0f, 0.32f, clrMuted, LOC(STR_MULTI_QUEUE_DONE), 0.25f);
            }

            if (!allFinished) {
                Ui::panel(12.0f, 188.0f, 296.0f, 44.0f, 0.20f, Ui::rgba(50, 35, 70, 240), Ui::rgba(130, 80, 200));
                drawCenteredText(textBuf, 160.0f, 203.0f, 0.38f, Ui::rgba(255, 215, 60), LOC(STR_MULTI_WAITING_ALL), 0.25f);
                drawCenteredText(textBuf, 160.0f, 219.0f, 0.28f, clrWhite, LOC(STR_MULTI_WAITING_ALL), 0.25f);
            } else if (multiMgr.isHost()) {
                Ui::button(12.0f, 188.0f, 296.0f, 44.0f, 0.20f, Ui::rgba(30, 165, 80, 240), Ui::rgba(100, 255, 160), true);
                Ui::pill(20.0f, 198.0f, 24.0f, 24.0f, 0.24f, Ui::rgba(100, 255, 160));
                drawCenteredText(textBuf, 32.0f, 210.0f, 0.34f, Ui::rgba(0, 0, 0, 240), "A", 0.28f);
                drawCenteredTextWithShadow(textBuf, 160.0f, 210.0f, 0.44f, clrWhite, LOC(STR_MULTI_RETURN_LOBBY), 0.28f);
            } else {
                Ui::button(12.0f, 188.0f, 296.0f, 44.0f, 0.20f, Ui::rgba(28, 38, 62, 240), Ui::rgba(60, 140, 230), false);
                drawCenteredTextWithShadow(textBuf, 160.0f, 203.0f, 0.38f, Ui::rgba(100, 200, 255), LOC(STR_MULTI_MATCH_FINISHED), 0.25f);
                drawCenteredTextWithShadow(textBuf, 160.0f, 219.0f, 0.30f, clrWhite, LOC(STR_MULTI_WAIT_HOST), 0.25f);
            }

            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 4D. SINCRONIZACION PREVIA AL JUEGO (CARGA DE RECURSOS Y HANDSHAKE)
        // =========================================================
        if (appState == STATE_MULTI_SYNC) {
            audio.update();
            multiMgr.update(false);

            if (!multiMgr.inMultiplayer) {
                multiMgr.startBrowser(sysUser);
                appState = STATE_MULTI_BROWSER;
                continue;
            }

            // Cancelar con B si el jugador decide abortar
            if (kDown & KEY_B) {
                audio.playHitSound(0);
                if (multiMgr.isHost()) {
                    multiMgr.notifyRoomClosed();
                } else {
                    multiMgr.notifyPlayerLeave();
                }
                multiMgr.stop();
                multiMgr.startBrowser(sysUser);
                appState = STATE_MULTI_BROWSER;
                continue;
            }

            // Fase 1: Carga de recursos a memoria (una sola vez)
            if (!multiSyncLoaded) {
                // Dibujar una pantalla de carga rapida antes de bloquear lectura de SD
                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                    C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                    C2D_TargetClear(curTop, clrBgDark);
                    C2D_SceneBegin(curTop);
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);
                    Ui::panel(50.0f, 80.0f, 300.0f, 80.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235));
                    drawCenteredText(textBuf, 200.0f, 108.0f, 0.44f, clrWhite, LOC(STR_MULTI_LOADING_RESOURCES), 0.25f);
                    drawCenteredText(textBuf, 200.0f, 134.0f, 0.32f, clrMuted, LOC(STR_MULTI_PREP_MAP), 0.25f);
                }
                C2D_TargetClear(bottomScreen, clrBgDark);
                C2D_SceneBegin(bottomScreen);
                Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);
                drawCenteredText(textBuf, 160.0f, 120.0f, 0.36f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_PREP_MAP), 0.25f);
                C3D_FrameEnd(0);

                std::string osuFileToLoad = findLocalMapFile(multiMgr.activeBeatmap.folderPath, multiMgr.activeBeatmap.osuFilename);
                if (osuFileToLoad.empty()) {
                    osuFileToLoad = "sdmc:/3ds/osu-3ds/songs/" + multiMgr.activeBeatmap.folderPath + "/" + multiMgr.activeBeatmap.osuFilename;
                }

                if (beatmap.loadFromFile(osuFileToLoad, false)) {
                    uint32_t matchMods = (multiMgr.modMode == MODS_HOST_ONLY)
                        ? multiMgr.players[multiMgr.hostSlot].activeMods
                        : multiMgr.players[multiMgr.localSlot].activeMods;
                    beatmap.applyMods(matchMods);
                    settings.activeMods = matchMods;

                    bgLoader.cancel();
                    previewDelay = 0;
                    if (!beatmap.fullBackgroundPath.empty()) {
                        bgTex.loadFromFile(beatmap.fullBackgroundPath, bgMaxW, bgMaxH);
                    }
                    if (settings.enableVideo && !beatmap.fullVideoPath.empty()) {
                        videoPlayer.openVideo(beatmap.fullVideoPath, beatmap.videoOffsetMs);
                    } else {
                        videoPlayer.closeVideo();
                    }

                    audio.setVolumes(settings.musicVolume, settings.sfxVolume);
                    applyActiveSkinAudio();
                    if (settings.beatmapHitsounds && !beatmap.folderPath.empty()) {
                        audio.loadBeatmapHitsounds(beatmap.folderPath, beatmap.sampleSetName);
                    }

                    bool isTaikoMatch = (settings.gameMode == MODE_TAIKO || beatmap.mode == 1);
                    bool isCatchMatch = (settings.gameMode == MODE_CATCH || beatmap.mode == 2);
                    bool isManiaMatch = (settings.gameMode == MODE_MANIA || beatmap.mode == 3);
                    if (isTaikoMatch) {
                        taikoEngine.initFromBeatmap(beatmap);
                    } else if (isCatchMatch) {
                        catchEngine.initFromBeatmap(beatmap);
                    } else if (isManiaMatch) {
                        maniaEngine.initFromBeatmap(beatmap);
                    }

                    multiSyncLeadIn = std::max(0, beatmap.audioLeadIn);
                    if (isTaikoMatch) {
                        if (!taikoEngine.notes.empty()) {
                            int firstObjTime = taikoEngine.notes.front().time;
                            int minStartWait = 1400;
                            if (firstObjTime < minStartWait) multiSyncLeadIn = std::max(multiSyncLeadIn, minStartWait - firstObjTime);
                        }
                    } else if (isCatchMatch) {
                        if (!catchEngine.fruits.empty()) {
                            int firstObjTime = catchEngine.fruits.front().time;
                            int minStartWait = 1300;
                            if (firstObjTime < minStartWait) multiSyncLeadIn = std::max(multiSyncLeadIn, minStartWait - firstObjTime);
                        }
                    } else if (isManiaMatch) {
                        if (!maniaEngine.notes.empty()) {
                            int firstObjTime = maniaEngine.notes.front().time;
                            int minStartWait = 1200;
                            if (firstObjTime < minStartWait) multiSyncLeadIn = std::max(multiSyncLeadIn, minStartWait - firstObjTime);
                        }
                    } else if (!beatmap.circles.empty()) {
                        int firstObjTime = beatmap.circles.front().time;
                        int minStartWait = static_cast<int>(beatmap.diff.preempt) + 500;
                        if (firstObjTime < minStartWait) multiSyncLeadIn = std::max(multiSyncLeadIn, minStartWait - firstObjTime);
                    }

                    multiSyncLoaded = true;
                    multiMgr.sendMatchLoaded();
                } else {
                    multiMgr.matchStarted = false;
                    multiMgr.players[multiMgr.localSlot].status = STATUS_MISSING_MAP;
                    multiMgr.sendPlayerUpdate();
                    appState = STATE_MULTI_LOBBY;
                    continue;
                }
            }

            // Fase 2: Coordinacion del Host (comprueba si todos estan cargados)
            if (multiMgr.isHost() && multiSyncLoaded && !multiMgr.matchGo) {
                if (multiMgr.areAllPlayersLoaded()) {
                    multiMgr.sendMatchGo(500); // 500 ms de margen para sincronizar el inicio simultaneo
                }
            }

            // Fase 3: Inicio simultaneo al recibir matchGo
            if (multiMgr.matchGo) {
                audio.playHitSound(2);
                float speedRate = getModsSpeedMultiplier(settings.activeMods);
                int totalLeadIn = multiSyncLeadIn + static_cast<int>(multiMgr.matchGoDelayMs);
                audio.startMusic(beatmap.fullAudioPath, totalLeadIn, speedRate, 0);
                gameplayFallbackStartTick = osGetTime() + totalLeadIn;

                combo = 0;
                prevCombo = 0;
                lastBrokenCombo = 0;
                maxCombo = 0;
                score = 0;
                count300 = count100 = count50 = countMiss = 0;
                k1PressCount = k2PressCount = 0;
                comboScale = 1.0f;
                comboBreakAnim = 0.0f;
                hitErrorTicks.clear();
                allHitErrors.clear();
                judgementHistory.clear();
                hitErrorNeedle = 0.0f;
                lastUnstableRate = 0.0f;
                lastMeanHitError = 0.0f;
                spinnerRPM = 0.0f;
                spinnerClearAnim = 0.0f;
                spinnerBonusAnim = 0.0f;
                lastSpinnerBonus = 0;
                shakeTimer = 0.0f;
                shakeIntensity = 0.0f;
                shakeX = 0.0f;
                shakeY = 0.0f;
                hp = 1.0f;
                stageFailed = false;
                newRecordAchieved = false;
                lastEarnedPP = 0.0f;
                isPaused = false;
                pauseOption = 0;
                cursorOsuX = 256.0f;
                cursorOsuY = 192.0f;
                prevCursorOsuX = 256.0f;
                prevCursorOsuY = 192.0f;
                cursorTrail.clear();
                smokeParticles.clear();
                popups.clear();
                particles.clear();
                overtakeFlashAnim = 0.0f;
                hasOvertakenBest = false;
                skipAvailable = false;
                skipTargetMs = 0;
                inBreakPeriod = false;
                breakRemainingMs = 0;
                breakTotalMs = 0;
                breakNextObjPos = !beatmap.circles.empty() ? Vec2(beatmap.circles.front().x, beatmap.circles.front().y) : Vec2(256.0f, 192.0f);
                inKiai = false;
                prevInKiai = false;
                kiaiGlow = 0.0f;
                kiaiBurstFlash = 0.0f;
                comboBurstTimer = 0;
                comboBurstSide = 0;
                comboBurstValue = 0;
                lastComboBurstMilestone = 0;
                lastNcStep = -999999;
                taikoGuideTimer = 240.0f;

                multiMgr.players[multiMgr.localSlot].resetForMatch();
                multiMgr.matchStarted = false;
                multiMgr.matchGo = false;

                appState = STATE_PLAYING;
                continue;
            }

            // Renderizado de la pantalla de espera de sincronizacion pre-partida
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
                float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                C2D_TargetClear(curTop, clrBgDark);
                C2D_SceneBegin(curTop);
                auto depthShift = [&](float px) { return eyeSign * slider3D * px; };

                float dBg = depthShift(-3.0f);
                if (bgTex.loaded) {
                    bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(14, 10, 24, 180));
                } else {
                    Ui::backdrop(400.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);
                }

                float dPan = depthShift(2.5f);
                Ui::panel(40.0f + dPan, 35.0f, 320.0f, 150.0f, 0.20f, Ui::rgba(20, 16, 32, 240), Ui::rgba(40, 215, 235));
                drawCenteredText(textBuf, 200.0f + dPan, 52.0f, 0.44f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_WAIT_OTHER_CONSOLES), 0.25f);

                char syncTxt[64];
                std::snprintf(syncTxt, sizeof(syncTxt), "%s: %d / %d", LOC(STR_READY), multiMgr.getLoadedPlayerCount(), multiMgr.getActivePlayerCount());
                drawCenteredText(textBuf, 200.0f + dPan, 72.0f, 0.34f, clrWhite, syncTxt, 0.25f);

                // Ranuras de jugadores con su estado de carga
                for (int s = 0; s < MULTI_MAX_PLAYERS; ++s) {
                    if (multiMgr.players[s].active) {
                        float py = 92.0f + s * 20.0f;
                        drawText(textBuf, 60.0f + dPan, py, 0.32f, clrWhite, multiMgr.players[s].name, 0.25f);
                        bool isL = (multiMgr.players[s].status == STATUS_LOADED);
                        Ui::pill(270.0f + dPan, py, 70.0f, 16.0f, 0.26f, isL ? Ui::rgba(30, 180, 80) : Ui::rgba(220, 140, 30));
                        drawCenteredText(textBuf, 305.0f + dPan, py + 8.0f, 0.28f, clrWhite, isL ? LOC(STR_READY) : LOC(STR_MULTI_LOADING_RESOURCES), 0.30f);
                    }
                }

                drawCenteredText(textBuf, 200.0f + dPan, 205.0f, 0.32f, clrMuted, LOC(STR_MULTI_SAME_TIME), 0.25f);
                drawCenteredText(textBuf, 200.0f + dPan, 222.0f, 0.28f, Ui::rgba(255, 90, 90), LOC(STR_MULTI_BACK_TO_ROOM), 0.25f);
            }

            C2D_TargetClear(bottomScreen, clrBgDark);
            C2D_SceneBegin(bottomScreen);
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);
            Ui::panel(16.0f, 40.0f, 288.0f, 160.0f, 0.20f, Ui::rgba(22, 18, 36, 240), Ui::rgba(85, 75, 110));
            drawCenteredText(textBuf, 160.0f, 65.0f, 0.44f, clrWhite, truncateString(multiMgr.activeBeatmap.title, 24).c_str(), 0.25f);
            drawCenteredText(textBuf, 160.0f, 85.0f, 0.34f, clrMuted, truncateString(multiMgr.activeBeatmap.artist, 26).c_str(), 0.25f);
            drawCenteredText(textBuf, 160.0f, 105.0f, 0.34f, Ui::rgba(255, 205, 50), truncateString(multiMgr.activeBeatmap.version, 24).c_str(), 0.25f);
            drawCenteredText(textBuf, 160.0f, 145.0f, 0.38f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_SYNC_CLOCK), 0.25f);
            C3D_FrameEnd(0);
            continue;
        }

        // =========================================================
        // 5. JUEGO ACTIVO + MENÚ DE PAUSA CORREGIDO
        // =========================================================
        bool isTaikoPlay = (settings.gameMode == MODE_TAIKO || beatmap.mode == 1);

        // En multijugador la pausa está totalmente deshabilitada para mantener la sincronía en tiempo real
        if (multiMgr.inMultiplayer) {
            isPaused = false;
        }

        if (!multiMgr.inMultiplayer && (kDown & KEY_START)) {
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
            if (isTaikoPlay) {
                bool touchBgToggle = touchJustDown && (touch.px >= 40 && touch.px <= 280 && touch.py >= 204 && touch.py <= 238);
                if ((kDown & KEY_X) || touchBgToggle) {
                    settings.taikoBgMode = (settings.taikoBgMode + 1) % 3;
                    settings.save();
                    audio.playHitSound(2);
                }
            }

            bool triggerPauseAction = (kDown & KEY_A) != 0;

            // Botones táctiles en la pantalla inferior durante la pausa
            if (touchJustDown && touch.px >= 28 && touch.px <= 292) {
                if (touch.py >= 36 && touch.py <= 84) {
                    pauseOption = 0;
                    triggerPauseAction = true;
                } else if (touch.py >= 88 && touch.py <= 136) {
                    pauseOption = 1;
                    triggerPauseAction = true;
                } else if (touch.py >= 140 && touch.py <= 188) {
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
                    videoPlayer.closeVideo();
                    applyActiveSkinAudio();
                    if (multiMgr.inMultiplayer) {
                        multiMgr.stop();
                        appState = STATE_MAIN_MENU;
                    } else {
                        appState = STATE_SONG_SELECT;
                        updateSongPreview();
                    }
                    continue;
                }
            }
        } else {
            audio.update();
        }

        if (multiMgr.inMultiplayer) {
            multiMgr.players[multiMgr.localSlot].score = score;
            multiMgr.players[multiMgr.localSlot].combo = combo;
            multiMgr.players[multiMgr.localSlot].maxCombo = maxCombo;
            int totalJudged = count300 + count100 + count50 + countMiss;
            multiMgr.players[multiMgr.localSlot].accuracy = (totalJudged > 0)
                ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                : 100.0f;
            multiMgr.players[multiMgr.localSlot].hp = hp;
            multiMgr.update(true);

            if (!multiMgr.inMultiplayer) {
                audio.stopMusic();
                videoPlayer.closeVideo();
                multiMgr.startBrowser(sysUser);
                appState = STATE_MULTI_BROWSER;
                continue;
            }
        }

        float speedRate    = getModsSpeedMultiplier(settings.activeMods);
        int songTime = 0;
        if (audio.getIsPlaying()) {
            songTime = audio.getTimeMs() - settings.audioOffsetMs;
        } else {
            // Reloj monotónico de respaldo para emuladores (Citra / Azahar) o fallos de audio
            u64 nowTick = osGetTime();
            if (nowTick < gameplayFallbackStartTick) {
                songTime = -static_cast<int>(gameplayFallbackStartTick - nowTick) - settings.audioOffsetMs;
            } else {
                songTime = static_cast<int>((nowTick - gameplayFallbackStartTick) * speedRate) - settings.audioOffsetMs;
            }
        }
        if (settings.enableVideo) {
            videoPlayer.update(songTime, isPaused);
        }
        bool isAutoplay    = (settings.activeMods & MOD_AUTO) != 0;
        bool isHidden      = (settings.activeMods & MOD_HD) != 0;
        bool isNoFail      = (settings.activeMods & MOD_NF) != 0;
        bool isSuddenDeath = (settings.activeMods & MOD_SD) != 0;
        bool isPerfect     = (settings.activeMods & MOD_PF) != 0;
        bool isFlashlight  = (settings.activeMods & MOD_FL) != 0;
        bool isRelax       = (settings.activeMods & MOD_RX) != 0;
        bool isAutoPilot   = (settings.activeMods & MOD_AP) != 0;
        bool isSpunOut     = (settings.activeMods & MOD_SO) != 0;
        float scoreMult    = getModsScoreMultiplier(settings.activeMods);
        isTaikoPlay        = (settings.gameMode == MODE_TAIKO || beatmap.mode == 1);
        bool isCatchPlay   = (settings.gameMode == MODE_CATCH || beatmap.mode == 2);
        bool isManiaPlay   = (settings.gameMode == MODE_MANIA || beatmap.mode == 3);

        // 1. Evaluar si estamos en período de Descanso (Break Time)
        inBreakPeriod = false;
        breakRemainingMs = 0;
        breakTotalMs = 0;
        for (const auto& brk : beatmap.breaks) {
            if (songTime >= brk.startTime && songTime <= brk.endTime) {
                inBreakPeriod = true;
                breakRemainingMs = brk.endTime - songTime;
                breakTotalMs = std::max(1, brk.endTime - brk.startTime);
                break;
            }
        }

        // Buscar coordenadas del siguiente objeto para el indicador de advertencia
        for (const auto& obj : beatmap.circles) {
            if (obj.time > songTime) {
                breakNextObjPos = {obj.x, obj.y};
                break;
            }
        }

        // 2. Evaluar disponibilidad de SKIP (Intro o Descanso largo, solo en Single Player)
        skipAvailable = false;
        skipTargetMs = 0;
        if (!multiMgr.inMultiplayer) {
            if (!beatmap.circles.empty()) {
                int firstNoteTime = beatmap.circles.front().time;
                int introWait = firstNoteTime - static_cast<int>(beatmap.diff.preempt) - 1000;
                if (introWait > songTime + 2200) {
                    skipAvailable = true;
                    skipTargetMs = introWait;
                }
            }
            if (!skipAvailable && inBreakPeriod && breakRemainingMs > 2600) {
                skipAvailable = true;
                skipTargetMs = songTime + breakRemainingMs - 1200;
            }
        }

        // 3. Procesar activación de SKIP (Toque táctil o botones ZL / ZR / START)
        // touchJustDown únicamente (isTouching repetitivo en cada frame provocaba lag y desync)
        bool touchOnSkip = touchJustDown && (touch.px >= 195 && touch.px <= 318 && touch.py >= 185 && touch.py <= 240);
        bool btnSkip = (kDown & (KEY_ZL | KEY_ZR | KEY_START)) != 0;
        if (skipAvailable && !isPaused && (touchOnSkip || btnSkip)) {
            audio.playHitSound(2);
            audio.seekToMs(skipTargetMs);
            songTime = audio.getTimeMs() - settings.audioOffsetMs;
            lastNcStep = -999999;
            if (settings.enableVideo) {
                videoPlayer.update(songTime, isPaused);
            }
            skipAvailable = false;
            kDown &= ~(KEY_ZL | KEY_ZR | KEY_START | KEY_TOUCH);
            touchJustDown = false;
            isTouching = false;
            cursorTrail.clear();
        }

        // 3b. Procesar activación de REINICIO RÁPIDO (Solo botón físico asignado como SELECT, deshabilitado en multijugador)
        bool btnRetry = (kDown & settings.keyQuickRetry) != 0;
        if (!isPaused && btnRetry && !multiMgr.inMultiplayer) {
            audio.playHitSound(2);
            startSelectedBeatmap();
            continue;
        }

        // 4. Animación y detección de superación de récord en vivo (In-Game Leaderboard, solo en Single Player)
        if (!multiMgr.inMultiplayer && !localRecords.empty() && !hasOvertakenBest) {
            if (score > localRecords[0].bestScore) {
                hasOvertakenBest = true;
                overtakeFlashAnim = 1.0f;
                audio.playHitSound(2);
                spawnParticles(256.0f, 192.0f, clr100, 16, 1.2f, 3.6f, 2.6f, 22);
            }
        }
        if (overtakeFlashAnim > 0.01f) {
            overtakeFlashAnim = Ui::approach(overtakeFlashAnim, 0.0f, 0.035f);
        }

        if (taikoGuideTimer > 0.0f && !isPaused) {
            taikoGuideTimer -= 1.0f;
        }
        float taikoGuideAlpha = (taikoGuideTimer > 60.0f) ? 1.0f : (taikoGuideTimer / 60.0f);
        if (taikoGuideAlpha < 0.0f) taikoGuideAlpha = 0.0f;

        // 5. Gestion del Efecto Kiai Time (Climax de la cancion con particulas luminosas y resplandor ritmico)
        bool nowKiai = beatmap.isKiaiAt(songTime);
        if (nowKiai && !prevInKiai) {
            kiaiBurstFlash = 1.0f;
            // Estallido inicial de estrellas y chispas al entrar en Kiai Time
            for (int i = 0; i < 20; ++i) {
                float sx = 30.0f + static_cast<float>(std::rand() % 452);
                float sy = 370.0f;
                float spd = 2.2f + static_cast<float>(std::rand() % 100) / 35.0f;
                float ang = -static_cast<float>(M_PI) * (0.28f + 0.44f * (static_cast<float>(std::rand() % 100) / 100.0f));
                u32 pClr = (i % 2 == 0) ? Ui::rgba(255, 215, 60) : Ui::rgba(255, 110, 185);
                particles.push_back({
                    sx * SCALE_FACTOR, sy * SCALE_FACTOR,
                    std::cos(ang) * spd, std::sin(ang) * spd,
                    3.2f, pClr, 38, 38
                });
            }
        }
        prevInKiai = nowKiai;
        inKiai = nowKiai;

        float targetKiai = inKiai ? 1.0f : 0.0f;
        kiaiGlow = Ui::approach(kiaiGlow, targetKiai, inKiai ? 0.08f : 0.035f);
        if (kiaiBurstFlash > 0.01f) {
            kiaiBurstFlash = Ui::approach(kiaiBurstFlash, 0.0f, 0.045f);
        }

        // 5b. Ritmo Nightcore (NC): Bombo (kick), caja (clap), charles (hat) y platillo sincronizados al compas
        if ((settings.activeMods & MOD_NC) && !isPaused && !stageFailed && !inBreakPeriod && songTime >= 0 && !audio.isSongFinished()) {
            double tpTime = 0.0;
            float beatLen = 500.0f;
            beatmap.getBeatInfoAt(songTime, tpTime, beatLen);
            if (beatLen > 10.0f) {
                double elapsed = static_cast<double>(songTime) - tpTime;
                int step = static_cast<int>(std::floor(elapsed / (beatLen * 0.5f)));
                if (step > lastNcStep) {
                    if (lastNcStep == -999999 || (step - lastNcStep) > 8) {
                        lastNcStep = step;
                    } else {
                        for (int s = lastNcStep + 1; s <= step; ++s) {
                            if ((s % 2) == 0) {
                                // Tiempo principal (compas 4/4)
                                int beatInBar = ((s / 2) % 4 + 4) % 4;
                                if (beatInBar == 0 || beatInBar == 2) {
                                    audio.playNcKick();
                                } else {
                                    audio.playNcClap();
                                }
                                // Cada 4 compases remate de platillo dance
                                if (((s / 2) % 16) == 0) {
                                    audio.playNcCrash();
                                }
                            } else {
                                // Contratiempo (corchea): charles rapido
                                audio.playNcHat();
                            }
                        }
                        lastNcStep = step;
                    }
                }
            }
        }

        // Generar chispas / estrellas flotantes continuas durante Kiai
        if (inKiai && !isPaused && (animTick % 5 == 0) && particles.size() < 48) {
            float sx = 20.0f + static_cast<float>(std::rand() % 472);
            float sy = 375.0f;
            float spd = 1.4f + static_cast<float>(std::rand() % 100) / 55.0f;
            float ang = -static_cast<float>(M_PI) * (0.32f + 0.36f * (static_cast<float>(std::rand() % 100) / 100.0f));
            u32 pClr = ((animTick / 5) % 2 == 0) ? Ui::rgba(255, 225, 70) : Ui::rgba(255, 115, 190);
            particles.push_back({
                sx * SCALE_FACTOR, sy * SCALE_FACTOR,
                std::cos(ang) * spd, std::sin(ang) * spd,
                2.5f, pClr, 32, 32
            });
        }

        auto triggerSuddenDeathFail = [&]() {
            hp = 0.0f;
            stageFailed = true;
            lastEarnedPP = 0.0f;
            audio.stopMusic();
            videoPlayer.closeVideo();
            int totalJudged = count300 + count100 + count50 + countMiss;
            float curAcc = (totalJudged > 0)
                ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                : 100.0f;
            if (multiMgr.inMultiplayer) {
                multiMgr.matchStarted = false;
                multiMgr.matchInProgressFlag = false;
                multiMgr.matchFinished = true;
                multiMgr.sendLiveGameplay(score, combo, maxCombo, curAcc, 0.0f, "F", count300, count100, count50, countMiss, true, true);
                appState = STATE_MULTI_RESULTS;
            } else {
                appState = STATE_RESULTS;
            }
        };

        bool tapWithTouch = settings.playOnBottomScreen || settings.tapWithStylus;
        u32 btnGroup1 = settings.keyOsuK1 | (tapWithTouch ? KEY_TOUCH : 0);
        u32 btnGroup2 = settings.keyOsuK2;
        bool k1Held = (kHeld & btnGroup1) != 0;
        bool k2Held = (kHeld & btnGroup2) != 0;
        bool anyHeld = (k1Held || k2Held);
        bool kActionDown = (kDown & (btnGroup1 | btnGroup2)) != 0;
        if (!isPaused && !isAutoplay) {
            if (kDown & btnGroup1) k1PressCount++;
            if (kDown & btnGroup2) k2PressCount++;
        }

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
            } else if (isAutoPilot) {
                // Mod AutoPilot: apuntado automático a las notas mientras el jugador pulsa las teclas
                for (size_t i = 0; i < beatmap.circles.size(); ++i) {
                    const auto& obj = beatmap.circles[i];
                    bool done = obj.isSlider ? obj.sliderDone :
                               (obj.isSpinner ? obj.sliderDone : (obj.state != NOT_HIT));
                    if (done) continue;

                    if (obj.isSpinner && songTime >= obj.time && songTime <= obj.endTime) {
                        float spinAng = songTime * 0.035f;
                        cursorOsuX = 256.0f + std::cos(spinAng) * 55.0f;
                        cursorOsuY = 192.0f + std::sin(spinAng) * 55.0f;
                        break;
                    } else if (obj.isSlider && songTime >= obj.time && songTime <= obj.endTime) {
                        Vec2 ball = obj.getSliderBallPos(songTime);
                        cursorOsuX = ball.x;
                        cursorOsuY = ball.y;
                        break;
                    } else {
                        float dt = static_cast<float>(obj.time - songTime);
                        if (dt <= 0.0f) {
                            cursorOsuX = obj.x;
                            cursorOsuY = obj.y;
                        } else if (dt < beatmap.diff.preempt) {
                            float lerpFactor = 0.35f;
                            cursorOsuX += (obj.x - cursorOsuX) * lerpFactor;
                            cursorOsuY += (obj.y - cursorOsuY) * lerpFactor;
                        }
                        break;
                    }
                }
            } else if (isTouching) {
                bool touchingSkip = skipAvailable && (touch.px >= 195 && touch.px <= 318 && touch.py >= 185 && touch.py <= 240);
                if (!touchingSkip) {
                    settings.mapTouchToOsu(touch.px, touch.py, cursorOsuX, cursorOsuY);
                }
            }

            // Actualizar estela dinámica y fluida del cursor (Dynamic Cursor Trail con sub-interpolación)
            float cdx = cursorOsuX - prevCursorOsuX;
            float cdy = cursorOsuY - prevCursorOsuY;
            float cdist = std::sqrt(cdx * cdx + cdy * cdy);

            if (activeSkin.cursorTrail) {
                const int trailMaxLife = 18;
                if (cdist > 1.2f) {
                    int subSteps = std::max(1, std::min(8, static_cast<int>(cdist / 4.5f)));
                    for (int s = 1; s <= subSteps; ++s) {
                        float t = static_cast<float>(s) / static_cast<float>(subSteps);
                        float ix = prevCursorOsuX + cdx * t;
                        float iy = prevCursorOsuY + cdy * t;
                        cursorTrail.push_back({ix, iy, trailMaxLife, trailMaxLife});
                    }
                    if (cursorTrail.size() > 42) {
                        cursorTrail.erase(cursorTrail.begin(), cursorTrail.begin() + (cursorTrail.size() - 42));
                    }
                } else if (cursorTrail.empty() || std::fabs(cursorTrail.back().x - cursorOsuX) > 0.5f || std::fabs(cursorTrail.back().y - cursorOsuY) > 0.5f) {
                    cursorTrail.push_back({cursorOsuX, cursorOsuY, trailMaxLife, trailMaxLife});
                    if (cursorTrail.size() > 42) cursorTrail.erase(cursorTrail.begin());
                }

                for (size_t t = 0; t < cursorTrail.size(); ) {
                    cursorTrail[t].life--;
                    if (cursorTrail[t].life <= 0) cursorTrail.erase(cursorTrail.begin() + t);
                    else ++t;
                }
            } else {
                cursorTrail.clear();
            }
            prevCursorOsuX = cursorOsuX;
            prevCursorOsuY = cursorOsuY;

            // Actualizar Humo (Smoke) estilo osu! 'C' (activado manteniendo el botón asignado en Ajustes)
            bool smokeActive = (kHeld & settings.keyOsuSmoke) != 0;
            if (smokeActive && !isPaused) {
                if (smokeParticles.size() < 240) {
                    for (int sm = 0; sm < 2; ++sm) {
                        float rndX = ((std::rand() % 100) - 50) * 0.04f;
                        float rndY = ((std::rand() % 100) - 50) * 0.04f;
                        float driftX = ((std::rand() % 100) - 50) * 0.006f;
                        float driftY = -0.045f - (static_cast<float>(std::rand() % 100) * 0.0004f);
                        float initSize = 4.8f + (std::rand() % 20) * 0.12f;
                        int sLife = 170 + (std::rand() % 50); // ~3.5 segundos
                        smokeParticles.push_back({
                            cursorOsuX + rndX, cursorOsuY + rndY,
                            driftX, driftY,
                            initSize, sLife, sLife
                        });
                    }
                }
            }

            for (size_t sm = 0; sm < smokeParticles.size(); ) {
                auto& sp = smokeParticles[sm];
                sp.life--;
                sp.x += sp.vx;
                sp.y += sp.vy;
                sp.vx *= 0.985f;
                sp.size += 0.042f; // El humo se expande suavemente al disiparse
                if (sp.life <= 0) {
                    smokeParticles.erase(smokeParticles.begin() + sm);
                } else {
                    ++sm;
                }
            }

            float hitRadius = beatmap.diff.radiusOsu;
            float hit50     = beatmap.diff.hit50;
            float hit100    = beatmap.diff.hit100;
            float hit300    = beatmap.diff.hit300;

            if (!inBreakPeriod && !beatmap.circles.empty() && songTime >= beatmap.circles.front().time &&
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
                        videoPlayer.closeVideo();
                        int totalJudged = count300 + count100 + count50 + countMiss;
                        float curAcc = (totalJudged > 0)
                            ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                            : 100.0f;
                        if (multiMgr.inMultiplayer) {
                            multiMgr.matchStarted = false;
                            multiMgr.matchInProgressFlag = false;
                            multiMgr.matchFinished = true;
                            multiMgr.sendLiveGameplay(score, combo, maxCombo, curAcc, 0.0f, "F", count300, count100, count50, countMiss, true, true);
                            appState = STATE_MULTI_RESULTS;
                        } else {
                            appState = STATE_RESULTS;
                        }
                        continue;
                    }
                }
            }

            bool allObjectsDone = true;
            bool anyActiveSpinner = false;

            if (isTaikoPlay) {
                float bpmPulse = getBpmPulse();
                taikoEngine.update(bpmPulse);

                // 1. Evaluar Misses de notas pasadas
                int tMisses = taikoEngine.checkMisses(songTime, hit50, isAutoplay);
                if (tMisses > 0) {
                    countMiss += tMisses;
                    combo = 0;
                    hp = std::max(0.0f, hp - (0.10f * tMisses));
                    triggerShake(3.0f, 10.0f);
                    audio.playComboBreak();
                    popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_MISS, 24.0f, clrMiss, 24));
                    if (isSuddenDeath || isPerfect) {
                        triggerSuddenDeathFail();
                    }
                }

                // 2. Procesar inputs de Don y Katsu
                bool donDown = false;
                bool katsuDown = false;

                if (isAutoplay) {
                    if (!taikoEngine.notes.empty()) {
                        for (size_t i = taikoEngine.nextNoteIndex; i < taikoEngine.notes.size(); ++i) {
                            auto& tn = taikoEngine.notes[i];
                            if (tn.state != NOT_HIT) continue;
                            if (tn.type == TAIKO_DRUMROLL || tn.type == TAIKO_DENDEN) {
                                if (songTime >= tn.time && songTime <= tn.endTime) {
                                    if (animTick % 4 == 0) {
                                        donDown = true;
                                        if ((animTick / 4) % 2 == 0) taikoEngine.drumHitDonLeft = 1.0f;
                                        else taikoEngine.drumHitDonRight = 1.0f;
                                    }
                                    break;
                                }
                            } else {
                                if (songTime >= tn.time - 8) {
                                    bool isDon = (tn.type == TAIKO_DON || tn.type == TAIKO_BIG_DON);
                                    if (isDon) {
                                        donDown = true;
                                        if (i % 2 == 0) taikoEngine.drumHitDonLeft = 1.0f;
                                        else taikoEngine.drumHitDonRight = 1.0f;
                                    } else {
                                        katsuDown = true;
                                        if (i % 2 == 0) taikoEngine.drumHitKatsuLeft = 1.0f;
                                        else taikoEngine.drumHitKatsuRight = 1.0f;
                                    }
                                    break;
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    // Controles físicos asignados con animación del tambor según el botón pulsado
                    if (kDown & settings.keyTaikoDonLeft) {
                        donDown = true;
                        taikoEngine.drumHitDonLeft = 1.0f;
                    }
                    if (kDown & settings.keyTaikoDonRight) {
                        donDown = true;
                        taikoEngine.drumHitDonRight = 1.0f;
                    }
                    if (kDown & settings.keyTaikoKatsuLeft) {
                        katsuDown = true;
                        taikoEngine.drumHitKatsuLeft = 1.0f;
                    }
                    if (kDown & settings.keyTaikoKatsuRight) {
                        katsuDown = true;
                        taikoEngine.drumHitKatsuRight = 1.0f;
                    }
                    if (!donDown && (kDown & settings.keyTaikoDon)) {
                        donDown = true;
                        taikoEngine.drumHitDonLeft = 1.0f;
                    }
                    if (!katsuDown && (kDown & settings.keyTaikoKatsu)) {
                        katsuDown = true;
                        taikoEngine.drumHitKatsuLeft = 1.0f;
                    }

                    // Entrada táctil en el tambor interactivo
                    if (touchJustDown) {
                        bool touchOnSkip = skipAvailable && (touch.px >= 195 && touch.px <= 318 && touch.py >= 185 && touch.py <= 240);
                        if (!touchOnSkip) {
                            int zone = taikoEngine.handleTouchDrum(touch.px, touch.py);
                            if (zone == 1 || zone == 2) donDown = true;
                            else if (zone == 3 || zone == 4) katsuDown = true;
                        }
                    }
                }

                if (donDown || katsuDown) {
                    if (donDown) k1PressCount++;
                    if (katsuDown) k2PressCount++;

                    bool isBigNote = false;
                    if (taikoEngine.nextNoteIndex > 0 && taikoEngine.nextNoteIndex <= taikoEngine.notes.size()) {
                        const auto& lastN = taikoEngine.notes[taikoEngine.nextNoteIndex - 1];
                        isBigNote = (lastN.type == TAIKO_BIG_DON || lastN.type == TAIKO_BIG_KATSU);
                    }

                    auto playTaikoSfx = [&]() {
                        if (donDown) audio.playTaikoDon(isBigNote);
                        else if (katsuDown) audio.playTaikoKatsu(isBigNote);
                    };

                    int hitRes = taikoEngine.processHit(donDown, false, songTime, hit300, hit100, hit50, isAutoplay);
                    if (hitRes == ModeTaiko::TAIKO_HIT_300) {
                        count300++;
                        combo++;
                        if (combo > maxCombo) maxCombo = combo;
                        score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                        hp = std::min(1.0f, hp + 0.05f);
                        playTaikoSfx();
                        if (combo > 0 && (combo % 50 == 0)) audio.playTaikoBalloonPop();
                        if (settings.showHit300) {
                            popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_300, 24.0f, clr300, 24));
                            spawnParticles(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, donDown ? ModeTaiko::CLR_DON_INNER : ModeTaiko::CLR_KATSU_INNER, 6, 0.8f, 2.4f, 2.2f, 15);
                        }
                    } else if (hitRes == ModeTaiko::TAIKO_HIT_100) {
                        count100++;
                        combo++;
                        if (combo > maxCombo) maxCombo = combo;
                        score += static_cast<int>((100 + 100 * combo * 0.05f) * scoreMult);
                        hp = std::min(1.0f, hp + 0.02f);
                        playTaikoSfx();
                        if (combo > 0 && (combo % 50 == 0)) audio.playTaikoBalloonPop();
                        popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_100, 20.0f, clr100, 24));
                        spawnParticles(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, clr100, 4, 0.6f, 1.8f, 2.0f, 12);
                        if (isPerfect) triggerSuddenDeathFail();
                    } else if (hitRes == ModeTaiko::TAIKO_HIT_TICK || hitRes == ModeTaiko::TAIKO_HIT_DENDEN) {
                        // Tick de redoble o denden estilo oficial Taiko: otorga puntos y combo, SIN penalizar la precisión
                        combo++;
                        if (combo > maxCombo) maxCombo = combo;
                        score += static_cast<int>(300 * scoreMult);
                        hp = std::min(1.0f, hp + 0.015f);
                        if (taikoEngine.balloonPopTimer > 0.95f) {
                            audio.playTaikoBalloonPop();
                        } else {
                            playTaikoSfx();
                        }
                        spawnParticles(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, Ui::rgba(255, 220, 60), 4, 0.8f, 2.4f, 2.2f, 12);
                    } else if (hitRes == ModeTaiko::TAIKO_HIT_MISS) {
                        countMiss++;
                        combo = 0;
                        hp = std::max(0.0f, hp - 0.12f);
                        triggerShake(3.0f, 10.0f);
                        audio.playComboBreak();
                        popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_MISS, 24.0f, clrMiss, 26));
                        if (isSuddenDeath || isPerfect) triggerSuddenDeathFail();
                    } else {
                        playTaikoSfx();
                    }
                }

                allObjectsDone = (taikoEngine.nextNoteIndex >= taikoEngine.notes.size());
            } else if (isManiaPlay) {
                maniaEngine.update(0.016f);

                // 1. Evaluar Misses de notas pasadas
                int mMisses = maniaEngine.checkMisses(songTime, hit50);
                if (mMisses > 0) {
                    countMiss += mMisses;
                    combo = 0;
                    hp = std::max(0.0f, hp - (0.08f * mMisses));
                    triggerShake(3.0f, 10.0f);
                    popups.push_back(HitPopup(ModeMania::TRACK_X + ModeMania::COL_WIDTH * 2.0f, ModeMania::HIT_Y - 20.0f, HIT_MISS, 24.0f, clrMiss, 24));
                    if (isSuddenDeath || isPerfect) {
                        triggerSuddenDeathFail();
                    }
                }

                // 2. Procesar inputs en las 4 columnas
                bool colDown[4] = { false, false, false, false };
                bool colUp[4]   = { false, false, false, false };

                if (isAutoplay) {
                    for (size_t i = maniaEngine.nextNoteIndex; i < maniaEngine.notes.size(); ++i) {
                        auto& mn = maniaEngine.notes[i];
                        if (mn.fullyCompleted) continue;
                        if (songTime >= mn.time && !mn.headJudged) {
                            colDown[mn.column] = true;
                        }
                        if (mn.isHold && mn.isHolding) {
                            maniaEngine.colPressed[mn.column] = true;
                            if (songTime >= mn.endTime) {
                                colUp[mn.column] = true;
                            }
                        }
                    }
                } else {
                    // Controles físicos por columna
                    if (kDown & settings.keyManiaCol1) colDown[0] = true;
                    if (kDown & settings.keyManiaCol2) colDown[1] = true;
                    if (kDown & settings.keyManiaCol3) colDown[2] = true;
                    if (kDown & settings.keyManiaCol4) colDown[3] = true;

                    if (kUp & settings.keyManiaCol1) colUp[0] = true;
                    if (kUp & settings.keyManiaCol2) colUp[1] = true;
                    if (kUp & settings.keyManiaCol3) colUp[2] = true;
                    if (kUp & settings.keyManiaCol4) colUp[3] = true;

                    // Ajuste de Scroll Speed al vuelo en Mania
                    if (kDown & (KEY_ZL | KEY_CPAD_DOWN)) {
                        maniaEngine.adjustScrollSpeed(-0.1f);
                        audio.playHitSound(0);
                    } else if (kDown & (KEY_ZR | KEY_CPAD_UP)) {
                        maniaEngine.adjustScrollSpeed(+0.1f);
                        audio.playHitSound(0);
                    } else if (kHeld & KEY_SELECT) {
                        if (kDown & KEY_DDOWN) {
                            maniaEngine.adjustScrollSpeed(-0.1f);
                            audio.playHitSound(0);
                        } else if (kDown & KEY_DUP) {
                            maniaEngine.adjustScrollSpeed(+0.1f);
                            audio.playHitSound(0);
                        }
                    }

                    // Entrada táctil en el teclado de la pantalla inferior
                    if (isTouching) {
                        int tCol = maniaEngine.handleTouch(touch.px, touch.py, true);
                        if (tCol >= 0 && tCol <= 3) {
                            if (touchJustDown || maniaEngine.lastTouchCol != tCol) {
                                colDown[tCol] = true;
                            }
                        }
                    }
                    if (touchJustUp && maniaEngine.lastTouchCol >= 0) {
                        colUp[maniaEngine.lastTouchCol] = true;
                        maniaEngine.handleTouch(0.0f, 0.0f, false);
                    }
                }

                // 3. Procesar liberaciones (Key Releases) para colas de Hold Notes
                for (int c = 0; c < 4; ++c) {
                    if (colUp[c]) {
                        int relRes = maniaEngine.processKeyRelease(c, songTime, hit300, hit100, hit50);
                        float popX = ModeMania::TRACK_X + (c + 0.5f) * ModeMania::COL_WIDTH;
                        if (relRes == 300) {
                            count300++;
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                            hp = std::min(1.0f, hp + 0.04f);
                            audio.playHitSound(c % 2);
                            if (settings.showHit300) popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_300, 22.0f, clr300, 22));
                        } else if (relRes == 100) {
                            count100++;
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>((100 + 100 * combo * 0.05f) * scoreMult);
                            hp = std::min(1.0f, hp + 0.02f);
                            audio.playHitSound(c % 2);
                            popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_100, 20.0f, clr100, 22));
                            if (isPerfect) triggerSuddenDeathFail();
                        } else if (relRes == -1) {
                            countMiss++;
                            combo = 0;
                            hp = std::max(0.0f, hp - 0.08f);
                            triggerShake(3.0f, 10.0f);
                            audio.playHitSound(c % 2);
                            popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_MISS, 22.0f, clrMiss, 24));
                            if (isSuddenDeath || isPerfect) triggerSuddenDeathFail();
                        }
                    }
                }

                // 4. Procesar pulsaciones (Key Presses)
                for (int c = 0; c < 4; ++c) {
                    if (colDown[c]) {
                        k1PressCount++;
                        int hitRes = maniaEngine.processKeyPress(c, songTime, hit300, hit100, hit50);
                        float popX = ModeMania::TRACK_X + (c + 0.5f) * ModeMania::COL_WIDTH;
                        u32 pClr = (c == 0 || c == 3) ? ModeMania::CLR_COL_OUTER : ModeMania::CLR_COL_INNER;

                        if (hitRes == 300) {
                            count300++;
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                            hp = std::min(1.0f, hp + 0.04f);
                            audio.playHitSound(c % 2);
                            if (settings.showHit300) {
                                popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_300, 24.0f, clr300, 24));
                                spawnParticles(popX, ModeMania::HIT_Y, pClr, 6, 0.8f, 2.2f, 2.0f, 15);
                            }
                        } else if (hitRes == 100) {
                            count100++;
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>((100 + 100 * combo * 0.05f) * scoreMult);
                            hp = std::min(1.0f, hp + 0.02f);
                            audio.playHitSound(c % 2);
                            popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_100, 20.0f, clr100, 24));
                            spawnParticles(popX, ModeMania::HIT_Y, clr100, 4, 0.6f, 1.8f, 1.8f, 12);
                            if (isPerfect) triggerSuddenDeathFail();
                        } else if (hitRes == 50) {
                            count50++;
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>((50 + 50 * combo * 0.05f) * scoreMult);
                            hp = std::min(1.0f, hp + 0.01f);
                            audio.playHitSound(c % 2);
                            popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_50, 18.0f, clr50, 22));
                            if (isPerfect) triggerSuddenDeathFail();
                        } else {
                            audio.playHitSound(c % 2);
                        }
                    }
                }

                // 5. Actualizar notas largas sostenidas y ticks
                int holdTicks = 0;
                int completeLN = maniaEngine.updateActiveHolds(songTime, holdTicks);
                if (holdTicks > 0) {
                    combo += holdTicks;
                    if (combo > maxCombo) maxCombo = combo;
                    score += static_cast<int>(10 * holdTicks * scoreMult);
                    hp = std::min(1.0f, hp + 0.005f * holdTicks);
                }
                if (completeLN == 300) {
                    count300++;
                    combo++;
                    if (combo > maxCombo) maxCombo = combo;
                    score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                    hp = std::min(1.0f, hp + 0.04f);
                    if (settings.showHit300) popups.push_back(HitPopup(ModeMania::TRACK_X + ModeMania::COL_WIDTH * 2.0f, ModeMania::HIT_Y - 20.0f, HIT_300, 24.0f, clr300, 24));
                }

                allObjectsDone = (maniaEngine.nextNoteIndex >= maniaEngine.notes.size());
            } else if (isCatchPlay) {
                // 1. Controles y Movimiento del Catcher
                float moveInput = 0.0f;
                bool dashInput = false;
                float analogX = 0.0f;

                if (isAutoplay) {
                    // Autoplay de Catch: se desplaza automáticamente hacia la próxima fruta activa
                    for (size_t i = catchEngine.nextFruitIndex; i < catchEngine.fruits.size(); ++i) {
                        const auto& f = catchEngine.fruits[i];
                        if (f.state != NOT_HIT) continue;
                        float diff = f.x - catchEngine.catcherX;
                        if (std::abs(diff) > 2.0f) {
                            moveInput = (diff > 0.0f) ? 1.0f : -1.0f;
                            if (std::abs(diff) > 40.0f) dashInput = true;
                        } else {
                            catchEngine.catcherX = f.x;
                        }
                        break;
                    }
                    catchEngine.update(16.0f, moveInput, dashInput, 0.0f, songTime, speedRate);
                } else {
                    if (kHeld & settings.keyCatchLeft)  moveInput -= 1.0f;
                    if (kHeld & settings.keyCatchRight) moveInput += 1.0f;
                    if (kHeld & settings.keyCatchDash)  dashInput = true;

                    // Soporte de palanca analógica Circle Pad para desplazamiento suave
                    if (std::abs(cpad.dx) > 20) {
                        analogX = static_cast<float>(cpad.dx) / 150.0f;
                        if (std::abs(cpad.dx) > 130) dashInput = true;
                    }

                    // Soporte táctil en pantalla inferior
                    if (isTouching) {
                        catchEngine.handleTouch(touch.px, touch.py, true);
                    } else if (touchJustUp) {
                        catchEngine.handleTouch(0.0f, 0.0f, false);
                    }

                    catchEngine.update(16.0f, moveInput, dashInput, analogX, songTime, speedRate);
                }

                // 2. Comprobar capturas y fallos de frutas
                int scoreAdd = 0;
                int comboAdd = 0;
                float hpAdd = 0.0f;
                int hitResult = catchEngine.checkCatches(songTime, hit50, scoreAdd, comboAdd, hpAdd);

                if (hitResult > 0) {
                    if (hitResult == 300 || hitResult == 1100) count300++;
                    else if (hitResult == 100) count100++;
                    else if (hitResult == 10) count50++;

                    combo += comboAdd;
                    if (combo > maxCombo) maxCombo = combo;
                    score += static_cast<int>((scoreAdd + (scoreAdd * combo * 0.035f)) * scoreMult);
                    hp = std::max(0.0f, std::min(1.0f, hp + hpAdd));

                    // Sonido y feedback de captura
                    audio.playHitSound((hitResult == 1100) ? 2 : (hitResult == 100 ? 1 : 0));
                    float fruitScrX = catchEngine.toScreenX(catchEngine.catcherX);
                    if (hitResult == 300) {
                        if (settings.showHit300) spawnParticles(fruitScrX, ModeCatch::PLATE_Y, Ui::rgba(255, 100, 140), 5, 0.8f, 2.2f, 2.0f, 14);
                    } else if (hitResult == 1100) {
                        if (settings.showHit300) {
                            spawnParticles(fruitScrX, ModeCatch::PLATE_Y, ModeCatch::CLR_BANANA, 8, 1.0f, 2.8f, 2.2f, 18);
                            popups.push_back(HitPopup(fruitScrX, ModeCatch::PLATE_Y - 25.0f, HIT_300, 24.0f, ModeCatch::CLR_BANANA, 24));
                        }
                    }
                } else if (hitResult == -1) {
                    // ¡MISS de fruta grande!
                    countMiss++;
                    combo = 0;
                    hp = std::max(0.0f, hp + hpAdd);
                    triggerShake(3.5f, 12.0f);
                    float fruitScrX = catchEngine.toScreenX(catchEngine.catcherX);
                    popups.push_back(HitPopup(fruitScrX, ModeCatch::PLATE_Y - 20.0f, HIT_MISS, 24.0f, clrMiss, 24));
                    if (isSuddenDeath || isPerfect) {
                        triggerSuddenDeathFail();
                    }
                } else if (hpAdd != 0.0f) {
                    hp = std::max(0.0f, std::min(1.0f, hp + hpAdd));
                }

                allObjectsDone = (catchEngine.nextFruitIndex >= catchEngine.fruits.size());
            } else {
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

                // 1. Evaluar Spinners activos (con RPM, Bonus +1000, CLEAR! y soporte Stylus / Circle Pad / C-Stick / SpunOut)
                if (obj.isSpinner && !obj.sliderDone && songTime >= obj.time) {
                    anyActiveSpinner = true;
                    float instantRPM = 0.0f;
                    bool stickSpin = (!isTouching && (std::abs(cpad.dx) > 28 || std::abs(cpad.dy) > 28 ||
                                                      std::abs(cstick.dx) > 25 || std::abs(cstick.dy) > 25));
                    if (isSpunOut && !isAutoplay) {
                        float dAng = 0.092f;
                        obj.spinnerProgress += dAng;
                        obj.spinnerRotAngle += dAng;
                        instantRPM = 286.0f;
                        cursorOsuX = 256.0f + std::cos(obj.spinnerRotAngle) * 52.0f;
                        cursorOsuY = 192.0f + std::sin(obj.spinnerRotAngle) * 52.0f;
                        if (animTick % 2 == 0) {
                            float spkR = 56.0f;
                            float sx = 256.0f + std::cos(obj.spinnerRotAngle) * spkR;
                            float sy = 192.0f + std::sin(obj.spinnerRotAngle) * spkR;
                            spawnParticles(sx, sy, obj.spinnerCleared ? clrCursor : clr300, 1, 0.6f, 1.8f, 2.1f, 12);
                        }
                    } else if (((anyHeld || !settings.tapWithStylus) && isTouching) || stickSpin || isAutoplay) {
                        float ang = 0.0f;
                        if (stickSpin && !isAutoplay) {
                            float sx = (std::abs(cstick.dx) > 25 || std::abs(cstick.dy) > 25) ? static_cast<float>(cstick.dx) : static_cast<float>(cpad.dx);
                            float sy = (std::abs(cstick.dx) > 25 || std::abs(cstick.dy) > 25) ? static_cast<float>(-cstick.dy) : static_cast<float>(-cpad.dy);
                            ang = std::atan2(sy, sx);
                            cursorOsuX = 256.0f + std::cos(ang) * 52.0f;
                            cursorOsuY = 192.0f + std::sin(ang) * 52.0f;
                        } else {
                            ang = std::atan2(cursorOsuY - 192.0f, cursorOsuX - 256.0f);
                        }
                        if (obj.spinnerAngleInit) {
                            float dAng = ang - obj.lastSpinnerAngle;
                            while (dAng >  static_cast<float>(M_PI)) dAng -= 2.0f * static_cast<float>(M_PI);
                            while (dAng < -static_cast<float>(M_PI)) dAng += 2.0f * static_cast<float>(M_PI);
                            float absDAng = std::fabs(dAng);
                            obj.spinnerProgress += absDAng;
                            obj.spinnerRotAngle += dAng;
                            instantRPM = std::min(480.0f, (absDAng / (2.0f * static_cast<float>(M_PI))) * 3600.0f);

                            if (absDAng > 0.05f && (animTick % 2 == 0)) {
                                float spkR = 56.0f;
                                float sx = 256.0f + std::cos(obj.spinnerRotAngle) * spkR;
                                float sy = 192.0f + std::sin(obj.spinnerRotAngle) * spkR;
                                spawnParticles(sx, sy, obj.spinnerCleared ? clrCursor : clr300, 1, 0.6f, 1.8f, 2.1f, 12);
                            }
                        }
                        obj.lastSpinnerAngle = ang;
                        obj.spinnerAngleInit = true;
                    } else {
                        obj.spinnerAngleInit = false;
                    }
                    spinnerRPM = spinnerRPM * 0.84f + instantRPM * 0.16f;

                    float clearTarget = std::max(3.14f, obj.spinnerRequiredRads * 0.65f);
                    if (obj.spinnerProgress >= clearTarget || isAutoplay) {
                        if (!obj.spinnerCleared) {
                            obj.spinnerCleared = true;
                            spinnerClearAnim = 1.0f;
                            audio.playHitSound(2);
                            spawnParticles(256.0f, 192.0f, clrCursor, 12, 1.2f, 3.2f, 2.6f, 18);
                        }
                        int extraSpins = static_cast<int>((obj.spinnerProgress - clearTarget) / static_cast<float>(M_PI));
                        while (obj.spinnerBonusCount < extraSpins) {
                            obj.spinnerBonusCount++;
                            score += static_cast<int>(1000 * scoreMult);
                            hp = std::min(1.0f, hp + 0.02f);
                            lastSpinnerBonus = obj.spinnerBonusCount * 1000;
                            spinnerBonusAnim = 1.0f;
                            audio.playHitSound(2);
                            spawnParticles(256.0f, 192.0f, clr300, 6, 1.0f, 2.6f, 2.2f, 14);
                        }
                    }

                    if (songTime >= obj.endTime) {
                        obj.sliderDone = true;
                        if (obj.spinnerProgress >= clearTarget || isAutoplay) {
                            obj.state = HIT_300;
                            audio.playHitSound(obj.hitSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>((300 + 300 * combo * 0.05f) * scoreMult);
                            count300++;
                            hp = std::min(1.0f, hp + 0.08f);
                            judgementHistory.push_back({obj.endTime, HIT_300, 0.0f});
                        } else if (obj.spinnerProgress >= obj.spinnerRequiredRads * 0.35f) {
                            obj.state = HIT_100;
                            audio.playHitSound(obj.hitSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            score += static_cast<int>(100 * scoreMult);
                            count100++;
                            hp = std::min(1.0f, hp + 0.04f);
                            judgementHistory.push_back({obj.endTime, HIT_100, 0.0f});
                            if (isPerfect) {
                                triggerSuddenDeathFail();
                                break;
                            }
                        } else {
                            obj.state = HIT_MISS;
                            combo = 0;
                            countMiss++;
                            hp = std::max(0.0f, hp - 0.12f);
                            triggerShake(3.2f, 11.0f);
                            judgementHistory.push_back({obj.endTime, HIT_MISS, 0.0f});
                            if (isSuddenDeath || isPerfect) {
                                triggerSuddenDeathFail();
                                break;
                            }
                        }
                        if (obj.state != HIT_300 || settings.showHit300) {
                            popups.push_back(HitPopup(256.0f, 192.0f, obj.state, hitRadius * SCALE_FACTOR, objBurstClr, 20));
                        }
                    }
                    continue;
                }

                // 2. Evaluar seguimiento, Slider Ticks, rebotes y finalización de Sliders activos
                if (obj.isSlider && !obj.sliderDone && songTime >= obj.time) {
                    Vec2 ballPos = obj.getSliderBallPos(songTime);
                    float dx = cursorOsuX - ballPos.x;
                    float dy = cursorOsuY - ballPos.y;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    obj.sliderTracking = isAutoplay || (anyHeld && dist <= hitRadius * 2.4f) || (isRelax && dist <= hitRadius * 2.4f);

                    if (obj.sliderTracking && (animTick % 2 == 0)) {
                        u32 spkClr = (animTick % 4 == 0) ? clrCursor : objBurstClr;
                        spawnParticles(ballPos.x, ballPos.y, spkClr, 1, 0.4f, 1.5f, 2.0f, 12);
                    }

                    // Evaluar Slider Ticks intermedios
                    for (auto& tk : obj.sliderTicks) {
                        if (!tk.hit && songTime >= tk.time) {
                            tk.hit = true;
                            if (obj.sliderTracking && obj.state != HIT_MISS) {
                                combo++;
                                if (combo > maxCombo) maxCombo = combo;
                                score += static_cast<int>(10 * scoreMult);
                                hp = std::min(1.0f, hp + 0.015f);
                                spawnParticles(tk.x, tk.y, clr300, 5, 0.7f, 2.0f, 2.2f, 13);
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
                                spawnParticles(ballPos.x, ballPos.y, clrCursor, 7, 0.8f, 2.3f, 2.3f, 14);
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
                            judgementHistory.push_back({obj.endTime, HIT_300, 0.0f});
                            if (settings.showHit300) {
                                popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_300, hitRadius * SCALE_FACTOR, objBurstClr, 18));
                                spawnParticles(ballPos.x, ballPos.y, objBurstClr, 7, 0.8f, 2.3f, 2.3f, 15);
                            }
                        } else if (obj.state == NOT_HIT) {
                            obj.state = HIT_MISS;
                            combo = 0;
                            countMiss++;
                            hp = std::max(0.0f, hp - 0.10f);
                            judgementHistory.push_back({obj.endTime, HIT_MISS, 0.0f});
                            popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_MISS, hitRadius * SCALE_FACTOR, clrMiss, 22));
                            triggerShake(2.8f, 9.0f);
                            if (isSuddenDeath || isPerfect) {
                                triggerSuddenDeathFail();
                                break;
                            }
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
                    judgementHistory.push_back({obj.time, HIT_MISS, 0.0f});
                    popups.push_back(HitPopup(obj.x, obj.y, HIT_MISS, hitRadius * SCALE_FACTOR, clrMiss, 22));
                    triggerShake(2.8f, 9.0f);
                    if (isSuddenDeath || isPerfect) {
                        triggerSuddenDeathFail();
                        break;
                    }
                    continue;
                }

                // 4. Evaluar golpe (Tap) en la nota (con soporte para Mod Relax)
                if (isRelax && !isAutoplay && !kActionDown) {
                    float dx = cursorOsuX - obj.x;
                    float dy = cursorOsuY - obj.y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    int timeDelta = std::abs(songTime - obj.time);
                    if (dist <= hitRadius * 1.15f && timeDelta <= hit100) {
                        kActionDown = true;
                    }
                }

                if (kActionDown) {
                    float signedErrMs = isAutoplay ? 0.0f : static_cast<float>(songTime - obj.time);
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
                                if (isPerfect) {
                                    triggerSuddenDeathFail();
                                    break;
                                }
                            } else {
                                obj.state = HIT_50;
                                score += static_cast<int>((50 + 50 * combo * 0.05f) * scoreMult);
                                count50++;
                                if (isPerfect) {
                                    triggerSuddenDeathFail();
                                    break;
                                }
                            }
                            int headSound = obj.isSlider ? obj.getEdgeHitSound(0) : obj.hitSound;
                            audio.playHitSound(headSound);
                            combo++;
                            if (combo > maxCombo) maxCombo = combo;
                            if (obj.isSlider) obj.sliderTracking = true;
                            judgementHistory.push_back({obj.time, obj.state, signedErrMs});
                            if (obj.state != HIT_300 || settings.showHit300) {
                                popups.push_back(HitPopup(obj.x, obj.y, obj.state, hitRadius * SCALE_FACTOR, objBurstClr, 18));
                                spawnParticles(obj.x, obj.y, (obj.state == HIT_300) ? objBurstClr : clr100,
                                               (obj.state == HIT_300) ? 7 : 4, 0.8f, 2.4f, 2.3f, 15);
                            }

                            // Registrar marca en el Hit Error Meter y estadisticas de UR
                            hitErrorTicks.push_back({signedErrMs, obj.state, 210});
                            if (hitErrorTicks.size() > 28) hitErrorTicks.erase(hitErrorTicks.begin());
                            allHitErrors.push_back(signedErrMs);
                            hitErrorNeedle = hitErrorNeedle * 0.78f + signedErrMs * 0.22f;

                            if (isAutoplay) k1PressCount++;
                            kActionDown = false;
                            break;
                        }
                    }
                }
            }
        }

            if (appState == STATE_RESULTS) continue;

            if (!anyActiveSpinner) {
                spinnerRPM = Ui::approach(spinnerRPM, 0.0f, 0.25f);
                lastSpinnerBonus = 0;
            }
            spinnerClearAnim = Ui::approach(spinnerClearAnim, 0.0f, 0.08f);
            spinnerBonusAnim = Ui::approach(spinnerBonusAnim, 0.0f, 0.07f);

            // Actualizar particulas de Sliders / Hits
            for (size_t p = 0; p < particles.size(); ) {
                particles[p].x += particles[p].vx;
                particles[p].y += particles[p].vy;
                particles[p].vy += 0.035f;
                particles[p].life--;
                if (particles[p].life <= 0) particles.erase(particles.begin() + p);
                else ++p;
            }

            // Envejecer marcas del Hit Error Meter
            for (size_t t = 0; t < hitErrorTicks.size(); ) {
                hitErrorTicks[t].life--;
                if (hitErrorTicks[t].life <= 0) hitErrorTicks.erase(hitErrorTicks.begin() + t);
                else ++t;
            }

            // Envejecer popups de calificacion (300 / 100 / 50 / X) una vez por frame logico
            for (size_t i = 0; i < popups.size(); ) {
                popups[i].timer--;
                if (popups[i].timer <= 0) popups.erase(popups.begin() + i);
                else ++i;
            }

            // Animaciones y sonido de Combo Pop, Combo Break, Combo Burst y Screen Shake
            if (combo > prevCombo) {
                comboScale = 1.45f;
                checkComboBurst(combo);
            } else if (combo < prevCombo) {
                lastComboBurstMilestone = 0;
                if (prevCombo >= 5) {
                    comboBreakAnim = 1.0f;
                    lastBrokenCombo = prevCombo;
                    audio.playComboBreak();
                    triggerShake(4.2f, 14.0f);
                }
            }
            prevCombo = combo;
            comboScale = Ui::approach(comboScale, 1.0f, 0.18f);
            comboBreakAnim = Ui::approach(comboBreakAnim, 0.0f, 0.06f);
            if (comboBurstTimer > 0) comboBurstTimer--;

            if (shakeTimer > 0.0f) {
                shakeTimer = std::max(0.0f, shakeTimer - 1.0f);
                float damp = shakeTimer / 14.0f;
                shakeX = std::sin(animTick * 2.3f) * shakeIntensity * damp;
                shakeY = std::cos(animTick * 2.9f) * shakeIntensity * damp;
            } else {
                shakeX = 0.0f;
                shakeY = 0.0f;
                shakeIntensity = 0.0f;
            }

            int songEndTime = isTaikoPlay ? (!taikoEngine.notes.empty() ? taikoEngine.notes.back().endTime : 0)
                            : isCatchPlay ? (!catchEngine.fruits.empty() ? catchEngine.fruits.back().time : 0)
                            : isManiaPlay ? (!maniaEngine.notes.empty() ? maniaEngine.notes.back().endTime : 0)
                            : (!beatmap.circles.empty() ? beatmap.circles.back().endTime : 0);
            if ((allObjectsDone && songEndTime > 0 && songTime > songEndTime + 1000) ||
                audio.isSongFinished()) {
                audio.stopMusic();
                videoPlayer.closeVideo();

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

                if (!allHitErrors.empty()) {
                    float sum = 0.0f;
                    for (float e : allHitErrors) sum += e;
                    lastMeanHitError = sum / static_cast<float>(allHitErrors.size());
                    float varSum = 0.0f;
                    for (float e : allHitErrors) {
                        float d = e - lastMeanHitError;
                        varSum += d * d;
                    }
                    lastUnstableRate = std::sqrt(varSum / static_cast<float>(allHitErrors.size())) * 10.0f;
                } else {
                    lastMeanHitError = 0.0f;
                    lastUnstableRate = 0.0f;
                }

                lastEarnedPP = calculatePP(
                    beatmap.starRating,
                    beatmap.diff.ar,
                    beatmap.diff.od,
                    maxCombo,
                    beatmap.maxPossibleCombo,
                    count300, count100, count50, countMiss,
                    settings.activeMods
                );

                int currentPlayMode = isTaikoPlay ? 1 : isCatchPlay ? 2 : isManiaPlay ? 3 : 0;
                newRecordAchieved = scoreMgr.submitScore(
                    beatmap.filePath, score, maxCombo, acc, grade, settings.activeMods, lastEarnedPP, currentPlayMode
                );
                if (multiMgr.inMultiplayer) {
                    multiMgr.matchStarted = false;
                    multiMgr.matchInProgressFlag = false;
                    multiMgr.matchFinished = true;
                    multiMgr.sendLiveGameplay(score, combo, maxCombo, acc, hp, grade, count300, count100, count50, countMiss, true, stageFailed);
                    appState = STATE_MULTI_RESULTS;
                } else {
                    appState = STATE_RESULTS;
                }
                continue;
            }
        }

        // =========================================================
        // RENDERIZADO DE LA PARTIDA (SOPORTA 3D EN PANTALLA SUPERIOR Y JUEGO TÁCTIL 1:1 EN PANTALLA INFERIOR)
        // =========================================================
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        float hitRadius = beatmap.diff.radiusOsu;
        float preempt   = beatmap.diff.preempt;
        float fadeIn    = beatmap.diff.fadeIn;
        float hit50     = beatmap.diff.hit50;
        float rScreen   = hitRadius * SCALE_FACTOR;

        const std::vector<RGBColor>& activePalette =
            !beatmap.customComboColors.empty() ? beatmap.customComboColors : activeSkin.comboColors;

        auto renderPlayfield = [&](float screenW, float baseOffsetX, float baseOffsetY, float eyeSign, float slider3DVal, bool drawFullHud) {
            auto depthShift = [&](float px) { return eyeSign * slider3DVal * px; };

            float dBg        = depthShift(-4.0f);
            float dPlayfield = depthShift(0.4f);
            float dSlider    = depthShift(2.6f);
            float dCircle    = depthShift(2.6f);
            float dFrontHud  = depthShift(3.6f);
            float dCursor    = depthShift(4.6f);

            auto playX = [&](float osuX, float dOffset = 0.0f) { return baseOffsetX + (osuX * SCALE_FACTOR) + shakeX + dOffset; };
            auto playY = [&](float osuY) { return baseOffsetY + (osuY * SCALE_FACTOR) + shakeY; };

            bool renderVideoNow = settings.enableVideo && videoPlayer.shouldRender();
            if (isTaikoPlay && settings.taikoBgMode == 1) {
                renderVideoNow = false; // Forzar imagen en vez de video
            }

            if (renderVideoNow) {
                videoPlayer.drawRect(shakeX * 0.5f + dBg, shakeY * 0.5f, screenW, 240.0f, 0.01f);
                u8 dimAlpha = static_cast<u8>((settings.bgDim * 255) / 100);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.02f, screenW, 240.0f, C2D_Color32(10, 8, 18, dimAlpha));
            } else if (bgTex.loaded) {
                bgTex.drawRect(shakeX * 0.5f + dBg, shakeY * 0.5f, screenW, 240.0f, 0.01f, 255);
                u8 dimAlpha = static_cast<u8>((settings.bgDim * 255) / 100);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.02f, screenW, 240.0f, C2D_Color32(10, 8, 18, dimAlpha));
            } else {
                Ui::backdrop(screenW, 240.0f, static_cast<float>(animTick), Ui::rgba(24, 16, 42), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);
                C2D_DrawRectSolid(baseOffsetX + shakeX + dPlayfield, baseOffsetY + shakeY, 0.02f, PLAYFIELD_W, PLAYFIELD_H, Ui::withAlpha(clrPlayfield, 210));
            }
            // Marco sutil del playfield 4:3 en zona segura (Safe Area - solo en modo Standard)
            if (settings.showPlayfieldBorder && !isTaikoPlay && !isManiaPlay && !isCatchPlay) {
                drawRectOutline(baseOffsetX + shakeX + dPlayfield, baseOffsetY + shakeY, PLAYFIELD_W, PLAYFIELD_H, 1.0f, Ui::rgba(255, 255, 255, 38), 0.03f);
            }

            // Efectos de Kiai Time: Resplandor rítmico y viñetas de energía
            float bpmPulse = getBpmPulse();
            if (kiaiGlow > 0.01f) {
                u8 glowAlpha = static_cast<u8>(kiaiGlow * (45.0f + 110.0f * bpmPulse));
                if (!isTaikoPlay && !isManiaPlay && !isCatchPlay) {
                    u32 kiaiBorderClr = Ui::rgba(255, 195, 75, glowAlpha);
                    drawRectOutline(baseOffsetX + shakeX + dPlayfield - 1.0f, baseOffsetY + shakeY - 1.0f, PLAYFIELD_W + 2.0f, PLAYFIELD_H + 2.0f, 2.5f, kiaiBorderClr, 0.04f);
                }

                float edgeH = (6.0f + 6.0f * bpmPulse) * kiaiGlow;
                u8 vAlpha = static_cast<u8>(kiaiGlow * (35.0f + 45.0f * bpmPulse));
                Ui::gradV(0.0f, 0.0f, screenW, edgeH, 0.04f, Ui::rgba(255, 120, 180, vAlpha), Ui::rgba(255, 120, 180, 0));
                Ui::gradV(0.0f, 240.0f - edgeH, screenW, edgeH, 0.04f, Ui::rgba(255, 120, 180, 0), Ui::rgba(255, 120, 180, vAlpha));
            }

            if (isTaikoPlay) {
                int tJudged = count300 + count100 + count50 + countMiss;
                float liveAcc = (tJudged > 0)
                    ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (tJudged * 300.0f)) * 100.0f
                    : 100.0f;
                taikoEngine.renderTopScreen(textBuf, songTime, speedRate, eyeSign, slider3DVal, inKiai, bpmPulse, combo, score, hp, liveAcc, beatmap.title.c_str(), beatmap.version.c_str(), settings.taikoBgMode);
            } else if (isCatchPlay) {
                catchEngine.renderTopScreen(textBuf, songTime, speedRate, eyeSign, slider3DVal, inKiai);
            } else if (isManiaPlay) {
                maniaEngine.renderTopScreen(textBuf, songTime, speedRate, eyeSign, slider3DVal, inKiai, bpmPulse);
            } else {
                // 0. Follow Points entre notas consecutivas del mismo combo
                for (size_t i = 0; i + 1 < beatmap.circles.size(); ++i) {
                const auto& a = beatmap.circles[i];
                const auto& b = beatmap.circles[i + 1];
                if (a.isSpinner || b.isSpinner || b.comboNumber == 1) continue;
                if (songTime < b.time - preempt || songTime > b.time) continue;

                Vec2 startPos = a.getEndPosition();
                float x0 = playX(startPos.x, dPlayfield);
                float y0 = playY(startPos.y);
                float x1 = playX(b.x, dPlayfield);
                float y1 = playY(b.y);
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

            std::vector<int> visibleIndices;
            visibleIndices.reserve(24);
            for (int i = 0; i < static_cast<int>(beatmap.circles.size()); ++i) {
                const auto& obj = beatmap.circles[i];
                bool visible = false;
                if (obj.isSpinner) {
                    visible = (songTime >= (obj.time - 400) && !obj.sliderDone);
                } else if (obj.isSlider) {
                    visible = (songTime >= (obj.time - preempt) && !obj.sliderDone);
                } else {
                    visible = (songTime >= (obj.time - preempt) && obj.state == NOT_HIT);
                }
                if (visible) {
                    visibleIndices.push_back(i);
                }
            }

            int numVisible = static_cast<int>(visibleIndices.size());
            float stepZ = 0.20f / static_cast<float>(std::max(1, numVisible));
            float subStep = stepZ * 0.095f;

            for (int v = numVisible - 1; v >= 0; --v) {
                int i = visibleIndices[v];
                const auto& obj = beatmap.circles[i];
                float objBaseZ = 0.065f + static_cast<float>(numVisible - 1 - v) * stepZ;

                // Calcular opacidad (Fade-In normal y Fade-Out en modo Hidden HD)
                float alphaMult = 1.0f;
                float timeSinceAppear = static_cast<float>(songTime - (obj.time - preempt));
                if (timeSinceAppear < fadeIn && fadeIn > 1.0f) {
                    alphaMult = std::max(0.0f, std::min(1.0f, timeSinceAppear / fadeIn));
                }
                if (isHidden && !obj.isSpinner) {
                    float fadeOutStart = preempt * 0.40f;
                    float fadeOutEnd   = preempt * 0.70f; // Se desvanece totalmente cuando falta 30% del preempt
                    if (timeSinceAppear > fadeOutStart) {
                        float t = (timeSinceAppear - fadeOutStart) / std::max(1.0f, fadeOutEnd - fadeOutStart);
                        alphaMult = std::max(0.0f, 1.0f - t);
                    }
                    if (timeSinceAppear >= fadeOutEnd) {
                        alphaMult = 0.0f;
                    }
                }

                const auto& rgb = activePalette[obj.colorIndex % activePalette.size()];
                u8 cAlpha = static_cast<u8>(235.0f * alphaMult);
                u8 sAlpha = static_cast<u8>(165.0f * alphaMult);
                u32 cColor = C2D_Color32(rgb.r, rgb.g, rgb.b, cAlpha);
                u32 sColor = activeSkin.hasSliderTrackOverride
                    ? C2D_Color32(activeSkin.sliderTrackColor.r, activeSkin.sliderTrackColor.g, activeSkin.sliderTrackColor.b, sAlpha)
                    : C2D_Color32(static_cast<u8>(rgb.r * 0.62f), static_cast<u8>(rgb.g * 0.62f), static_cast<u8>(rgb.b * 0.62f), sAlpha);
                u32 ringClr = C2D_Color32(activeSkin.sliderBorderColor.r, activeSkin.sliderBorderColor.g, activeSkin.sliderBorderColor.b, cAlpha);

                if (obj.isSpinner) {
                    float spX = playX(256.0f, dCircle);
                    float spY = playY(192.0f);
                    float clearTarget = std::max(3.14f, obj.spinnerRequiredRads * 0.65f);
                    float fillRatio = std::min(1.0f, obj.spinnerProgress / clearTarget);
                    float spDur = std::max(1.0f, static_cast<float>(obj.endTime - obj.time));
                    float timeRatio = std::max(0.0f, std::min(1.0f, static_cast<float>(songTime - obj.time) / spDur));

                    // Atenuacion suave del fondo para destacar el Spinner
                    C2D_DrawRectSolid(baseOffsetX + shakeX + dPlayfield, baseOffsetY + shakeY, 0.025f, PLAYFIELD_W, PLAYFIELD_H, Ui::rgba(8, 6, 18, 95));

                    u32 spinClr = obj.spinnerCleared ? clrCursor : ((fillRatio > 0.6f) ? clr100 : clr300);

                    // Anillo exterior de tiempo en 3D (converge desde el frente hacia el disco al cerrarse)
                    float approachR = 14.0f + 74.0f * (1.0f - timeRatio);
                    float dSpApp = depthShift(2.6f + (1.0f - timeRatio) * 3.0f);
                    drawRingZ(playX(256.0f, dSpApp), spY, 0.22f, approachR, 2.2f, Ui::withAlpha(clrWhite, 190), 32);

                    // Disco de turbina central con aspas giratorias
                    const float discR = 46.0f;
                    Ui::glow(spX, spY, 0.18f, discR * (1.25f + fillRatio * 0.45f), Ui::withAlpha(spinClr, static_cast<u8>(70 + fillRatio * 90)), 5);
                    C2D_DrawCircleSolid(spX, spY, 0.19f, discR, Ui::rgba(16, 14, 28, 220));
                    drawRingZ(spX, spY, 0.23f, discR, 3.0f, spinClr, 32);
                    drawRingZ(spX, spY, 0.23f, discR * 0.68f, 1.2f, Ui::withAlpha(spinClr, 130), 24);

                    for (int b = 0; b < 6; ++b) {
                        float bAng = obj.spinnerRotAngle + b * (static_cast<float>(M_PI) / 3.0f);
                        float xIn  = spX + std::cos(bAng) * 10.0f;
                        float yIn  = spY + std::sin(bAng) * 10.0f;
                        float xOut = spX + std::cos(bAng) * (discR - 3.0f);
                        float yOut = spY + std::sin(bAng) * (discR - 3.0f);
                        C2D_DrawLine(xIn, yIn, clrWhite, xOut, yOut, spinClr, 2.2f, 0.24f);
                    }
                    C2D_DrawCircleSolid(spX, spY, 0.25f, 9.0f + (spinnerRPM / 480.0f) * 4.0f, spinClr);
                    C2D_DrawCircleSolid(spX, spY, 0.26f, 4.5f, clrWhite);

                    // Barras laterales simetricas de energia (Progreso del Spinner 0% -> 100%)
                    const float barH = 112.0f;
                    const float barW = 9.0f;
                    const float barTopY = spY - barH * 0.5f;
                    float fillH = barH * fillRatio;
                    for (int side = -1; side <= 1; side += 2) {
                        float bx = spX + side * 98.0f - barW * 0.5f;
                        Ui::panel(bx - 2.0f, barTopY - 2.0f, barW + 4.0f, barH + 4.0f, 0.21f, Ui::rgba(14, 12, 24, 230), spinClr);
                        if (fillH > 1.0f) {
                            Ui::gradV(bx, barTopY + (barH - fillH), barW, fillH, 0.24f, Ui::brighten(spinClr, 0.6f), spinClr);
                        }
                        for (int seg = 1; seg < 5; ++seg) {
                            float sy = barTopY + seg * (barH / 5.0f);
                            C2D_DrawLine(bx, sy, Ui::rgba(10, 8, 18, 180), bx + barW, sy, Ui::rgba(10, 8, 18, 180), 1.0f, 0.25f);
                        }
                    }

                    // Texto superior: "GIRA!" o "CLEAR!" con efecto pop en primer plano 3D, y Bonus "+1000"
                    if (obj.spinnerCleared) {
                        float cScale = 0.62f + spinnerClearAnim * 0.25f;
                        drawCenteredText(textBuf, playX(256.0f, dFrontHud), spY - 68.0f, cScale, clrCursor, "CLEAR!", 0.32f);
                    } else if (songTime >= obj.time) {
                        drawCenteredText(textBuf, playX(256.0f, dFrontHud), spY - 66.0f, 0.48f, clrWhite, LOC(STR_GAME_SPIN), 0.32f);
                    }

                    if (lastSpinnerBonus > 0 && spinnerBonusAnim > 0.01f) {
                        char bonStr[32];
                        std::snprintf(bonStr, sizeof(bonStr), "+%d", lastSpinnerBonus);
                        float bScale = 0.52f + spinnerBonusAnim * 0.20f;
                        drawCenteredText(textBuf, playX(256.0f, dFrontHud), spY + 56.0f - (1.0f - spinnerBonusAnim) * 8.0f, bScale, clr300, bonStr, 0.33f);
                    }

                    // Medidor digital de RPM abajo del Spinner
                    char rpmStr[32];
                    std::snprintf(rpmStr, sizeof(rpmStr), "%.0f RPM", spinnerRPM);
                    Ui::pill(spX - 36.0f, spY + 68.0f, 72.0f, 16.0f, 0.28f, Ui::rgba(20, 16, 34, 230));
                    drawCenteredText(textBuf, spX, spY + 76.0f, 0.38f, spinClr, rpmStr, 0.32f);
                    continue;
                }

                // 1. Cuerpo del Slider, Ticks, Flechas de Rebote y Bola
                if (obj.isSlider && obj.sliderPath.size() >= 2) {
                    size_t fullPtsCount = obj.sliderPath.size();
                    float sRadius = rScreen * 0.85f;
                    float sBorder = 2.0f;
                    float rOuter = sRadius + sBorder;
                    float rInner = sRadius;

                    // Cálculo de Snaking In (la curva se va dibujando suavemente como una serpiente al aparecer)
                    float snakeInFrac = 1.0f;
                    if (settings.snakingSliders && songTime < obj.time) {
                        float timeSinceAppear = static_cast<float>(songTime - (obj.time - preempt));
                        float snakeDuration = std::max(100.0f, preempt * 0.66f);
                        float t = std::max(0.0f, std::min(1.0f, timeSinceAppear / snakeDuration));
                        snakeInFrac = std::sin(t * (static_cast<float>(M_PI) * 0.5f));
                        snakeInFrac = std::max(0.04f, snakeInFrac);
                    }

                    // Construir la lista de puntos visibles recortada según snakeInFrac
                    std::vector<Vec2> visiblePath;
                    if (snakeInFrac >= 0.999f || fullPtsCount < 2) {
                        visiblePath = obj.sliderPath;
                    } else {
                        visiblePath.reserve(fullPtsCount + 1);
                        float targetIdx = snakeInFrac * static_cast<float>(fullPtsCount - 1);
                        int count = static_cast<int>(targetIdx);
                        if (count < 1) count = 1;
                        for (int p = 0; p <= count && p < static_cast<int>(fullPtsCount); ++p) {
                            visiblePath.push_back(obj.sliderPath[p]);
                        }
                        float frac = targetIdx - static_cast<float>(count);
                        if (frac > 0.005f && count + 1 < static_cast<int>(fullPtsCount)) {
                            const Vec2& pA = obj.sliderPath[count];
                            const Vec2& pB = obj.sliderPath[count + 1];
                            visiblePath.push_back(Vec2(pA.x + (pB.x - pA.x) * frac, pA.y + (pB.y - pA.y) * frac));
                        }
                        if (visiblePath.size() < 2 && fullPtsCount >= 2) {
                            visiblePath.push_back(obj.sliderPath[1]);
                        }
                    }

                    size_t nPts = visiblePath.size();
                    std::vector<Vec2> pts(nPts);
                    std::vector<Vec2> nrm(nPts);
                    for (size_t p = 0; p < nPts; ++p) {
                        pts[p] = Vec2(playX(visiblePath[p].x, dSlider), playY(visiblePath[p].y));
                    }
                    for (size_t p = 0; p < nPts; ++p) {
                        Vec2 dir;
                        if (p == 0) {
                            dir = Vec2(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
                        } else if (p == nPts - 1) {
                            dir = Vec2(pts[nPts - 1].x - pts[nPts - 2].x, pts[nPts - 1].y - pts[nPts - 2].y);
                        } else {
                            dir = Vec2(pts[p + 1].x - pts[p - 1].x, pts[p + 1].y - pts[p - 1].y);
                        }
                        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len > 1e-4f) {
                            nrm[p] = Vec2(-dir.y / len, dir.x / len);
                        } else {
                            nrm[p] = (p > 0) ? nrm[p - 1] : Vec2(0.0f, 1.0f);
                        }
                    }

                    float zBorder = objBaseZ + subStep * 1.0f;
                    float zBody   = objBaseZ + subStep * 2.0f;

                    // Pase 1: Malla continua de triángulos para el borde exterior (ringClr)
                    for (size_t p = 0; p + 1 < nPts; ++p) {
                        float l0x = pts[p].x + nrm[p].x * rOuter,     l0y = pts[p].y + nrm[p].y * rOuter;
                        float r0x = pts[p].x - nrm[p].x * rOuter,     r0y = pts[p].y - nrm[p].y * rOuter;
                        float l1x = pts[p + 1].x + nrm[p + 1].x * rOuter, l1y = pts[p + 1].y + nrm[p + 1].y * rOuter;
                        float r1x = pts[p + 1].x - nrm[p + 1].x * rOuter, r1y = pts[p + 1].y - nrm[p + 1].y * rOuter;
                        C2D_DrawTriangle(l0x, l0y, ringClr, r0x, r0y, ringClr, l1x, l1y, ringClr, zBorder);
                        C2D_DrawTriangle(r0x, r0y, ringClr, r1x, r1y, ringClr, l1x, l1y, ringClr, zBorder);
                    }

                    // Pase 2: Malla continua de triángulos para el relleno interior (sColor)
                    for (size_t p = 0; p + 1 < nPts; ++p) {
                        float l0x = pts[p].x + nrm[p].x * rInner,     l0y = pts[p].y + nrm[p].y * rInner;
                        float r0x = pts[p].x - nrm[p].x * rInner,     r0y = pts[p].y - nrm[p].y * rInner;
                        float l1x = pts[p + 1].x + nrm[p + 1].x * rInner, l1y = pts[p + 1].y + nrm[p + 1].y * rInner;
                        float r1x = pts[p + 1].x - nrm[p + 1].x * rInner, r1y = pts[p + 1].y - nrm[p + 1].y * rInner;
                        C2D_DrawTriangle(l0x, l0y, sColor, r0x, r0y, sColor, l1x, l1y, sColor, zBody);
                        C2D_DrawTriangle(r0x, r0y, sColor, r1x, r1y, sColor, l1x, l1y, sColor, zBody);
                    }

                    // Extremos redondeados (Caps) en cabeza y en la punta visible de la serpiente
                    C2D_DrawCircleSolid(pts[0].x, pts[0].y, zBorder, rOuter, ringClr);
                    C2D_DrawCircleSolid(pts.back().x, pts.back().y, zBorder, rOuter, ringClr);
                    C2D_DrawCircleSolid(pts[0].x, pts[0].y, zBody, rInner, sColor);
                    C2D_DrawCircleSolid(pts.back().x, pts.back().y, zBody, rInner, sColor);

                    // Anillo de la cola: en el destino final se desvanece suavemente a medida que la serpiente se acerca
                    Vec2 finalTail = obj.sliderPath.back();
                    float tailX = playX(finalTail.x, dSlider);
                    float tailY = playY(finalTail.y);
                    u8 tailAlpha = static_cast<u8>(cAlpha * (snakeInFrac >= 0.95f ? 1.0f : (snakeInFrac * 0.85f)));
                    if (tailAlpha > 5) {
                        drawRing(tailX, tailY, sRadius, 1.5f, Ui::withAlpha(ringClr, tailAlpha), 20, objBaseZ + subStep * 3.0f);
                    }

                    // Dibujar Slider Ticks pendientes (solo aquellos alcanzados por la serpiente)
                    int curSpan = 0;
                    float slideDur = static_cast<float>(std::max(1, obj.endTime - obj.time)) / std::max(1, obj.repeats);
                    if (songTime > obj.time && obj.endTime > obj.time) {
                        curSpan = static_cast<int>((songTime - obj.time) / slideDur);
                    }
                    for (const auto& tk : obj.sliderTicks) {
                        if (!tk.hit && tk.time >= songTime - 50) {
                            float tkProgress = static_cast<float>(tk.time - obj.time) / std::max(1.0f, slideDur);
                            if (songTime >= obj.time || snakeInFrac >= tkProgress) {
                                C2D_DrawCircleSolid(playX(tk.x, dSlider), playY(tk.y), objBaseZ + subStep * 3.0f, 2.2f, Ui::withAlpha(clrWhite, cAlpha));
                            }
                        }
                    }

                    // Flecha de rebote (Reverse Arrow) si quedan repeticiones y la serpiente ya llegó al final
                    if (obj.repeats > 1 && curSpan + 1 < obj.repeats && snakeInFrac >= 0.92f) {
                        bool arrowAtTail = (curSpan % 2 == 0);
                        if (arrowAtTail) {
                            const Vec2& pTip = obj.sliderPath.back();
                            const Vec2& pPrev = obj.sliderPath[obj.sliderPath.size() - 2];
                            drawReverseArrow(
                                playX(pTip.x, dSlider), playY(pTip.y),
                                playX(pPrev.x, dSlider), playY(pPrev.y),
                                rScreen * 0.65f, Ui::withAlpha(clrWhite, cAlpha), objBaseZ + subStep * 4.0f
                            );
                        } else {
                            const Vec2& pTip = obj.sliderPath.front();
                            const Vec2& pNext = obj.sliderPath[1];
                            drawReverseArrow(
                                playX(pTip.x, dSlider), playY(pTip.y),
                                playX(pNext.x, dSlider), playY(pNext.y),
                                rScreen * 0.65f, Ui::withAlpha(clrWhite, cAlpha), objBaseZ + subStep * 4.0f
                            );
                        }
                    }

                    if (songTime >= obj.time && songTime <= obj.endTime) {
                        Vec2 ballOsu = obj.getSliderBallPos(songTime);
                        float bx = playX(ballOsu.x, dCircle);
                        float by = playY(ballOsu.y);
                        Ui::glow(bx, by, objBaseZ + subStep * 8.5f, rScreen * 1.45f, C2D_Color32(rgb.r, rgb.g, rgb.b, 120), 3);
                        C2D_DrawCircleSolid(bx, by, objBaseZ + subStep * 9.0f, rScreen * 0.88f, C2D_Color32(rgb.r, rgb.g, rgb.b, 240));
                        C2D_DrawCircleSolid(bx, by, objBaseZ + subStep * 9.5f, rScreen * 0.35f, clrWhite);
                        float followR = obj.sliderTracking ? (rScreen * 1.9f) : (rScreen * 1.3f);
                        u32 followClr = obj.sliderTracking ? clrCursor : clrWhite;
                        drawRing(bx, by, followR, 1.5f, followClr, 20, objBaseZ + subStep * 9.8f);
                    }
                }

                // 2. Cabeza del círculo y número (con soporte para Sprites PNG de Skin o Vectorial 60 FPS + Profundidad 3D)
                if (obj.state == NOT_HIT && songTime <= obj.time + hit50 && cAlpha > 8) {
                    float cx = playX(obj.x, dCircle);
                    float cy = playY(obj.y);

                    // Resplandor radial suave alrededor de la nota (con boost durante Kiai Time)
                    float kiaiGlowBoost = (kiaiGlow > 0.01f) ? (0.24f * kiaiGlow * bpmPulse) : 0.0f;
                    u8 glowA = static_cast<u8>(std::min(255.0f, (85.0f + 50.0f * kiaiGlow * bpmPulse) * alphaMult));
                    Ui::glow(cx, cy, objBaseZ + subStep * 4.5f, rScreen * (1.42f + kiaiGlowBoost), C2D_Color32(rgb.r, rgb.g, rgb.b, glowA), 3);

                    if (texSkinCircle.loaded) {
                        texSkinCircle.drawCenteredSprite(cx, cy, rScreen * 1.05f, objBaseZ + subStep * 5.0f, cColor);
                        if (texSkinOverlay.loaded) {
                            texSkinOverlay.drawCenteredSprite(cx, cy, rScreen * 1.05f, objBaseZ + subStep * 6.0f, C2D_Color32(255, 255, 255, cAlpha));
                        }
                    } else if (activeSkin.darkCircleCenter) {
                        C2D_DrawCircleSolid(cx, cy, objBaseZ + subStep * 5.0f, rScreen, C2D_Color32(20, 18, 30, static_cast<u8>(255 * alphaMult)));
                        drawRing(cx, cy, rScreen, activeSkin.ringThickness, cColor, 24, objBaseZ + subStep * 6.0f);
                        drawRing(cx, cy, rScreen * 0.78f, 1.0f, C2D_Color32(255, 255, 255, static_cast<u8>(80 * alphaMult)), 20, objBaseZ + subStep * 5.5f);
                    } else {
                        C2D_DrawCircleSolid(cx, cy, objBaseZ + subStep * 5.0f, rScreen, cColor);
                        C2D_DrawCircleSolid(cx, cy, objBaseZ + subStep * 5.5f, rScreen * 0.72f, C2D_Color32(static_cast<u8>(rgb.r * 0.78f), static_cast<u8>(rgb.g * 0.78f), static_cast<u8>(rgb.b * 0.78f), cAlpha));
                        drawRing(cx, cy, rScreen, activeSkin.ringThickness, ringClr, 24, objBaseZ + subStep * 6.0f);
                    }

                    char numStr[16];
                    std::snprintf(numStr, sizeof(numStr), "%d", obj.comboNumber);
                    drawCenteredText(textBuf, cx, cy, 0.55f, C2D_Color32(255, 255, 255, cAlpha), numStr, objBaseZ + subStep * 7.0f);

                    // Anillo de aproximación 3D: nace más cerca del jugador y aterriza en el plano del círculo al llegar a 0 ms
                    if (!isHidden) {
                        float timeLeft = static_cast<float>(obj.time - songTime);
                        if (timeLeft > 0.0f) {
                            float progress = timeLeft / preempt;
                            float approachRadius = rScreen * (1.0f + 2.5f * progress);
                            float dApproach = depthShift(2.6f + progress * 3.4f);
                            float ax = playX(obj.x, dApproach);
                            if (texSkinApproach.loaded) {
                                texSkinApproach.drawCenteredSprite(ax, cy, approachRadius, objBaseZ + subStep * 8.0f, cColor);
                            } else {
                                drawRing(ax, cy, approachRadius, 1.5f, cColor, 20, objBaseZ + subStep * 8.0f);
                            }
                        }
                    }
                }
            }

            // Mod Flashlight (FL): oscurece la pantalla de juego dejando un haz de luz focal centrado en el cursor
            if (isFlashlight) {
                float flX = baseOffsetX + cursorOsuX * SCALE_FACTOR + dCursor;
                float flY = baseOffsetY + cursorOsuY * SCALE_FACTOR;
                float flR = (combo >= 200) ? 46.0f : ((combo >= 100) ? 60.0f : 78.0f);
                float flRInner = flR * 0.70f;
                float flROuter = 420.0f;
                u32 clrDarkSolid = Ui::rgba(6, 4, 12, 252);
                u32 clrDarkTrans = Ui::rgba(6, 4, 12, 0);

                const int SEGMENTS = 24;
                for (int s = 0; s < SEGMENTS; ++s) {
                    float a0 = (s * 2.0f * static_cast<float>(M_PI)) / SEGMENTS;
                    float a1 = ((s + 1) * 2.0f * static_cast<float>(M_PI)) / SEGMENTS;
                    float cos0 = std::cos(a0), sin0 = std::sin(a0);
                    float cos1 = std::cos(a1), sin1 = std::sin(a1);

                    // Anillo de desvanecimiento suave (borde difuso del foco)
                    C2D_DrawTriangle(
                        flX + cos0 * flRInner, flY + sin0 * flRInner, clrDarkTrans,
                        flX + cos1 * flRInner, flY + sin1 * flRInner, clrDarkTrans,
                        flX + cos0 * flR,      flY + sin0 * flR,      clrDarkSolid,
                        0.71f
                    );
                    C2D_DrawTriangle(
                        flX + cos1 * flRInner, flY + sin1 * flRInner, clrDarkTrans,
                        flX + cos1 * flR,      flY + sin1 * flR,      clrDarkSolid,
                        flX + cos0 * flR,      flY + sin0 * flR,      clrDarkSolid,
                        0.71f
                    );

                    // Área exterior completamente a oscuras
                    C2D_DrawTriangle(
                        flX + cos0 * flR,      flY + sin0 * flR,      clrDarkSolid,
                        flX + cos1 * flR,      flY + sin1 * flR,      clrDarkSolid,
                        flX + cos0 * flROuter, flY + sin0 * flROuter, clrDarkSolid,
                        0.72f
                    );
                    C2D_DrawTriangle(
                        flX + cos1 * flR,      flY + sin1 * flR,      clrDarkSolid,
                        flX + cos1 * flROuter, flY + sin1 * flROuter, clrDarkSolid,
                        flX + cos0 * flROuter, flY + sin0 * flROuter, clrDarkSolid,
                        0.72f
                    );
                }
            }
            }

            // 2B. Chispas y particulas de Sliders / Hits en plano frontal
            for (const auto& pt : particles) {
                float lifeRatio = static_cast<float>(pt.life) / static_cast<float>(std::max(1, pt.maxLife));
                u8 pAlpha = static_cast<u8>(235.0f * lifeRatio);
                u32 pClr = Ui::withAlpha(pt.color, pAlpha);
                float pr = pt.size * (0.45f + 0.55f * lifeRatio);
                float px = baseOffsetX + pt.x + shakeX + dFrontHud;
                float py = baseOffsetY + pt.y + shakeY;
                C2D_DrawCircleSolid(px, py, 0.76f, pr * 1.6f, Ui::withAlpha(pt.color, static_cast<u8>(pAlpha * 0.35f)));
                C2D_DrawCircleSolid(px, py, 0.78f, pr, pClr);
            }

            // 3. Efecto Hit Burst y calificaciones flotantes (300 / 100 / 50 / X)
            float dHitPopup = depthShift(4.8f);
            for (size_t i = 0; i < popups.size(); ++i) {
                if (popups[i].state == HIT_300 && !settings.showHit300) continue;
                float px = (isTaikoPlay || isManiaPlay || isCatchPlay) ? (popups[i].x + dHitPopup) : playX(popups[i].x, dHitPopup);
                float py = (isTaikoPlay || isManiaPlay || isCatchPlay) ? (popups[i].y + shakeY) : playY(popups[i].y);
                float age = static_cast<float>(18 - popups[i].timer);

                if (popups[i].state != HIT_MISS && popups[i].timer > 6) {
                    float burstR = popups[i].burstRadius * (1.0f + age * 0.045f);
                    u8 bAlpha = static_cast<u8>(std::max(0, popups[i].timer * 10));
                    u32 bClr = (popups[i].burstColor & 0x00FFFFFF) | (static_cast<u32>(bAlpha) << 24);
                    drawRing(px, py, burstR, 1.8f, bClr, 18, 0.79f);
                }

                const char* txt = "300";
                u32 clr = clr300;
                if (popups[i].state == HIT_100)      { txt = "100"; clr = clr100; }
                else if (popups[i].state == HIT_50)  { txt = "50";  clr = clr50; }
                else if (popups[i].state == HIT_MISS){ txt = "X";   clr = clrMiss; }

                if (!isTaikoPlay) {
                    drawCenteredTextWithShadow(textBuf, px, py - age * 0.6f, 0.52f, clr, txt, 0.82f);
                }
            }

            u32 hpClr = (hp > 0.5f) ? clr100 : ((hp > 0.25f) ? clr50 : clrMiss);
            if (!isTaikoPlay) {
                if (drawFullHud) {
                    // Barra de Vida (HP Bar) con degradado y borde en plano HUD frontal (Pantalla Superior)
                C2D_DrawRectSolid(5.0f + dFrontHud, 5.0f, 0.29f, 148.0f, 11.0f, Ui::rgba(0, 0, 0, 170));
                C2D_DrawRectSolid(7.0f + dFrontHud, 7.0f, 0.3f, 144.0f, 7.0f, clrKeyIdle);
                if (hp > 0.0f) Ui::gradH(7.0f + dFrontHud, 7.0f, 144.0f * hp, 7.0f, 0.35f, Ui::brighten(hpClr, 0.6f), hpClr);
                if (hp < 0.25f) {
                    float blink = 0.5f + 0.5f * std::sin(animTick * 0.35f);
                    C2D_DrawRectSolid(5.0f + dFrontHud, 5.0f, 0.4f, 148.0f, 11.0f, Ui::rgba(255, 60, 60, static_cast<int>(60 * blink)));
                }

                // Barra de progreso de la cancion
                if (!beatmap.circles.empty()) {
                    float endT = static_cast<float>(beatmap.circles.back().endTime + 1000);
                    float prog = std::max(0.0f, std::min(1.0f, static_cast<float>(songTime) / std::max(1.0f, endT)));
                    C2D_DrawRectSolid(5.0f + dFrontHud, 18.0f, 0.3f, 148.0f, 2.0f, Ui::rgba(255, 255, 255, 40));
                    C2D_DrawRectSolid(5.0f + dFrontHud, 18.0f, 0.35f, 148.0f * prog, 2.0f, clrPrimary);
                }

                // HUD: Score, Accuracy, Live PP, Mods y Combo
                char hudStr[64];
                Ui::gradH(300.0f + dFrontHud, 2.0f, 100.0f, 52.0f, 0.28f, Ui::rgba(10, 8, 20, 0), Ui::rgba(10, 8, 20, 170));
                std::snprintf(hudStr, sizeof(hudStr), "%07d", score);
                drawText(textBuf, 322.0f + dFrontHud, 3.0f, 0.62f, clrWhite, hudStr);

                int totalJudged = count300 + count100 + count50 + countMiss;
                float acc = (totalJudged > 0)
                    ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                    : 100.0f;
                std::snprintf(hudStr, sizeof(hudStr), "%.1f%%", acc);
                drawText(textBuf, 340.0f + dFrontHud, 24.0f, 0.40f, clr300, hudStr);

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
                drawText(textBuf, 345.0f + dFrontHud, 38.0f, 0.36f, clrPrimary, hudStr);

                if (settings.activeMods != MOD_NONE) {
                    std::string mTag = formatModsString(settings.activeMods);
                    drawText(textBuf, 330.0f + dFrontHud, 52.0f, 0.34f, clrCursor, mTag.c_str());
                }

                // Leaderboard lateral Multijugador en vivo (Estilo osu! 3DS)
                if (multiMgr.inMultiplayer) {
                    auto rankedList = multiMgr.getLeaderboard();
                    for (size_t r = 0; r < rankedList.size() && r < 4; ++r) {
                        const auto& item = rankedList[r];
                        bool onLeft = (r < 2);
                        float cardX = (onLeft ? 8.0f : 348.0f) + dFrontHud;
                        float cardY = (r % 2 == 0) ? 55.0f : 140.0f;
                        float cardW = 44.0f;
                        float cardH = 46.0f;

                        u32 rankClr = (r == 0) ? Ui::rgba(255, 195, 45) :
                                      ((r == 1) ? Ui::rgba(40, 215, 235) :
                                      ((r == 2) ? Ui::rgba(195, 100, 255) : Ui::rgba(140, 140, 160)));

                        u32 cardBorderClr = item.player->isFailed ? Ui::rgba(235, 45, 45) : rankClr;
                        Ui::panel(cardX, cardY, cardW, cardH, 0.32f, Ui::rgba(16, 14, 28, 220), cardBorderClr);

                        char rStr[8];
                        if (item.player->isFailed) {
                            std::snprintf(rStr, sizeof(rStr), "FAIL");
                        } else {
                            std::snprintf(rStr, sizeof(rStr), "#%u", static_cast<unsigned int>(r + 1));
                        }
                        Ui::pill(cardX + 4.0f, cardY + 4.0f, cardW - 8.0f, 13.0f, 0.35f, item.player->isFailed ? Ui::rgba(235, 45, 45) : rankClr);
                        drawCenteredText(textBuf, cardX + cardW * 0.5f, cardY + 10.0f, 0.27f, Ui::rgba(0, 0, 0, 240), rStr, 0.38f);

                        std::string shortName = item.player->name;
                        if (shortName.size() > 5) shortName = shortName.substr(0, 4) + ".";
                        drawCenteredText(textBuf, cardX + cardW * 0.5f, cardY + 23.0f, 0.30f, item.player->isFailed ? Ui::rgba(255, 120, 120) : clrWhite, shortName.c_str(), 0.38f);

                        char statStr[16];
                        if (multiMgr.winCondition == WIN_ACCURACY) {
                            std::snprintf(statStr, sizeof(statStr), "%.1f%%", item.player->accuracy);
                        } else if (multiMgr.winCondition == WIN_COMBO) {
                            std::snprintf(statStr, sizeof(statStr), "%dx", item.player->maxCombo);
                        } else {
                            if (item.player->score >= 1000000) {
                                std::snprintf(statStr, sizeof(statStr), "%.1fM", item.player->score / 1000000.0f);
                            } else {
                                std::snprintf(statStr, sizeof(statStr), "%ldk", item.player->score / 1000);
                            }
                        }
                        drawCenteredText(textBuf, cardX + cardW * 0.5f, cardY + 36.0f, 0.28f, item.player->isFailed ? Ui::rgba(255, 100, 100) : rankClr, statStr, 0.38f);
                    }
                }

                // Combo con efecto pop al golpear + fantasma rojo al romper combo + pulso rítmico en Kiai
                std::snprintf(hudStr, sizeof(hudStr), "%dx", combo);
                float kiaiPulseScale = (kiaiGlow > 0.01f) ? (0.07f * kiaiGlow * bpmPulse) : 0.0f;
                float cScale = 0.60f * (comboScale + kiaiPulseScale);
                u32 comboClr = (comboBreakAnim > 0.1f) ? clrMiss :
                    ((kiaiGlow > 0.01f) ? Ui::lerpColor(clrCursor, Ui::rgba(255, 235, 110), kiaiGlow * bpmPulse * 0.75f) : clrCursor);
                drawText(textBuf, 7.0f + dFrontHud, 234.0f - 30.0f * cScale, cScale, Ui::rgba(0, 0, 0, 160), hudStr, 0.38f);
                drawText(textBuf, 6.0f + dFrontHud, 233.0f - 30.0f * cScale, cScale, comboClr, hudStr, 0.40f);
                if (comboBreakAnim > 0.02f && lastBrokenCombo >= 5) {
                    char brkStr[32];
                    std::snprintf(brkStr, sizeof(brkStr), "%dx BREAK!", lastBrokenCombo);
                    u8 bAlpha = static_cast<u8>(comboBreakAnim * 235.0f);
                    float bLift = (1.0f - comboBreakAnim) * 14.0f;
                    drawText(textBuf, 8.0f + dFrontHud, 196.0f - bLift, 0.40f, Ui::rgba(255, 75, 85, bAlpha), brkStr, 0.42f);
                }

                // Hit Error Meter (barra inferior de timing adelantado / retrasado estilo osu! PC)
                {
                    const float meterCX = 200.0f + dFrontHud;
                    const float meterY  = 232.0f;
                    const float halfW   = 62.0f;
                    float maxWin = std::max(1.0f, hit50);
                    float w100 = halfW * std::min(1.0f, beatmap.diff.hit100 / maxWin);
                    float w300 = halfW * std::min(1.0f, beatmap.diff.hit300 / maxWin);

                    C2D_DrawRectSolid(meterCX - halfW - 3.0f, meterY - 4.5f, 0.36f, (halfW + 3.0f) * 2.0f, 9.0f, Ui::rgba(8, 6, 16, 185));
                    C2D_DrawRectSolid(meterCX - halfW, meterY - 1.5f, 0.38f, halfW * 2.0f, 3.0f, Ui::withAlpha(clr50, 155));
                    C2D_DrawRectSolid(meterCX - w100,  meterY - 1.5f, 0.39f, w100 * 2.0f,  3.0f, Ui::withAlpha(clr100, 185));
                    C2D_DrawRectSolid(meterCX - w300,  meterY - 1.5f, 0.40f, w300 * 2.0f,  3.0f, Ui::withAlpha(clr300, 220));

                    for (const auto& tk : hitErrorTicks) {
                        float norm = std::max(-1.0f, std::min(1.0f, tk.errMs / maxWin));
                        float tx = meterCX + norm * halfW;
                        u8 tAlpha = static_cast<u8>(std::min(255, tk.life * 2));
                        u32 baseC = (tk.state == HIT_300) ? clr300 : ((tk.state == HIT_100) ? clr100 : clr50);
                        u32 tClr = Ui::withAlpha(baseC, tAlpha);
                        C2D_DrawLine(tx, meterY - 4.5f, tClr, tx, meterY + 4.5f, tClr, 1.5f, 0.43f);
                    }

                    // Marca central (0 ms perfecto) y aguja de tendencia media
                    C2D_DrawLine(meterCX, meterY - 5.5f, clrWhite, meterCX, meterY + 5.5f, clrWhite, 1.8f, 0.45f);
                    float needleNorm = std::max(-1.0f, std::min(1.0f, hitErrorNeedle / maxWin));
                    float nx = meterCX + needleNorm * halfW;
                    C2D_DrawTriangle(
                        nx - 3.2f, meterY - 7.5f, clrWhite,
                        nx + 3.2f, meterY - 7.5f, clrWhite,
                        nx,        meterY - 3.2f, clrCursor,
                        0.46f
                    );
                }

                // Key Overlay estilizado con contadores K1 / K2
                for (int k = 0; k < 2; ++k) {
                    bool held = (k == 0) ? k1Held : k2Held;
                    int cnt   = (k == 0) ? k1PressCount : k2PressCount;
                    float ky  = 92.0f + k * 28.0f;
                    u32 kClr  = held ? clrKeyActive : Ui::rgba(28, 24, 44, 210);
                    u32 bClr  = held ? clrWhite : clrPrimary;
                    Ui::panel(364.0f + dFrontHud, ky, 32.0f, 24.0f, 0.3f, kClr);
                    C2D_DrawRectSolid(364.0f + dFrontHud, ky, 0.34f, 3.0f, 24.0f, bClr);
                    std::snprintf(hudStr, sizeof(hudStr), "%d", cnt);
                    drawCenteredText(textBuf, 381.0f + dFrontHud, ky + 12.0f, 0.37f, held ? Ui::rgba(20, 16, 30) : clrWhite, hudStr, 0.4f);
                }
            } else {
                // Mini HUD superior discreto en la pantalla táctil inferior (barra fina de HP + Progreso)
                C2D_DrawRectSolid(0.0f, 0.0f, 0.29f, 320.0f, 4.0f, Ui::rgba(0, 0, 0, 150));
                if (hp > 0.0f) Ui::gradH(0.0f, 0.0f, 320.0f * hp, 3.0f, 0.35f, Ui::brighten(hpClr, 0.6f), hpClr);
                if (!beatmap.circles.empty()) {
                    float endT = static_cast<float>(beatmap.circles.back().endTime + 1000);
                    float prog = std::max(0.0f, std::min(1.0f, static_cast<float>(songTime) / std::max(1.0f, endT)));
                    C2D_DrawRectSolid(0.0f, 3.0f, 0.36f, 320.0f * prog, 1.5f, clrPrimary);
                }
            }
            }

            // 1. Renderizado de Humo (Smoke) estilo osu! 'C'
            if (!isTaikoPlay && !isManiaPlay && !isCatchPlay) {
                // 1. Partículas de Humo estilo osu! 'C'
                for (const auto& sp : smokeParticles) {
                    float ratio = static_cast<float>(sp.life) / static_cast<float>(sp.maxLife);
                    float fadeAlpha = std::pow(ratio, 1.35f);
                    u8 aOuter = static_cast<u8>(90.0f * fadeAlpha);
                    u8 aInner = static_cast<u8>(155.0f * fadeAlpha);
                    float sRadius = sp.size * SCALE_FACTOR * settings.cursorScale;
                    float sx = baseOffsetX + sp.x * SCALE_FACTOR + dCursor;
                    float sy = baseOffsetY + sp.y * SCALE_FACTOR;

                    u32 smokeOuterClr = Ui::rgba(210, 218, 235, aOuter);
                    u32 smokeInnerClr = Ui::rgba(235, 240, 252, aInner);

                    C2D_DrawCircleSolid(sx, sy, 0.84f, sRadius, smokeOuterClr);
                    C2D_DrawCircleSolid(sx, sy, 0.845f, sRadius * 0.52f, smokeInnerClr);
                }

                // 2. Renderizado de Estela Dinámica y Fluida del Cursor (Cursor Trail)
                for (size_t t = 0; t < cursorTrail.size(); ++t) {
                    float ratio = static_cast<float>(cursorTrail[t].life) / static_cast<float>(cursorTrail[t].maxLife);
                    float tr = (5.2f * ratio + 0.8f) * settings.cursorScale;
                    float tx = baseOffsetX + cursorTrail[t].x * SCALE_FACTOR + dCursor;
                    float ty = baseOffsetY + cursorTrail[t].y * SCALE_FACTOR;

                    u8 tAlpha = static_cast<u8>(145.0f * std::pow(ratio, 1.15f));
                    u32 tClr = C2D_Color32(
                        activeSkin.cursorTrailColor.r,
                        activeSkin.cursorTrailColor.g,
                        activeSkin.cursorTrailColor.b,
                        tAlpha
                    );

                    if (texSkinCursorTrail.loaded) {
                        texSkinCursorTrail.drawCenteredSprite(tx, ty, tr * 2.4f, 0.86f, tClr);
                    } else {
                        // Núcleo brillante y halo translúcido
                        C2D_DrawCircleSolid(tx, ty, 0.86f, tr, tClr);
                        C2D_DrawCircleSolid(tx, ty, 0.865f, tr * 0.42f, Ui::rgba(255, 255, 255, static_cast<u8>(160.0f * ratio)));
                    }

                    // Conectar con segmento hacia el punto siguiente para que la estela sea continua y sin huecos
                    if (t + 1 < cursorTrail.size()) {
                        float nextX = baseOffsetX + cursorTrail[t + 1].x * SCALE_FACTOR + dCursor;
                        float nextY = baseOffsetY + cursorTrail[t + 1].y * SCALE_FACTOR;
                        float lineW = std::max(1.5f, tr * 1.3f);
                        C2D_DrawLine(tx, ty, tClr, nextX, nextY, tClr, lineW, 0.858f);
                    }
                }

                // 3. Cursor principal
                float expandFactor = (activeSkin.cursorExpand && anyHeld) ? 7.5f : 5.5f;
                float cursorRadius = expandFactor * settings.cursorScale;
                float curScreenX = baseOffsetX + cursorOsuX * SCALE_FACTOR + dCursor;
                float curScreenY = baseOffsetY + cursorOsuY * SCALE_FACTOR;
                if (texSkinCursor.loaded) {
                    texSkinCursor.drawCenteredSprite(curScreenX, curScreenY, cursorRadius * 1.4f, 0.88f, C2D_Color32(255, 255, 255, 255));
                } else {
                    Ui::glow(curScreenX, curScreenY, 0.87f, cursorRadius * 1.8f, Ui::withAlpha(clrCursor, 110), 3);
                    C2D_DrawCircleSolid(curScreenX, curScreenY, 0.88f, cursorRadius, clrCursor);
                    C2D_DrawCircleSolid(curScreenX, curScreenY, 0.89f, 2.0f * settings.cursorScale, clrWhite);
                }
            }

            // Marcador en vivo (Leaderboard) estilo osu! lazer en el margen izquierdo (solo Single Player; en Multi se usa el Leaderboard multijugador)
            if (drawFullHud && !isTaikoPlay && !multiMgr.inMultiplayer) {
                float lbX = (3.0f + dFrontHud);
                float lbW = isTaikoPlay ? 80.0f : 74.0f;
                float lbH = isTaikoPlay ? 18.0f : 26.0f;
                float startY = isTaikoPlay ? 196.0f : 48.0f;

                // Grado actual de precisión de la partida en vivo
                int tJudged = count300 + count100 + count50 + countMiss;
                float liveAcc = (tJudged > 0)
                    ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (tJudged * 300.0f)) * 100.0f
                    : 100.0f;
                const char* liveGrade = "SS";
                u32 liveGradeClr = Ui::rgba(255, 102, 171);
                if (countMiss > 0) {
                    if (liveAcc >= 90.0f)      { liveGrade = "A"; liveGradeClr = clr100; }
                    else if (liveAcc >= 80.0f) { liveGrade = "B"; liveGradeClr = clr300; }
                    else if (liveAcc >= 70.0f) { liveGrade = "C"; liveGradeClr = clr50; }
                    else                       { liveGrade = "D"; liveGradeClr = clrMiss; }
                } else {
                    if (liveAcc >= 99.9f)      { liveGrade = "SS"; liveGradeClr = Ui::rgba(255, 102, 171); }
                    else if (liveAcc >= 93.0f) { liveGrade = "S";  liveGradeClr = clrCursor; }
                    else if (liveAcc >= 85.0f) { liveGrade = "A";  liveGradeClr = clr100; }
                    else if (liveAcc >= 75.0f) { liveGrade = "B";  liveGradeClr = clr300; }
                    else                       { liveGrade = "C";  liveGradeClr = clr50; }
                }

                struct LeaderEntry {
                    const char* tag;
                    const char* name;
                    int pts;
                    const char* gr;
                    u32 grClr;
                    bool isLive;
                };
                std::vector<LeaderEntry> entries;

                if (!localRecords.empty()) {
                    int bestScore = localRecords[0].bestScore;
                    const char* bGr = localRecords[0].bestGrade.c_str();
                    u32 bGrClr = clrWhite;
                    if (localRecords[0].bestGrade == "SS") bGrClr = Ui::rgba(255, 102, 171);
                    else if (localRecords[0].bestGrade == "S") bGrClr = clrCursor;
                    else if (localRecords[0].bestGrade == "A") bGrClr = clr100;
                    else if (localRecords[0].bestGrade == "B") bGrClr = clr300;
                    else bGrClr = clrMiss;

                    if (score >= bestScore) {
                        entries.push_back({ "#1", "TU", score, liveGrade, liveGradeClr, true });
                        entries.push_back({ "#2", "RECORD", bestScore, bGr, bGrClr, false });
                    } else {
                        entries.push_back({ "#1", "RECORD", bestScore, bGr, bGrClr, false });
                        entries.push_back({ "#2", "TU", score, liveGrade, liveGradeClr, true });
                    }
                } else {
                    entries.push_back({ "#1", "TU", score, liveGrade, liveGradeClr, true });
                }

                for (size_t e = 0; e < entries.size(); ++e) {
                    const auto& ent = entries[e];
                    float ey = startY + e * (lbH + (isTaikoPlay ? 3.0f : 5.0f));
                    u32 cBorder = ent.isLive
                        ? (hasOvertakenBest ? Ui::rgba(255, 215, 60) : clrCursor)
                        : Ui::rgba(65, 55, 90);
                    u32 cBg = ent.isLive
                        ? (hasOvertakenBest ? Ui::rgba(54, 44, 20, 230) : Ui::rgba(34, 26, 52, 230))
                        : Ui::rgba(18, 14, 28, 210);

                    if (ent.isLive && overtakeFlashAnim > 0.05f) {
                        cBg = Ui::lerpColor(cBg, Ui::rgba(255, 215, 60), overtakeFlashAnim * 0.5f);
                        cBorder = Ui::rgba(255, 255, 220);
                    }

                    Ui::panel(lbX, ey, lbW, lbH, 0.32f, cBg, cBorder);
                    if (isTaikoPlay) {
                        drawText(textBuf, lbX + 4.0f, ey + 3.0f, 0.28f, ent.isLive ? clr100 : clrWhite, ent.tag, 0.35f);
                        drawText(textBuf, lbX + 22.0f, ey + 3.0f, 0.26f, ent.isLive ? clrCursor : clrMuted, ent.name, 0.35f);
                        drawCenteredText(textBuf, lbX + lbW - 10.0f, ey + 9.0f, 0.32f, ent.grClr, ent.gr, 0.35f);
                        char sBuf[32];
                        std::snprintf(sBuf, sizeof(sBuf), "%d", ent.pts);
                        drawText(textBuf, lbX + 4.0f, ey + 9.5f, 0.24f, clrWhite, sBuf, 0.35f);
                    } else {
                        drawText(textBuf, lbX + 4.0f, ey + 4.0f, 0.30f, ent.isLive ? clr100 : clrWhite, ent.tag, 0.35f);
                        drawText(textBuf, lbX + 22.0f, ey + 4.0f, 0.28f, ent.isLive ? clrCursor : clrMuted, ent.name, 0.35f);
                        drawCenteredText(textBuf, lbX + lbW - 10.0f, ey + 8.5f, 0.35f, ent.grClr, ent.gr, 0.35f);
                        char sBuf[32];
                        std::snprintf(sBuf, sizeof(sBuf), "%d", ent.pts);
                        drawText(textBuf, lbX + 4.0f, ey + 15.0f, 0.27f, clrWhite, sBuf, 0.35f);
                    }
                }
            }

            // Pantalla central de Descanso (Break Time) con calificación, cuenta atrás y advertencia
            if (inBreakPeriod) {
                if (isTaikoPlay) {
                    // En Taiko, mantener la pista visible y mostrar un cartel limpio estilo descanso de festival arriba
                    float bcX = 200.0f + dFrontHud;
                    float bcY = 46.0f;
                    Ui::panel(bcX - 60.0f, bcY - 18.0f, 120.0f, 36.0f, 0.32f, Ui::rgba(20, 16, 34, 230), clrPrimary);
                    drawCenteredText(textBuf, bcX, bcY - 8.0f, 0.35f, clrWhite, LOC(STR_GAME_BREAK), 0.35f);
                    char cdStr[32];
                    float sLeft = std::max(0.0f, breakRemainingMs / 1000.0f);
                    std::snprintf(cdStr, sizeof(cdStr), "%.1f s", sLeft);
                    drawCenteredText(textBuf, bcX, bcY + 6.0f, 0.32f, clr300, cdStr, 0.35f);
                } else {
                    C2D_DrawRectSolid(baseOffsetX + shakeX + dPlayfield, shakeY, 0.48f, 320.0f, 240.0f, Ui::rgba(10, 8, 20, 120));

                    float bcX = playX(256.0f, dFrontHud);
                    float bcY = playY(116.0f);

                    Ui::panel(bcX - 85.0f, bcY - 44.0f, 170.0f, 96.0f, 0.50f, Ui::rgba(20, 16, 34, 245), clrPrimary);
                    Ui::pill(bcX - 44.0f, bcY - 39.0f, 88.0f, 14.0f, 0.52f, Ui::rgba(255, 102, 171, 230));
                    drawCenteredText(textBuf, bcX, bcY - 32.0f, 0.32f, clrWhite, LOC(STR_GAME_BREAK), 0.54f);

                    // Letra grande de Accuracy Grade actual
                    int tJudged = count300 + count100 + count50 + countMiss;
                    float bAcc = (tJudged > 0)
                        ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (tJudged * 300.0f)) * 100.0f
                        : 100.0f;
                    const char* bGrade = "SS";
                    u32 bGradeClr = Ui::rgba(255, 102, 171);
                    if (countMiss > 0) {
                        if (bAcc >= 90.0f)      { bGrade = "A"; bGradeClr = clr100; }
                        else if (bAcc >= 80.0f) { bGrade = "B"; bGradeClr = clr300; }
                        else if (bAcc >= 70.0f) { bGrade = "C"; bGradeClr = clr50; }
                        else                    { bGrade = "D"; bGradeClr = clrMiss; }
                    } else {
                        if (bAcc >= 99.9f)      { bGrade = "SS"; bGradeClr = Ui::rgba(255, 102, 171); }
                        else if (bAcc >= 93.0f) { bGrade = "S";  bGradeClr = clrCursor; }
                        else if (bAcc >= 85.0f) { bGrade = "A";  bGradeClr = clr100; }
                        else if (bAcc >= 75.0f) { bGrade = "B";  bGradeClr = clr300; }
                        else                    { bGrade = "C";  bGradeClr = clr50; }
                    }

                    Ui::glow(bcX, bcY - 4.0f, 0.51f, 32.0f, Ui::withAlpha(bGradeClr, 110), 3);
                    drawCenteredText(textBuf, bcX, bcY - 4.0f, 0.85f, bGradeClr, bGrade, 0.55f);

                    char cdStr[48];
                    float sLeft = std::max(0.0f, breakRemainingMs / 1000.0f);
                    std::snprintf(cdStr, sizeof(cdStr), LOC(STR_GAME_CIRCLES_IN), sLeft);
                    drawCenteredText(textBuf, bcX, bcY + 21.0f, 0.38f, clrWhite, cdStr, 0.54f);

                    std::snprintf(cdStr, sizeof(cdStr), "%s: %.1f%%  •  %dx", LOC(STR_ACCURACY), bAcc, combo);
                    drawCenteredText(textBuf, bcX, bcY + 33.0f, 0.28f, clrMuted, cdStr, 0.54f);
                    drawCenteredText(textBuf, bcX, bcY + 44.0f, 0.25f, clrCursor, LOC(STR_GAME_SMOKE), 0.54f);

                    // Indicador de advertencia visual (últimos 2.2 segundos antes de que vuelvan las notas)
                    if (breakRemainingMs <= 2200) {
                        float warnPhase = static_cast<float>(2200 - breakRemainingMs) / 2200.0f;
                        float blink = 0.5f + 0.5f * std::sin(animTick * 0.45f);

                        // Pill superior parpadeante de ¡ATENTO!
                        Ui::pill(bcX - 45.0f, bcY - 64.0f, 90.0f, 16.0f, 0.56f, Ui::withAlpha(clrMiss, static_cast<u8>(180 + 75 * blink)));
                        drawCenteredText(textBuf, bcX, bcY - 56.0f, 0.35f, clrWhite, LOC(STR_GAME_WARNING), 0.58f);

                        // Anillos concéntricos convergiendo hacia el próximo círculo
                        float nx = playX(breakNextObjPos.x, dCircle);
                        float ny = playY(breakNextObjPos.y);
                        float targetR = rScreen * (1.1f + (1.0f - warnPhase) * 1.5f);

                        drawRing(nx, ny, targetR, 2.2f, Ui::withAlpha(clrCursor, static_cast<u8>(220 * blink)), 24, 0.58f);
                        drawRing(nx, ny, rScreen, 1.4f, Ui::withAlpha(clrWhite, 190), 20, 0.58f);

                        // Flechas laterales apuntando hacia el objetivo
                        float aDist = targetR + 8.0f;
                        C2D_DrawTriangle(nx - aDist, ny, clrCursor, nx - aDist - 7.0f, ny - 6.0f, clrCursor, nx - aDist - 7.0f, ny + 6.0f, clrCursor, 0.59f);
                        C2D_DrawTriangle(nx + aDist, ny, clrCursor, nx + aDist + 7.0f, ny - 6.0f, clrCursor, nx + aDist + 7.0f, ny + 6.0f, clrCursor, 0.59f);
                    }
                }
            }

            // Botón SKIP >> animado con estilo osu!
            if (skipAvailable && !isPaused) {
                float skipX = drawFullHud ? (302.0f + dFrontHud) : 210.0f;
                float skipY = drawFullHud ? 198.0f : 194.0f;
                float skipW = drawFullHud ? 92.0f : 100.0f;
                float skipH = drawFullHud ? 32.0f : 36.0f;

                float skipPulse = 0.5f + 0.5f * std::sin(animTick * 0.25f);
                u32 sBg = Ui::lerpColor(Ui::rgba(255, 80, 140, 240), Ui::rgba(255, 120, 180, 250), skipPulse);

                Ui::panel(skipX, skipY, skipW, skipH, 0.94f, sBg, clrWhite);
                drawCenteredText(textBuf, skipX + (skipW * 0.5f), skipY + (skipH * 0.35f), 0.46f, clrWhite, "SKIP >>", 0.96f);
                drawCenteredText(textBuf, skipX + (skipW * 0.5f), skipY + (skipH * 0.74f), 0.26f, Ui::rgba(255, 235, 245),
                                 drawFullHud ? "ZL / ZR / START" : LOC(STR_SKIP_HINT), 0.96f);
            }

            // Combo Burst (Aparición de personaje / mascota en hitos de combo: 30x, 60x, 100x, 150x...)
            if (comboBurstTimer > 0 && drawFullHud && !isPaused) {
                float slideT = 1.0f;
                float alpha = 1.0f;
                if (comboBurstTimer > 32) {
                    float t = static_cast<float>(48 - comboBurstTimer) / 16.0f;
                    slideT = std::sin(t * (static_cast<float>(M_PI) * 0.5f));
                    alpha = std::min(1.0f, t * 1.5f);
                } else if (comboBurstTimer <= 14) {
                    float t = static_cast<float>(comboBurstTimer) / 14.0f;
                    slideT = t;
                    alpha = t;
                }

                float burstW = 120.0f;
                float burstH = 175.0f;
                float restX = (comboBurstSide == 0) ? (screenW - burstW + 15.0f) : -15.0f;
                float offX  = (comboBurstSide == 0) ? (screenW + 10.0f) : (-burstW - 10.0f);
                float dBurst = depthShift(4.4f);
                float burstX = offX + (restX - offX) * slideT + dBurst;
                float burstY = 240.0f - burstH + 5.0f;
                u8 aVal = static_cast<u8>(255.0f * alpha);

                if (texSkinComboBurst.loaded) {
                    texSkinComboBurst.drawRect(burstX, burstY, burstW, burstH, 0.82f, aVal);
                } else {
                    // Mascota vectorial estilizada de osu! (Chibi Pippi con orejitas anime, destellos y badge)
                    float cx = burstX + burstW * 0.5f;
                    float cy = burstY + burstH * 0.55f;
                    u32 pPink = Ui::withAlpha(Ui::rgba(255, 102, 171), aVal);
                    u32 pGold = Ui::withAlpha(Ui::rgba(255, 215, 60), aVal);
                    u32 pDark = Ui::withAlpha(Ui::rgba(26, 20, 42), static_cast<u8>(aVal * 0.90f));

                    // Resplandor de fondo
                    Ui::glow(cx, cy, 0.80f, 65.0f, Ui::withAlpha(pPink, static_cast<u8>(aVal * 0.45f)), 3);

                    // Silueta de cuerpo y cabeza estilizada
                    C2D_DrawCircleSolid(cx, cy - 14.0f, 0.81f, 32.0f, pPink);
                    C2D_DrawCircleSolid(cx, cy - 14.0f, 0.82f, 28.0f, pDark);

                    // Auriculares de gatito / orejitas anime
                    C2D_DrawTriangle(cx - 24.0f, cy - 30.0f, pPink, cx - 10.0f, cy - 42.0f, pPink, cx - 8.0f, cy - 28.0f, pPink, 0.83f);
                    C2D_DrawTriangle(cx + 24.0f, cy - 30.0f, pPink, cx + 10.0f, cy - 42.0f, pPink, cx + 8.0f, cy - 28.0f, pPink, 0.83f);
                    C2D_DrawTriangle(cx - 20.0f, cy - 30.0f, pGold, cx - 11.0f, cy - 38.0f, pGold, cx - 9.0f, cy - 28.0f, pGold, 0.84f);
                    C2D_DrawTriangle(cx + 20.0f, cy - 30.0f, pGold, cx + 11.0f, cy - 38.0f, pGold, cx + 9.0f, cy - 28.0f, pGold, 0.84f);

                    // Ojos estilo anime felices (^^)
                    drawRing(cx - 10.0f, cy - 16.0f, 6.0f, 1.8f, pPink, 14, 0.84f);
                    drawRing(cx + 10.0f, cy - 16.0f, 6.0f, 1.8f, pPink, 14, 0.84f);
                    C2D_DrawCircleSolid(cx - 12.0f, cy - 10.0f, 0.84f, 4.0f, Ui::withAlpha(Ui::rgba(255, 140, 180), aVal));
                    C2D_DrawCircleSolid(cx + 12.0f, cy - 10.0f, 0.84f, 4.0f, Ui::withAlpha(Ui::rgba(255, 140, 180), aVal));

                    // Estrellas flotantes de celebración
                    float sBob = std::sin(animTick * 0.2f) * 4.0f;
                    Ui::glow(cx - 36.0f, cy - 35.0f + sBob, 0.85f, 10.0f, pGold, 2);
                    C2D_DrawCircleSolid(cx - 36.0f, cy - 35.0f + sBob, 0.86f, 4.0f, pGold);
                    Ui::glow(cx + 36.0f, cy - 25.0f - sBob, 0.85f, 10.0f, pGold, 2);
                    C2D_DrawCircleSolid(cx + 36.0f, cy - 25.0f - sBob, 0.86f, 4.5f, pGold);
                }

                // Cinta o Badge con el hito: "100 COMBO!"
                char cbStr[32];
                std::snprintf(cbStr, sizeof(cbStr), "%d COMBO!", comboBurstValue);
                float pillW = 96.0f;
                float pillH = 18.0f;
                float pillX = burstX + (burstW - pillW) * 0.5f;
                float pillY = burstY + burstH - 26.0f;

                Ui::pill(pillX, pillY, pillW, pillH, 0.85f, Ui::withAlpha(Ui::rgba(255, 102, 171), aVal));
                drawCenteredText(textBuf, pillX + pillW * 0.5f, pillY + pillH * 0.5f, 0.38f, Ui::withAlpha(clrWhite, aVal), cbStr, 0.88f);
            }

            // Overlay de Pausa en pantalla superior (estilo osu!lazer)
            if (drawFullHud && isPaused) {
                float dPause = depthShift(5.0f);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.90f, 400.0f, 240.0f, Ui::rgba(8, 6, 16, 185));
                Ui::panel(70.0f + dPause, 36.0f, 260.0f, 168.0f, 0.92f, Ui::rgba(22, 18, 38, 248), clrPrimary);
                drawCenteredText(textBuf, 200.0f + dPause, 58.0f, 0.62f, clrCursor, LOC(STR_GAME_PAUSED), 0.99f);

                const char* pOpts[3] = { LOC(STR_PAUSE_RESUME), LOC(STR_PAUSE_RETRY), LOC(STR_PAUSE_QUIT) };
                const char* pKeys[3] = { "A", "X", "B" };
                u32 pClrs[3] = { clr100, clrCursor, clrPrimary };
                for (int i = 0; i < 3; ++i) {
                    bool sel = (pauseOption == i);
                    float py = 84.0f + i * 34.0f;
                    float px = 86.0f + dPause;
                    float pw = 228.0f;
                    float ph = 28.0f;
                    u32 bg = sel ? Ui::rgba(45, 36, 72, 250) : Ui::rgba(24, 20, 38, 235);
                    Ui::button(px, py, pw, ph, 0.95f, bg, pClrs[i], sel);

                    Ui::pill(px + 8.0f, py + 5.0f, 18.0f, 18.0f, 0.96f, sel ? pClrs[i] : Ui::rgba(45, 38, 65));
                    drawCenteredText(textBuf, px + 17.0f, py + 14.0f, 0.28f, sel ? Ui::rgba(0, 0, 0, 240) : clrWhite, pKeys[i], 0.98f);
                    drawTextWithShadow(textBuf, px + 34.0f, py + 5.0f, 0.40f, clrWhite, pOpts[i], 0.98f);
                }
                if (isTaikoPlay) {
                    u32 bClr = (settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25, 230) : ((settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215, 230) : Ui::rgba(165, 55, 185, 230));
                    Ui::pill(90.0f + dPause, 186.0f, 220.0f, 15.0f, 0.94f, bClr);
                    char bgInfo[64];
                    std::snprintf(bgInfo, sizeof(bgInfo), "X: %s (%s)", LOC(STR_SET_TAIKO_BG), settings.getTaikoBgModeName());
                    drawCenteredText(textBuf, 200.0f + dPause, 193.5f, 0.26f, clrWhite, bgInfo, 0.99f);
                }
            }
            // Destello inicial de Kiai Time (en primer plano sin contaminar el depth buffer de los círculos)
            if (kiaiBurstFlash > 0.01f) {
                u8 fAlpha = static_cast<u8>(kiaiBurstFlash * 65.0f);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.82f, screenW, 240.0f, Ui::rgba(255, 240, 200, fAlpha));
            }

            // Borde de aviso al romper combo (Combo Break)
            if (comboBreakAnim > 0.01f) {
                u8 cbAlpha = static_cast<u8>(comboBreakAnim * 110.0f);
                drawRectOutline(0.0f, 0.0f, screenW, 240.0f, 4.0f, Ui::rgba(255, 60, 75, cbAlpha), 0.85f);
            }

            Ui::drawTransition(screenW, 240.0f, screenFade, clrPrimary);
        };

        // --- PANTALLA SUPERIOR (OJO IZQUIERDO Y OJO DERECHO EN 3D + HUD COMPLETO) ---
        for (int eye = 0; eye < (stereoActive ? 2 : 1); ++eye) {
            C3D_RenderTarget* curTop = (eye == 0) ? topScreen : topScreenRight;
            float eyeSign = stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
            C2D_TargetClear(curTop, clrBgDark);
            C2D_SceneBegin(curTop);
            renderPlayfield(400.0f, PLAYFIELD_TOP_OFFSET_X, PLAYFIELD_TOP_OFFSET_Y, eyeSign, slider3D, true);
        }

        // --- PANTALLA INFERIOR: JUEGO TÁCTIL DIRECTO 1:1, SUPERFICIE DE TABLET O PAUSA ---
        C2D_TargetClear(bottomScreen, clrTabletBg);
        C2D_SceneBegin(bottomScreen);

        if (isPaused) {
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(22, 16, 38), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);
            const char* pOpts[3] = { LOC(STR_PAUSE_RESUME), LOC(STR_PAUSE_RETRY), LOC(STR_PAUSE_QUIT) };
            const char* pKeys[3] = { "A", "X", "B" };
            u32 pClrs[3] = { clr100, clrCursor, clrPrimary };
            for (int i = 0; i < 3; ++i) {
                bool sel = (pauseOption == i);
                float bx = 30.0f;
                float by = 38.0f + i * 52.0f;
                float bw = 260.0f;
                float bh = 44.0f;

                u32 bg = sel ? Ui::rgba(45, 36, 72, 250) : Ui::rgba(22, 19, 36, 235);
                Ui::button(bx, by, bw, bh, 0.20f, bg, pClrs[i], sel);

                // Pastilla indicadora de la tecla
                Ui::pill(bx + 12.0f, by + 12.0f, 22.0f, 20.0f, 0.24f, sel ? pClrs[i] : Ui::rgba(40, 34, 58, 240));
                drawCenteredText(textBuf, bx + 23.0f, by + 22.0f, 0.32f, sel ? Ui::rgba(0, 0, 0, 240) : clrWhite, pKeys[i], 0.28f);

                // Texto del boton con sombra de alto contraste
                drawTextWithShadow(textBuf, bx + 44.0f, by + 13.0f, 0.46f, clrWhite, pOpts[i], 0.28f);
            }
            if (isTaikoPlay) {
                u32 bClr = (settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25, 240) : ((settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215, 240) : Ui::rgba(165, 55, 185, 240));
                Ui::pill(40.0f, 206.0f, 240.0f, 24.0f, 0.25f, bClr);
                char bgInfo[64];
                std::snprintf(bgInfo, sizeof(bgInfo), "%s: %s (Toca o X)", LOC(STR_SET_TAIKO_BG), settings.getTaikoBgModeName());
                drawCenteredText(textBuf, 160.0f, 218.0f, 0.32f, clrWhite, bgInfo, 0.85f);
            }
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
        } else if (isTaikoPlay) {
            // Modo osu!taiko: Tambor interactivo táctil completo en la pantalla inferior (estilo 3DS 1:1)
            taikoEngine.renderBottomDrum(textBuf, static_cast<float>(animTick), &settings, taikoGuideAlpha);

            // Botón SKIP si aplica
            if (skipAvailable) {
                float skipPulse = 0.5f + 0.5f * std::sin(animTick * 0.25f);
                u32 sBg = Ui::lerpColor(Ui::rgba(255, 80, 140, 240), Ui::rgba(255, 120, 180, 250), skipPulse);
                Ui::panel(212.0f, 196.0f, 100.0f, 34.0f, 0.85f, sBg, clrWhite);
                drawCenteredText(textBuf, 262.0f, 208.0f, 0.44f, clrWhite, "SKIP >>", 0.90f);
                drawCenteredText(textBuf, 262.0f, 222.0f, 0.25f, Ui::rgba(255, 230, 242), LOC(STR_SKIP_HINT), 0.90f);
            }
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
        } else if (isManiaPlay) {
            // Modo osu!mania: Teclado interactivo de 4 columnas en la pantalla inferior
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(18, 14, 30), Ui::rgba(8, 6, 14), clrPrimary, 0.01f);
            maniaEngine.renderBottomLanes(textBuf, static_cast<float>(animTick));

            // Mini HUD superior discreto (Barra de HP)
            u32 hpClr = (hp > 0.5f) ? clr100 : ((hp > 0.25f) ? clr50 : clrMiss);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.29f, 320.0f, 4.0f, Ui::rgba(0, 0, 0, 150));
            if (hp > 0.0f) Ui::gradH(0.0f, 0.0f, 320.0f * hp, 3.0f, 0.35f, Ui::brighten(hpClr, 0.6f), hpClr);

            // Botón SKIP si aplica
            if (skipAvailable) {
                float skipPulse = 0.5f + 0.5f * std::sin(animTick * 0.25f);
                u32 sBg = Ui::lerpColor(Ui::rgba(255, 80, 140, 240), Ui::rgba(255, 120, 180, 250), skipPulse);
                Ui::panel(212.0f, 196.0f, 100.0f, 34.0f, 0.85f, sBg, clrWhite);
                drawCenteredText(textBuf, 262.0f, 208.0f, 0.44f, clrWhite, "SKIP >>", 0.90f);
                drawCenteredText(textBuf, 262.0f, 222.0f, 0.25f, Ui::rgba(255, 230, 242), LOC(STR_SKIP_HINT), 0.90f);
            }
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
        } else if (isCatchPlay) {
            // Modo osu!catch: Deck de control táctil con radar de frutas en la pantalla inferior
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(16, 22, 34), Ui::rgba(8, 12, 18), clrPrimary, 0.01f);
            catchEngine.renderBottomDeck(textBuf, static_cast<float>(animTick), songTime);

            // Mini HUD superior discreto (Barra de HP)
            u32 hpClr = (hp > 0.5f) ? clr100 : ((hp > 0.25f) ? clr50 : clrMiss);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.29f, 320.0f, 4.0f, Ui::rgba(0, 0, 0, 150));
            if (hp > 0.0f) Ui::gradH(0.0f, 0.0f, 320.0f * hp, 3.0f, 0.35f, Ui::brighten(hpClr, 0.6f), hpClr);

            // Botón SKIP si aplica
            if (skipAvailable) {
                float skipPulse = 0.5f + 0.5f * std::sin(animTick * 0.25f);
                u32 sBg = Ui::lerpColor(Ui::rgba(255, 80, 140, 240), Ui::rgba(255, 120, 180, 250), skipPulse);
                Ui::panel(212.0f, 196.0f, 100.0f, 34.0f, 0.85f, sBg, clrWhite);
                drawCenteredText(textBuf, 262.0f, 208.0f, 0.44f, clrWhite, "SKIP >>", 0.90f);
                drawCenteredText(textBuf, 262.0f, 222.0f, 0.25f, Ui::rgba(255, 230, 242), LOC(STR_SKIP_HINT), 0.90f);
            }
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
        } else if (settings.playOnBottomScreen) {
            // ¡Modo Táctil Directo! Renderiza el playfield completo 1:1 en la pantalla inferior (320x240)
            renderPlayfield(320.0f, PLAYFIELD_BOT_OFFSET_X, PLAYFIELD_BOT_OFFSET_Y, 0.0f, 0.0f, false);
        } else {
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(animTick), Ui::rgba(22, 16, 38), Ui::rgba(10, 8, 18), clrPrimary, 0.01f);
            float rx, ry, rw, rh;
            settings.getTabletRect(rx, ry, rw, rh);

            Ui::panel(rx, ry, rw, rh, 0.1f, C2D_Color32(28, 24, 44, 235), clrPrimary);
            u32 gridClr = C2D_Color32(95, 80, 145, 70);
            C2D_DrawLine(rx + rw * 0.5f, ry, gridClr, rx + rw * 0.5f, ry + rh, gridClr, 1.0f, 0.14f);
            C2D_DrawLine(rx, ry + rh * 0.5f, gridClr, rx + rw, ry + rh * 0.5f, gridClr, 1.0f, 0.14f);
            drawRectOutline(rx, ry, rw, rh, 1.5f, C2D_Color32(120, 95, 185, 200), 0.16f);
            if (kiaiGlow > 0.01f) {
                float pulse = getBpmPulse();
                u8 tAlpha = static_cast<u8>(kiaiGlow * (45.0f + 110.0f * pulse));
                drawRectOutline(rx - 1.0f, ry - 1.0f, rw + 2.0f, rh + 2.0f, 2.0f, Ui::rgba(255, 195, 75, tAlpha), 0.17f);
            }
            drawRing(settings.tabletCenterX, settings.tabletCenterY, 12.0f, 1.2f, C2D_Color32(110, 95, 165, 180), 18, 0.18f);

            // Barra inferior de información rápida de la canción
            Ui::gradV(0.0f, 218.0f, 320.0f, 22.0f, 0.25f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 240));
            std::string nowPlaying = truncateString(beatmap.title + " [" + beatmap.version + "]", 42);
            drawCenteredText(textBuf, 160.0f, 229.0f, 0.36f, clrMuted, nowPlaying.c_str(), 0.3f);

            if (isTouching) {
                Ui::glow(touch.px, touch.py, 0.45f, 12.0f, Ui::withAlpha(clrKeyActive, 130), 3);
                C2D_DrawCircleSolid(touch.px, touch.py, 0.5f, 4.0f, clrKeyActive);
            }

            if (skipAvailable && !isPaused) {
                float skipPulse = 0.5f + 0.5f * std::sin(animTick * 0.25f);
                u32 sBg = Ui::lerpColor(Ui::rgba(255, 80, 140, 240), Ui::rgba(255, 120, 180, 250), skipPulse);
                Ui::panel(212.0f, 196.0f, 100.0f, 34.0f, 0.85f, sBg, clrWhite);
                drawCenteredText(textBuf, 262.0f, 208.0f, 0.44f, clrWhite, "SKIP >>", 0.90f);
                drawCenteredText(textBuf, 262.0f, 222.0f, 0.25f, Ui::rgba(255, 230, 242), LOC(STR_SKIP_HINT), 0.90f);
            }
            Ui::drawTransition(320.0f, 240.0f, screenFade, clrPrimary);
        }

        C3D_FrameEnd(0);
    }

    videoPlayer.shutdown();
    audio.stopAsyncLoader();
    bgLoader.stop();
    bgTex.unload();
    texSkinCursor.unload();
    texSkinCursorTrail.unload();
    texSkinCircle.unload();
    texSkinOverlay.unload();
    texSkinApproach.unload();
    texSkinComboBurst.unload();
    settings.save();
    audio.exitAll();
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    acExit();
    gfxExit();
    osSetSpeedupEnable(false);
    return 0;
}
