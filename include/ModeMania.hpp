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

    void adjustScrollSpeed(float delta) {
        userScrollSpeed = std::max(0.5f, std::min(3.5f, userScrollSpeed + delta));
        scrollOsdTimer = 1.8f;
    }

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
    static void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f) {
        C2D_Text text;
        C2D_TextParse(&text, buf, str);
        C2D_TextOptimize(&text);
        float w = 0.0f, h = 0.0f;
        C2D_TextGetDimensions(&text, scale, scale, &w, &h);
        C2D_DrawText(&text, C2D_WithColor, cx - (w * 0.5f), cy - (h * 0.5f), z, scale, scale, color);
    }

public:
    void reset() {
        notes.clear();
        nextNoteIndex = 0;
        particles.clear();
        scrollOsdTimer = 0.0f;
        for (int i = 0; i < 4; ++i) {
            colPressed[i] = false;
            colLightAlpha[i] = 0.0f;
            colHitGlow[i] = 0.0f;
            activeHoldNoteIdx[i] = -1;
        }
        lastTouchCol = -1;
    }

    // Inicializa y convierte cualquier beatmap a 4K Mania
    void initFromBeatmap(const Beatmap& beatmap) {
        reset();
        notes.reserve(beatmap.circles.size() * 2);

        bool isNativeMania = (beatmap.mode == 3);

        for (const auto& c : beatmap.circles) {
            int col = 0;
            if (isNativeMania) {
                // Mapeo oficial osu!mania: col = floor(x * 4 / 512)
                col = std::max(0, std::min(3, static_cast<int>(std::floor(c.x * 4.0f / 512.0f))));
            } else {
                // Conversión de osu!standard a Mania 4K
                // Mapea la posición X de standard (0..512) a 4 columnas
                col = std::max(0, std::min(3, static_cast<int>(std::floor(c.x * 4.0f / 512.0f))));
            }

            ManiaNote mn;
            mn.column = col;
            mn.time = c.time;
            mn.endTime = c.endTime;
            mn.hitSound = c.hitSound;
            mn.headState = NOT_HIT;
            mn.tailState = NOT_HIT;
            mn.isHolding = false;
            mn.lastTickTime = c.time;
            mn.headJudged = false;
            mn.fullyCompleted = false;

            // Determinar si es una Hold Note (LN)
            if (c.isHold) {
                mn.isHold = true;
                mn.endTime = std::max(mn.time + 60, c.endTime);
            } else if (c.isSlider && (c.endTime - c.time >= 120)) {
                // En osu! standard, los sliders se convierten en Hold Notes
                mn.isHold = true;
                mn.endTime = c.endTime;
            } else if (c.isSpinner && (c.endTime - c.time >= 250)) {
                mn.isHold = true;
                mn.endTime = c.endTime;
            } else {
                mn.isHold = false;
                mn.endTime = c.time;
            }

            notes.push_back(mn);
        }

        // Ordenar notas cronológicamente
        std::sort(notes.begin(), notes.end(), [](const ManiaNote& a, const ManiaNote& b) {
            if (a.time != b.time) return a.time < b.time;
            return a.column < b.column;
        });
    }

    // Actualiza animaciones de receptor y destellos
    void update(float deltaTime = 0.016f) {
        if (scrollOsdTimer > 0.0f) {
            scrollOsdTimer = std::max(0.0f, scrollOsdTimer - deltaTime);
        }
        for (int i = 0; i < 4; ++i) {
            if (!colPressed[i]) {
                if (colLightAlpha[i] > 0.0f) {
                    colLightAlpha[i] = std::max(0.0f, colLightAlpha[i] - deltaTime * 4.5f);
                }
            } else {
                colLightAlpha[i] = 1.0f;
            }
            if (colHitGlow[i] > 0.0f) {
                colHitGlow[i] = std::max(0.0f, colHitGlow[i] - deltaTime * 5.0f);
            }
        }

        // Actualizar partículas de impacto de notas
        for (auto& p : particles) {
            p.x += p.vx;
            p.y += p.vy;
            p.vy += 0.08f;
            p.life -= deltaTime * 3.2f;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const ManiaParticle& p) {
            return p.life <= 0.0f;
        }), particles.end());
    }

    void spawnHitParticles(int col, u32 baseClr) {
        float cx = TRACK_X + col * COL_WIDTH + (COL_WIDTH * 0.5f);
        for (int k = 0; k < 8; ++k) {
            float ang = -static_cast<float>(M_PI) * (0.18f + 0.64f * (static_cast<float>(std::rand() % 100) / 100.0f));
            float spd = 1.5f + static_cast<float>(std::rand() % 100) / 32.0f;
            particles.push_back({
                cx + (static_cast<float>(std::rand() % 18) - 9.0f),
                HIT_Y - 2.0f,
                std::cos(ang) * spd,
                std::sin(ang) * spd,
                2.2f + static_cast<float>(std::rand() % 10) / 5.0f,
                (k % 2 == 0) ? CLR_WHITE : baseClr,
                1.0f
            });
        }
    }

    // Revisa notas pasadas sin tocar (Miss)
    int checkMisses(int songTime, float hit50) {
        int misses = 0;
        for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
            auto& n = notes[i];
            if (n.fullyCompleted) continue;

            if (!n.headJudged && songTime > (n.time + static_cast<int>(hit50))) {
                n.headJudged = true;
                n.headState = HIT_MISS;
                if (!n.isHold) {
                    n.fullyCompleted = true;
                } else {
                    // Si falló el inicio de una LN, cancela el hold
                    n.isHolding = false;
                    n.tailState = HIT_MISS;
                    if (songTime > n.endTime + static_cast<int>(hit50)) {
                        n.fullyCompleted = true;
                    }
                }
                misses++;
            }

            // Si es LN y se pasó el endTime completamente
            if (n.isHold && !n.fullyCompleted && songTime > (n.endTime + static_cast<int>(hit50))) {
                if (n.tailState == NOT_HIT) {
                    n.tailState = HIT_MISS;
                    misses++;
                }
                n.fullyCompleted = true;
            }

            if (n.time > songTime + static_cast<int>(hit50)) {
                break;
            }
        }

        // Avanzar nextNoteIndex sobre notas completamente terminadas
        while (nextNoteIndex < notes.size() && notes[nextNoteIndex].fullyCompleted) {
            nextNoteIndex++;
        }

        return misses;
    }

    // Procesa presión de tecla en columna (0 .. 3)
    int processKeyPress(int col, int songTime, float hit300, float hit100, float hit50) {
        if (col < 0 || col > 3) return 0;
        colPressed[col] = true;
        colLightAlpha[col] = 1.0f;

        // Buscar la nota no juzgada más cercana en esta columna
        for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
            auto& n = notes[i];
            if (n.column != col || n.headJudged || n.fullyCompleted) continue;

            int dt = songTime - n.time;
            int absDt = std::abs(dt);

            if (absDt <= static_cast<int>(hit50)) {
                n.headJudged = true;
                colHitGlow[col] = 1.0f;
                bool isOuter = (col == 0 || col == 3);
                u32 colClr = isOuter ? CLR_COL_OUTER : CLR_COL_INNER;
                spawnHitParticles(col, colClr);

                if (absDt <= static_cast<int>(hit300)) {
                    n.headState = HIT_300;
                    if (n.isHold) {
                        n.isHolding = true;
                        n.lastTickTime = songTime;
                        activeHoldNoteIdx[col] = static_cast<int>(i);
                    } else {
                        n.fullyCompleted = true;
                    }
                    return 300;
                } else if (absDt <= static_cast<int>(hit100)) {
                    n.headState = HIT_100;
                    if (n.isHold) {
                        n.isHolding = true;
                        n.lastTickTime = songTime;
                        activeHoldNoteIdx[col] = static_cast<int>(i);
                    } else {
                        n.fullyCompleted = true;
                    }
                    return 100;
                } else {
                    n.headState = HIT_50;
                    if (n.isHold) {
                        n.isHolding = true;
                        n.lastTickTime = songTime;
                        activeHoldNoteIdx[col] = static_cast<int>(i);
                    } else {
                        n.fullyCompleted = true;
                    }
                    return 50;
                }
            } else if (dt < 0 && absDt > static_cast<int>(hit50 * 1.6f)) {
                // Nota demasiado lejana aún en el futuro, no consumir
                break;
            }
        }

        return 0;
    }

    // Procesa liberación de tecla en columna (0 .. 3)
    int processKeyRelease(int col, int songTime, float hit300, float hit100, float hit50) {
        if (col < 0 || col > 3) return 0;
        colPressed[col] = false;

        int activeIdx = activeHoldNoteIdx[col];
        if (activeIdx >= 0 && static_cast<size_t>(activeIdx) < notes.size()) {
            auto& n = notes[activeIdx];
            if (n.isHold && n.isHolding) {
                n.isHolding = false;
                activeHoldNoteIdx[col] = -1;

                int dt = songTime - n.endTime;
                int absDt = std::abs(dt);

                if (absDt <= static_cast<int>(hit300)) {
                    n.tailState = HIT_300;
                    n.fullyCompleted = true;
                    colHitGlow[col] = 1.0f;
                    return 300;
                } else if (absDt <= static_cast<int>(hit100)) {
                    n.tailState = HIT_100;
                    n.fullyCompleted = true;
                    colHitGlow[col] = 1.0f;
                    return 100;
                } else if (absDt <= static_cast<int>(hit50)) {
                    n.tailState = HIT_50;
                    n.fullyCompleted = true;
                    return 50;
                } else if (songTime < n.endTime - static_cast<int>(hit50)) {
                    // Soltó demasiado temprano la Hold Note -> Rompe el hold (Miss)
                    n.tailState = HIT_MISS;
                    n.fullyCompleted = true;
                    return -1;
                } else {
                    n.tailState = HIT_50;
                    n.fullyCompleted = true;
                    return 50;
                }
            }
        }
        activeHoldNoteIdx[col] = -1;
        return 0;
    }

    // Actualiza notas largas (LN) que están activamente sostenidas y otorga ticks intermedios
    int updateActiveHolds(int songTime, int& tickHits) {
        tickHits = 0;
        for (int c = 0; c < 4; ++c) {
            int activeIdx = activeHoldNoteIdx[c];
            if (activeIdx >= 0 && static_cast<size_t>(activeIdx) < notes.size()) {
                auto& n = notes[activeIdx];
                if (n.isHold && n.isHolding) {
                    // Generar tick intermedio cada 120ms
                    if (songTime - n.lastTickTime >= 120 && songTime < n.endTime) {
                        n.lastTickTime = songTime;
                        tickHits++;
                        colHitGlow[c] = 0.5f;
                    }
                    // Si ya se alcanzó o pasó el endTime mientras se mantiene presionada
                    if (songTime >= n.endTime) {
                        n.tailState = HIT_300;
                        n.isHolding = false;
                        n.fullyCompleted = true;
                        activeHoldNoteIdx[c] = -1;
                        colHitGlow[c] = 1.0f;
                        return 300;
                    }
                }
            }
        }
        return 0;
    }

    // Renderizado en Pantalla Superior (400x240) con soporte Estereoscópico 3D
    void renderTopScreen(C2D_TextBuf textBuf, int songTime, float speedRate, float eyeSign, float slider3DVal, bool inKiai, float bpmPulse) {
        auto depthShift = [&](float px) { return eyeSign * slider3DVal * px; };
        float dTrack = depthShift(-1.0f);
        float dReceptor = depthShift(3.0f);
        float trackX = TRACK_X + dTrack;
        float trackW = COL_WIDTH * 4.0f; // 152 px

        float approachTime = BASE_APPROACH / (userScrollSpeed * std::max(0.5f, speedRate));

        // 1. Fondo de la pista (Track)
        u32 trackBgTop = inKiai ? Ui::rgba(28, 14, 40, 235) : Ui::rgba(12, 10, 22, 235);
        u32 trackBgBot = inKiai ? Ui::rgba(38, 18, 55, 245) : Ui::rgba(18, 14, 30, 245);
        Ui::gradV(trackX, 0.0f, trackW, 240.0f, 0.15f, trackBgTop, trackBgBot);

        // 2. Líneas divisorias verticales de las columnas
        u32 borderClr = Ui::rgba(85, 75, 120, 180);
        u32 divClr = Ui::rgba(60, 50, 90, 110);
        C2D_DrawLine(trackX, 0.0f, borderClr, trackX, 240.0f, borderClr, 2.0f, 0.18f);
        C2D_DrawLine(trackX + trackW, 0.0f, borderClr, trackX + trackW, 240.0f, borderClr, 2.0f, 0.18f);
        for (int i = 1; i < 4; ++i) {
            float lx = trackX + i * COL_WIDTH;
            C2D_DrawLine(lx, 0.0f, divClr, lx, 240.0f, divClr, 1.0f, 0.18f);
        }

        // 3. Destellos de presión e iluminación de columna (Lane Flash & Laser Beam)
        for (int i = 0; i < 4; ++i) {
            if (colLightAlpha[i] > 0.01f || colHitGlow[i] > 0.01f) {
                float lx = trackX + i * COL_WIDTH;
                bool isOuter = (i == 0 || i == 3);
                u32 colClr = isOuter ? CLR_COL_OUTER : CLR_COL_INNER;
                u8 aTop = static_cast<u8>(colHitGlow[i] * 95.0f);
                u8 aBot = static_cast<u8>(std::min(255.0f, colLightAlpha[i] * 130.0f + colHitGlow[i] * 125.0f));
                Ui::gradV(lx + 1.0f, 0.0f, COL_WIDTH - 2.0f, HIT_Y, 0.20f,
                          Ui::withAlpha(colClr, aTop), Ui::withAlpha(colClr, aBot));
                if (colHitGlow[i] > 0.12f) {
                    u8 coreA = static_cast<u8>(colHitGlow[i] * 210.0f);
                    C2D_DrawRectSolid(lx + (COL_WIDTH * 0.5f) - 1.5f, 0.0f, 0.22f, 3.0f, HIT_Y, Ui::rgba(255, 255, 255, coreA));
                }
            }
        }

        // Función de proyección estereoscópica para notas (Paralaje dinámico según altura Y)
        auto getNoteX = [&](int col, float y) {
            float yRatio = std::max(0.0f, std::min(1.0f, y / HIT_Y));
            float pDepth = -2.2f + yRatio * 5.2f; // -2.2f en el fondo arriba, a +3.0f en receptor al frente
            return TRACK_X + depthShift(pDepth) + col * COL_WIDTH;
        };

        // 4. Renderizado de Notas (Hold Notes y Tap Notes)
        for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
            auto& n = notes[i];
            if (n.fullyCompleted) continue;

            // Comprobar si está dentro de la ventana visible
            if (n.time - songTime > static_cast<int>(approachTime)) {
                // Demasiado arriba para verse aún
                break;
            }

            float noteW = COL_WIDTH - 4.0f;
            bool isOuter = (n.column == 0 || n.column == 3);
            u32 mainClr = isOuter ? CLR_COL_OUTER : CLR_COL_INNER;

            if (n.isHold) {
                // Cálculo de posiciones de Cabeza y Cola
                float dtHead = static_cast<float>(n.time - songTime);
                float yHead = HIT_Y - (dtHead / approachTime) * HIT_Y;
                float dtTail = static_cast<float>(n.endTime - songTime);
                float yTail = HIT_Y - (dtTail / approachTime) * HIT_Y;

                // Si se está sosteniendo, la cabeza permanece en la línea de receptor
                if (n.isHolding) {
                    yHead = HIT_Y;
                }

                float clampedYTail = std::max(0.0f, yTail);
                float clampedYHead = std::min(HIT_Y, yHead);

                if (clampedYHead > clampedYTail) {
                    float noteXHead = getNoteX(n.column, clampedYHead) + 2.0f;
                    float noteXTail = getNoteX(n.column, clampedYTail) + 2.0f;

                    // Cuerpo de la Hold Note (cuerpo translúcido con brillo central)
                    u8 bodyAlpha = n.isHolding ? 190 : 130;
                    u32 bodyClr = Ui::withAlpha(mainClr, bodyAlpha);

                    C2D_DrawTriangle(noteXTail + 3.0f, clampedYTail, bodyClr,
                                     noteXTail + noteW - 3.0f, clampedYTail, bodyClr,
                                     noteXHead + 3.0f, clampedYHead, bodyClr, 0.35f);
                    C2D_DrawTriangle(noteXTail + noteW - 3.0f, clampedYTail, bodyClr,
                                     noteXHead + noteW - 3.0f, clampedYHead, bodyClr,
                                     noteXHead + 3.0f, clampedYHead, bodyClr, 0.35f);

                    // Línea de energía central brillante
                    u32 spineClr = Ui::withAlpha(CLR_WHITE, n.isHolding ? 240 : 180);
                    float midTail = noteXTail + (noteW * 0.5f);
                    float midHead = noteXHead + (noteW * 0.5f);
                    C2D_DrawLine(midTail, clampedYTail, spineClr, midHead, clampedYHead, spineClr, 3.0f, 0.37f);

                    // Bordes laterales de la nota sostenida
                    u32 edgeClr = Ui::withAlpha(mainClr, 220);
                    C2D_DrawLine(noteXTail + 3.0f, clampedYTail, edgeClr, noteXHead + 3.0f, clampedYHead, edgeClr, 1.5f, 0.38f);
                    C2D_DrawLine(noteXTail + noteW - 3.0f, clampedYTail, edgeClr, noteXHead + noteW - 3.0f, clampedYHead, edgeClr, 1.5f, 0.38f);
                }

                // Cola de la Hold Note (Barra final)
                if (yTail >= -10.0f && yTail <= HIT_Y) {
                    float noteXTail = getNoteX(n.column, yTail) + 2.0f;
                    Ui::pill(noteXTail + 2.0f, yTail - 4.0f, noteW - 4.0f, 7.0f, 0.40f, Ui::brighten(mainClr, 0.4f));
                    C2D_DrawRectSolid(noteXTail + 4.0f, yTail - 2.0f, 0.42f, noteW - 8.0f, 3.0f, CLR_WHITE);
                }

                // Cabeza de la Hold Note (si aún no se ha golpeado)
                if (!n.headJudged && yHead >= -10.0f && yHead <= 240.0f) {
                    float noteXHead = getNoteX(n.column, yHead) + 2.0f;
                    Ui::panel(noteXHead, yHead - 5.0f, noteW, 11.0f, 0.45f, mainClr, CLR_WHITE);
                    C2D_DrawRectSolid(noteXHead + 2.0f, yHead - 2.0f, 0.47f, noteW - 4.0f, 3.5f, CLR_WHITE);
                }
            } else {
                // Nota simple (Tap Note)
                float dt = static_cast<float>(n.time - songTime);
                float noteY = HIT_Y - (dt / approachTime) * HIT_Y;

                if (noteY >= -12.0f && noteY <= 240.0f && !n.headJudged) {
                    float noteX = getNoteX(n.column, noteY) + 2.0f;
                    // Cuerpo de nota con relieve redondeado
                    Ui::panel(noteX, noteY - 5.0f, noteW, 11.0f, 0.45f, mainClr, CLR_WHITE);
                    // Brillo interior superior blanco
                    C2D_DrawRectSolid(noteX + 2.0f, noteY - 3.0f, 0.47f, noteW - 4.0f, 3.5f, CLR_WHITE);
                }
            }
        }

        // 5. Receptores (Hit Line & Targets)
        float rTrackX = TRACK_X + dReceptor;
        C2D_DrawRectSolid(rTrackX, HIT_Y - 7.0f, 0.28f, trackW, RECEPTOR_H, Ui::rgba(10, 8, 16, 230));

        // Línea de impacto luminosa
        u32 hitLineClr = inKiai ? Ui::rgba(255, 120, 200, 240) : Ui::rgba(220, 220, 255, 220);
        C2D_DrawLine(rTrackX, HIT_Y, hitLineClr, rTrackX + trackW, HIT_Y, hitLineClr, 2.5f, 0.50f);

        // Cajas de receptor para cada una de las 4 columnas
        for (int i = 0; i < 4; ++i) {
            float rx = rTrackX + i * COL_WIDTH;
            bool isOuter = (i == 0 || i == 3);
            u32 rClr = isOuter ? CLR_COL_OUTER : CLR_COL_INNER;

            // Borde del receptor
            drawRectOutline(rx + 2.0f, HIT_Y - 6.0f, COL_WIDTH - 4.0f, RECEPTOR_H, 1.2f, Ui::withAlpha(rClr, 160), 0.52f);

            // Explosión de impacto al presionar/acertar y onda expansiva
            if (colHitGlow[i] > 0.01f) {
                u8 gAlpha = static_cast<u8>(colHitGlow[i] * 200.0f);
                u32 glowClr = Ui::withAlpha(rClr, gAlpha);
                Ui::panel(rx + 1.0f, HIT_Y - 8.0f, COL_WIDTH - 2.0f, RECEPTOR_H + 4.0f, 0.55f, glowClr, CLR_WHITE);
                C2D_DrawRectSolid(rx + 4.0f, HIT_Y - 2.0f, 0.58f, COL_WIDTH - 8.0f, 4.0f, CLR_WHITE);

                // Anillo de choque expansivo
                float swR = (1.0f - colHitGlow[i]) * 18.0f + 6.0f;
                u8 swA = static_cast<u8>(colHitGlow[i] * 190.0f);
                drawRing(rx + (COL_WIDTH * 0.5f), HIT_Y, swR, 1.8f, Ui::withAlpha(CLR_WHITE, swA), 16, 0.56f);
            } else if (colPressed[i]) {
                Ui::panel(rx + 2.0f, HIT_Y - 6.0f, COL_WIDTH - 4.0f, RECEPTOR_H, 0.53f, Ui::withAlpha(rClr, 140), CLR_WHITE);
            }
        }

        // Partículas luminosas de impacto que estallan hacia arriba
        float dSparks = depthShift(4.2f);
        for (const auto& p : particles) {
            u8 pA = static_cast<u8>(std::max(0.0f, std::min(1.0f, p.life)) * 255.0f);
            C2D_DrawCircleSolid(p.x + dSparks, p.y, 0.57f, p.size, Ui::withAlpha(p.color, pA));
            C2D_DrawCircleSolid(p.x + dSparks, p.y, 0.58f, p.size * 0.45f, Ui::withAlpha(CLR_WHITE, pA));
        }

        // 6. Indicadores de columna en la base del receptor (1, 2, 3, 4)
        const char* colLabels[4] = { "1", "2", "3", "4" };
        for (int i = 0; i < 4; ++i) {
            float cx = rTrackX + i * COL_WIDTH + (COL_WIDTH * 0.5f);
            drawCenteredText(textBuf, cx, HIT_Y + 16.0f, 0.32f, Ui::rgba(170, 160, 200), colLabels[i], 0.60f);
        }

        // 7. OSD flotante de velocidad de scroll al ajustarla en vivo
        if (scrollOsdTimer > 0.01f) {
            float osdAlpha = std::min(1.0f, scrollOsdTimer * 2.0f);
            u8 oA = static_cast<u8>(osdAlpha * 240.0f);
            float osdX = 200.0f + depthShift(4.5f);
            float osdY = 22.0f;
            Ui::pill(osdX - 68.0f, osdY - 10.0f, 136.0f, 20.0f, 0.88f, Ui::rgba(20, 16, 32, oA));
            drawRectOutline(osdX - 68.0f, osdY - 10.0f, 136.0f, 20.0f, 1.2f, Ui::rgba(255, 215, 60, oA), 0.89f);
            char osdBuf[32];
            std::snprintf(osdBuf, sizeof(osdBuf), "SPEED: %.1fx  [^/v]", userScrollSpeed);
            drawCenteredText(textBuf, osdX, osdY, 0.35f, Ui::rgba(255, 230, 80, oA), osdBuf, 0.90f);
        }
    }

    // Renderizado en Pantalla Inferior (320x240): Teclado táctil interactivo de 4 columnas
    void renderBottomLanes(C2D_TextBuf textBuf, float animTick) {
        constexpr float LANE_W = 80.0f; // 320 / 4 = 80 px por carril

        for (int i = 0; i < 4; ++i) {
            float lx = i * LANE_W;
            bool isOuter = (i == 0 || i == 3);
            u32 colClr = isOuter ? CLR_COL_OUTER : CLR_COL_INNER;

            // Fondo del carril táctil
            u32 bgTop = Ui::rgba(18, 14, 28, 240);
            u32 bgBot = Ui::rgba(26, 20, 40, 240);

            if (colPressed[i]) {
                // Iluminación reactiva al tocar o presionar botón
                bgTop = Ui::lerpColor(bgTop, Ui::withAlpha(colClr, 160), 0.35f);
                bgBot = Ui::lerpColor(bgBot, Ui::withAlpha(colClr, 220), 0.60f);
            }

            Ui::gradV(lx, 0.0f, LANE_W, 240.0f, 0.10f, bgTop, bgBot);

            // Línea divisoria derecha
            if (i < 3) {
                C2D_DrawLine(lx + LANE_W, 0.0f, Ui::rgba(55, 45, 75, 160), lx + LANE_W, 240.0f, Ui::rgba(55, 45, 75, 160), 1.0f, 0.15f);
            }

            // Destello brillante en la base cuando se presiona
            if (colPressed[i]) {
                float wavePulse = 0.5f + 0.5f * std::sin(animTick * 0.3f);
                u8 glowA = static_cast<u8>(140 + 80 * wavePulse);
                Ui::gradV(lx + 2.0f, 130.0f, LANE_W - 4.0f, 110.0f, 0.22f, Ui::rgba(0, 0, 0, 0), Ui::withAlpha(colClr, glowA));
                C2D_DrawRectSolid(lx + 4.0f, 234.0f, 0.30f, LANE_W - 8.0f, 4.0f, CLR_WHITE);
            }

            // Icono / Tecla física sugerida en la parte inferior
            const char* keyHint = "";
            const char* keySec = "";
            if (i == 0)      { keyHint = "L"; keySec = "D-PAD <"; }
            else if (i == 1) { keyHint = "^"; keySec = "D-PAD v"; }
            else if (i == 2) { keyHint = "X"; keySec = "BOTON Y"; }
            else if (i == 3) { keyHint = "A"; keySec = "B / R"; }

            // Placa decorativa de la columna
            float badgeY = 120.0f;
            u32 pillClr = colPressed[i] ? Ui::brighten(colClr, 0.4f) : Ui::rgba(40, 32, 60, 200);
            Ui::pill(lx + 15.0f, badgeY, LANE_W - 30.0f, 26.0f, 0.25f, pillClr);
            drawCenteredText(textBuf, lx + (LANE_W * 0.5f), badgeY + 13.0f, 0.48f, colPressed[i] ? CLR_WHITE : colClr, keyHint, 0.35f);

            drawCenteredText(textBuf, lx + (LANE_W * 0.5f), badgeY + 34.0f, 0.26f, Ui::rgba(180, 170, 210), keySec, 0.35f);
        }

        // Barra de ajuste rápido de velocidad táctil en la parte superior
        Ui::pill(10.0f, 6.0f, 75.0f, 22.0f, 0.28f, Ui::rgba(32, 24, 48, 230));
        drawCenteredText(textBuf, 47.5f, 17.0f, 0.33f, Ui::rgba(255, 120, 150), "[-] VEL", 0.32f);

        char curSpd[32];
        std::snprintf(curSpd, sizeof(curSpd), "VEL: %.1fx", userScrollSpeed);
        Ui::pill(110.0f, 6.0f, 100.0f, 22.0f, 0.28f, Ui::rgba(24, 18, 38, 240));
        drawCenteredText(textBuf, 160.0f, 17.0f, 0.34f, Ui::rgba(255, 215, 60), curSpd, 0.32f);

        Ui::pill(235.0f, 6.0f, 75.0f, 22.0f, 0.28f, Ui::rgba(32, 24, 48, 230));
        drawCenteredText(textBuf, 272.5f, 17.0f, 0.33f, Ui::rgba(80, 220, 160), "VEL [+]", 0.32f);
    }

    // Procesa interacción táctil directa en pantalla inferior
    // Retorna la columna tocada (0..3), o -1 si no aplica
    int handleTouch(float px, float py, bool isDown) {
        if (!isDown) {
            lastTouchCol = -1;
            return -1;
        }

        // Zona táctil de ajuste de velocidad en la parte superior
        if (py >= 4.0f && py <= 32.0f) {
            if (px >= 8.0f && px <= 90.0f) {
                adjustScrollSpeed(-0.1f);
                return -1;
            } else if (px >= 230.0f && px <= 312.0f) {
                adjustScrollSpeed(+0.1f);
                return -1;
            }
        }

        // Zona de botón SKIP en esquina inferior derecha
        bool inSkipZone = (px >= 210.0f && py >= 194.0f);
        if (inSkipZone) {
            return -1;
        }

        int col = std::max(0, std::min(3, static_cast<int>(px / 80.0f)));
        lastTouchCol = col;
        return col;
    }

private:
    static void drawRectOutline(float x, float y, float w, float h, float thickness, u32 color, float z = 0.5f) {
        C2D_DrawLine(x, y, color, x + w, y, color, thickness, z);
        C2D_DrawLine(x + w, y, color, x + w, y + h, color, thickness, z);
        C2D_DrawLine(x + w, y + h, color, x, y + h, color, thickness, z);
        C2D_DrawLine(x, y + h, color, x, y, color, thickness, z);
    }

    static void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24, float z = 0.5f) {
        const float step = 6.2831853f / segments;
        for (int i = 0; i < segments; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;
            C2D_DrawLine(x + std::cos(a0) * radius, y + std::sin(a0) * radius, color,
                         x + std::cos(a1) * radius, y + std::sin(a1) * radius, color,
                         thickness, z);
        }
    }
};
