#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <citro2d.h>
#include "Beatmap.hpp"
#include "Settings.hpp"
#include "AudioPlayer.hpp"
#include "UiKit.hpp"

enum TaikoNoteType {
    TAIKO_DON = 0,       // Centro Rojo (Normal)
    TAIKO_KATSU = 1,     // Borde Azul (Normal)
    TAIKO_BIG_DON = 2,   // Centro Rojo Grande
    TAIKO_BIG_KATSU = 3, // Borde Azul Grande
    TAIKO_DRUMROLL = 4,  // Barra Amarilla de Redoble
    TAIKO_DENDEN = 5     // Globo / Spinner Taiko
};

struct TaikoNote {
    TaikoNoteType type = TAIKO_DON;
    int time = 0;
    int endTime = 0;
    HitState state = NOT_HIT;
    bool isHit = false;
    float scrollSpeed = 1.0f;
    int drumrollTicksHit = 0;
    int drumrollTotalTicks = 0;
    int dendenRequiredHits = 0;
    int dendenCurrentHits = 0;
};

class ModeTaiko {
public:
    std::vector<TaikoNote> notes;
    size_t nextNoteIndex = 0;

    // Estados visuales del tambor en pantalla inferior
    float drumHitDonLeft = 0.0f;
    float drumHitDonRight = 0.0f;
    float drumHitKatsuLeft = 0.0f;
    float drumHitKatsuRight = 0.0f;

    // Animación de impacto en el receptor (Hit Target)
    float targetBurstScale = 1.0f;
    u32 targetBurstColor = 0;
    float targetBurstAlpha = 0.0f;

    // Animación de mascota Don-chan
    float mascotBounce = 0.0f;

    // Indicador de Redoble (Fan / Abanico "Drum Roll")
    int drumrollActiveCounter = 0;
    float drumrollFanTimer = 0.0f;

    // Calificación temporal para popup sobre el receptor
    int lastHitJudgement = 0; // 300 (GOOD), 100 (OK), -1 (MISS)
    float hitJudgementTimer = 0.0f;

    // Emociones dinámicas de Don-chan y animaciones de celebración
    float missFaceTimer = 0.0f;
    float balloonPopTimer = 0.0f;
    int totalDrumrollHits = 0;

    // Constantes de resultado de golpe
    static constexpr int TAIKO_HIT_NONE   = 0;
    static constexpr int TAIKO_HIT_300    = 300;
    static constexpr int TAIKO_HIT_100    = 100;
    static constexpr int TAIKO_HIT_MISS   = -1;
    static constexpr int TAIKO_HIT_TICK   = 1; // Tick de redoble (da puntos y combo, NO penaliza precisión)
    static constexpr int TAIKO_HIT_DENDEN = 2; // Tick de denden (da puntos y combo, NO penaliza precisión)

    // Constantes de diseño del playfield de Taiko (Alineado con Arcade / 3DS)
    static constexpr float TRACK_Y = 89.0f;
    static constexpr float TARGET_X = 114.0f; // Receptor desplazado para dar espacio al combo drum & score
    static constexpr float NOTE_RADIUS = 18.0f;
    static constexpr float BIG_NOTE_RADIUS = 26.0f;

    // Colores oficiales estilo Taiko no Tatsujin 3DS
    static constexpr u32 CLR_DON_INNER = 0xFF3535FF;   // Rojo brillante C2D ABGR
    static constexpr u32 CLR_DON_OUTER = 0xFF1010CC;   // Rojo oscuro contorno
    static constexpr u32 CLR_KATSU_INNER = 0xFFFF9922; // Azul celeste brillante
    static constexpr u32 CLR_KATSU_OUTER = 0xFFCC5500; // Azul profundo contorno
    static constexpr u32 CLR_DRUMROLL = 0xFF22D8FF;    // Amarillo oro 3DS
    static constexpr u32 CLR_DRUMROLL_DARK = 0xFF0090EE; // Oro ámbar para sombras
    static constexpr u32 CLR_DENDEN = 0xFF33CCFF;
    static constexpr u32 CLR_WHITE = 0xFFFFFFFF;

private:
    static void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24, float z = 0.5f);
    static void drawEllipseFilled(float cx, float cy, float rx, float ry, u32 color, float z = 0.25f, int segs = 28);
    static void drawEllipseRing(float cx, float cy, float rx, float ry, float thickness, u32 color, float z = 0.26f, int segs = 28);
    static void drawSunburst(float cx, float cy, float rInner, float rOuter, int rays, u32 color, float z = 0.23f, float rot = 0.0f);
    static void drawWadaikoByo(float cx, float cy, float rx, float ry, int count, u32 rivetClr, float z = 0.13f);
    static void drawKikkoPattern(float startX, float startY, float w, float h, float z = 0.03f);
    static void drawJapaneseCloud(float cx, float cy, float scale, u32 bodyClr, u32 borderClr, float z = 0.11f);
    static void drawFestivalDancer(float cx, float baseY, float bounce, int variant, float z = 0.12f);
    static void drawKomaTop(float cx, float cy, float scale, float rot, float z = 0.115f);
    static void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f);
    static void drawRightAlignedText(C2D_TextBuf buf, float rx, float cy, float scale, u32 color, const char* str, float z = 0.85f);
    static void drawTaikoJudgementBadge(float cx, float cy, int judgement, float timer, C2D_TextBuf textBuf, float z = 0.45f);
    static void drawSakuraPetal(float cx, float cy, float angle, float z = 0.26f);
    static void drawBachi(float handleX, float handleY, float tipX, float tipY, float hitIntensity, bool isDonHit, float z = 0.225f);

public:
    static void drawTaikoFace(float cx, float cy, float r, bool isBig, bool isDon, float z = 0.285f, int emotion = 0);

    // Lógica de juego y evaluación (ModeTaiko.cpp)
    void reset();
    void initFromBeatmap(const Beatmap& beatmap);
    void update(float bpmPulse);
    int processHit(bool isDon, bool isBigAction, int songTime, float hit300, float hit100, float hit50, bool isAutoplay = false);
    int checkMisses(int songTime, float hit50, bool isAutoplay = false);
    void triggerBurst(u32 color);
    int handleTouchDrum(float px, float py);

    // Renderizado visual (TaikoRenderer.cpp)
    void renderTopScreen(C2D_TextBuf textBuf, int songTime, float scrollRate, float eyeSign, float slider3D, bool inKiai, float bpmPulse,
                         int combo = 0, int score = 0, float hp = 1.0f, float acc = 100.0f,
                         const char* songTitle = nullptr, const char* diffName = nullptr,
                         int bgMode = 0);
    void renderBottomDrum(C2D_TextBuf textBuf, float animTick, const GameSettings* pSettings = nullptr, float guideAlpha = 0.0f);
};
