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

struct AppContext;

struct ManiaNote {
    int column = 0;          // Columna 0 .. 3 (4K)
    int time = 0;            // Tiempo de inicio en ms
    int endTime = 0;         // Tiempo de fin en ms (para Hold Notes / Long Notes)
    bool isHold = false;     // True si es una nota larga (LN)
    HitState headState = NOT_HIT;
    HitState tailState = NOT_HIT;
    bool isHolding = false;  // True mientras el jugador mantenga presionada la tecla
    int lastTickTime = 0;    // Para conteo de ticks intermedios durante el hold
    bool headJudged = false; // Si ya se evaluó el inicio
    bool fullyCompleted = false; // Si la nota terminó su ciclo
    int hitSound = 0;
};

struct ManiaParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float size = 3.0f;
    u32 color = 0xFFFFFFFF;
    float life = 1.0f;

    ManiaParticle(float _x = 0.0f, float _y = 0.0f, float _vx = 0.0f, float _vy = 0.0f, float _s = 3.0f, u32 _c = 0xFFFFFFFF, float _l = 1.0f)
        : x(_x), y(_y), vx(_vx), vy(_vy), size(_s), color(_c), life(_l) {}
};

class ModeMania {
public:
    std::vector<ManiaNote> notes;
    size_t nextNoteIndex = 0;

    // Estado de las 4 columnas (0 .. 3)
    bool colPressed[4] = { false, false, false, false };
    float colLightAlpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float colHitGlow[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int activeHoldNoteIdx[4] = { -1, -1, -1, -1 };

    // Efectos de partículas al pulsar y conectar notas
    std::vector<ManiaParticle> particles;

    // Velocidad de scroll ajustable al vuelo (Scroll Speed: 0.5x .. 3.5x)
    float userScrollSpeed = 1.0f;
    float scrollOsdTimer = 0.0f;

    // Seguimiento de entrada táctil en pantalla inferior
    int lastTouchCol = -1;

    // Constantes de diseño del playfield de Mania
    static constexpr float TRACK_X = 124.0f;     // Centrado en pantalla superior (400px): 124 .. 276
    static constexpr float COL_WIDTH = 38.0f;    // Ancho de cada columna: 4 * 38 = 152 px
    static constexpr float HIT_Y = 202.0f;       // Línea de impacto (Receptors)
    static constexpr float RECEPTOR_H = 14.0f;
    static constexpr float BASE_APPROACH = 500.0f; // Tiempo de caída en ms a velocidad 1.0x

    // Colores oficiales 4K estilo arcade
    // Columnas exteriores (0 y 3): Cian / Azul neón
    static constexpr u32 CLR_COL_OUTER = 0xFFFFAA00;       // Cian ABGR (R: 0, G: 170, B: 255)
    static constexpr u32 CLR_COL_OUTER_GLOW = 0x88FFAA00;
    // Columnas interiores (1 y 2): Dorado / Ámbar brillante
    static constexpr u32 CLR_COL_INNER = 0xFF22DDFF;       // Dorado ABGR (R: 255, G: 221, B: 34)
    static constexpr u32 CLR_COL_INNER_GLOW = 0x8822DDFF;
    static constexpr u32 CLR_WHITE = 0xFFFFFFFF;

private:
    static void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f);
    static void drawRectOutline(float x, float y, float w, float h, float thickness, u32 color, float z = 0.5f);
    static void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24, float z = 0.5f);

public:
    // Lógica de juego y evaluación (ModeMania.cpp)
    void adjustScrollSpeed(float delta);
    void reset();
    void initFromBeatmap(const Beatmap& beatmap);
    void update(float deltaTime = 0.016f);
    void spawnHitParticles(int col, u32 baseClr);
    int checkMisses(int songTime, float hit50);
    int processKeyPress(int col, int songTime, float hit300, float hit100, float hit50);
    int processKeyRelease(int col, int songTime, float hit300, float hit100, float hit50);
    int updateActiveHolds(int songTime, int& tickHits);
    int handleTouch(float px, float py, bool isDown);

    // Renderizado visual (ManiaRenderer.cpp)
    void renderTopScreen(C2D_TextBuf textBuf, int songTime, float speedRate, float eyeSign, float slider3DVal, bool inKiai, float bpmPulse, const struct AppContext* pCtx = nullptr);
    void renderBottomLanes(C2D_TextBuf textBuf, float animTick, const struct AppContext* pCtx = nullptr);
};
