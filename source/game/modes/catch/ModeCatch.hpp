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

enum CatchFruitType {
    FRUIT_APPLE = 0,       // Manzana roja (300 pts, +1 combo)
    FRUIT_ORANGE = 1,      // Naranja (300 pts, +1 combo)
    FRUIT_GRAPE = 2,       // Uvas violetas (300 pts, +1 combo)
    FRUIT_STRAWBERRY = 3,  // Fresa (300 pts, +1 combo)
    FRUIT_DROPLET = 4,     // Gota de jugo mediana (100 pts, +1 combo)
    FRUIT_TINY_DROPLET = 5,// Gota pequeña de recorrido (10 pts, no rompe combo)
    FRUIT_BANANA = 6       // Plátano de lluvia / Spinner (+1100 pts bonus)
};

struct CatchFruit {
    CatchFruitType type = FRUIT_APPLE;
    float x = 256.0f;        // Coordenada horizontal osu! (0 .. 512)
    int time = 0;            // Tiempo en ms
    bool isHyper = false;    // True si es un Hyperfruit (genera aceleración extrema)
    float hyperTargetX = 0.0f; // Destino de la siguiente fruta
    HitState state = NOT_HIT;
    bool isCaught = false;
    int colorIndex = 0;
};

struct PlateFruit {
    float relX = 0.0f;       // Desplazamiento respecto al centro del plato
    float relY = 0.0f;       // Altura en la pila de frutas
    CatchFruitType type = FRUIT_APPLE;
    PlateFruit(float _rx = 0.0f, float _ry = 0.0f, CatchFruitType _t = FRUIT_APPLE)
        : relX(_rx), relY(_ry), type(_t) {}
};

struct CatcherGhost {
    float x = 256.0f;
    float alpha = 1.0f;
    bool isHyper = false;
    CatcherGhost(float _x = 256.0f, float _a = 1.0f, bool _h = false)
        : x(_x), alpha(_a), isHyper(_h) {}
};

struct CatchSplash {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float size = 3.0f;
    u32 color = 0xFFFFFFFF;
    float life = 1.0f;

    CatchSplash(float _x = 0.0f, float _y = 0.0f, float _vx = 0.0f, float _vy = 0.0f, float _s = 3.0f, u32 _c = 0xFFFFFFFF, float _l = 1.0f)
        : x(_x), y(_y), vx(_vx), vy(_vy), size(_s), color(_c), life(_l) {}
};

struct CatchPopup {
    float x = 0.0f;
    float y = 0.0f;
    const char* text = "+300";
    u32 color = 0xFFFFFFFF;
    float life = 1.0f;

    CatchPopup(float _x = 0.0f, float _y = 0.0f, const char* _txt = "+300", u32 _c = 0xFFFFFFFF, float _l = 1.0f)
        : x(_x), y(_y), text(_txt), color(_c), life(_l) {}
};

class ModeCatch {
public:
    std::vector<CatchFruit> fruits;
    size_t nextFruitIndex = 0;

    // Estado del Catcher (Personaje y plato)
    float catcherX = 256.0f;          // Posición horizontal (0 .. 512)
    float catcherPlateWidth = 64.0f;  // Ancho del plato según Circle Size (CS)
    int facingDir = 1;                // 1: Derecha, -1: Izquierda
    bool isDashing = false;
    bool isHyperDashing = false;
    float hyperTargetX = 256.0f;
    float hyperDashTimer = 0.0f;

    // Efectos de partículas de captura (Juice Splash) y rebote del plato
    std::vector<CatchSplash> splashes;
    std::vector<CatchPopup> popups;
    float plateBounce = 0.0f;

    // Pila de frutas recogidas sobre el plato
    std::vector<PlateFruit> plateFruits;
    std::vector<CatcherGhost> ghosts;

    // Parámetros de física de Catch
    static constexpr float BASE_WALK_SPEED = 0.54f; // Unidades osu por ms (~540 u/s)
    static constexpr float DASH_FACTOR     = 2.0f;  // 2x velocidad al acelerar
    static constexpr float HYPER_FACTOR    = 3.8f;  // 3.8x velocidad al atrapar Hyperfruit

    // Geometría del Playfield en Pantalla Superior (400x240)
    static constexpr float PLAYFIELD_X = 40.0f;   // Margen izquierdo
    static constexpr float PLAYFIELD_W = 320.0f;  // Ancho del área jugable
    static constexpr float SCALE_X     = 320.0f / 512.0f; // 0.625f
    static constexpr float PLATE_Y     = 206.0f;  // Altura del plato receptor
    static constexpr float BASE_APPROACH = 680.0f; // Tiempo de caída visible en ms

    // Colores oficiales de frutas
    static constexpr u32 CLR_APPLE      = 0xFF3333EE; // Rojo brillante
    static constexpr u32 CLR_ORANGE     = 0xFF1199FF; // Naranja cítrico
    static constexpr u32 CLR_GRAPE      = 0xFFDD33AA; // Violeta / Púrpura
    static constexpr u32 CLR_STRAWBERRY = 0xFF5533FF; // Fresa carmesí
    static constexpr u32 CLR_BANANA     = 0xFF22EEFF; // Amarillo dorado
    static constexpr u32 CLR_DROPLET    = 0xFFFFAA00; // Cian agua
    static constexpr u32 CLR_HYPER_GLOW = 0xFF2222FF; // Resplandor rojo Hyper
    static constexpr u32 CLR_WHITE      = 0xFFFFFFFF;

    inline float toScreenX(float osuX) const {
        return PLAYFIELD_X + (osuX * SCALE_X);
    }

    inline float toOsuX(float screenX) const {
        return (screenX - PLAYFIELD_X) / SCALE_X;
    }

private:
    static void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f);
    void renderFruit(float x, float y, CatchFruitType type, bool isHyper, float z, const struct AppContext* pCtx = nullptr);
    void renderFruitMini(float x, float y, CatchFruitType type, float z);
    void renderCatcherChibi(float cx, float cy, int dir, bool dashing, float z);

public:
    // Lógica de juego y evaluación (ModeCatch.cpp)
    void reset();
    void initFromBeatmap(const Beatmap& beatmap);
    void update(float dtMs, float moveInput, bool dashPressed, float analogX, int songTime, float speedRate);
    int checkCatches(int songTime, float hit50, int& outScoreAdd, int& outComboAdd, float& outHpAdd);
    void handleTouch(float px, float py, bool isDown);

    // Renderizado visual (CatchRenderer.cpp)
    void renderTopScreen(C2D_TextBuf textBuf, int songTime, float speedRate, float eyeSign, float slider3DVal, bool inKiai, const struct AppContext* pCtx = nullptr);
    void renderBottomDeck(C2D_TextBuf textBuf, float animTick, int songTime, const struct AppContext* pCtx = nullptr);
};
