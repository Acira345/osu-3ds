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
        fruits.clear();
        nextFruitIndex = 0;
        catcherX = 256.0f;
        catcherPlateWidth = 64.0f;
        facingDir = 1;
        isDashing = false;
        isHyperDashing = false;
        hyperTargetX = 256.0f;
        hyperDashTimer = 0.0f;
        plateFruits.clear();
        ghosts.clear();
        splashes.clear();
        popups.clear();
        plateBounce = 0.0f;
    }

    // Inicializa y convierte cualquier beatmap a osu!catch
    void initFromBeatmap(const Beatmap& beatmap) {
        reset();

        // Calcular ancho del plato según Circle Size (CS)
        float cs = beatmap.diff.cs;
        catcherPlateWidth = std::max(38.0f, std::min(88.0f, 64.0f * (1.0f - 0.7f * (cs - 5.0f) / 5.0f)));

        fruits.reserve(beatmap.circles.size() * 3);

        int fruitTypeCounter = 0;

        for (const auto& c : beatmap.circles) {
            if (c.isSpinner) {
                // Spinners se convierten en Lluvia de Plátanos (Banana Shower)
                int duration = std::max(200, c.endTime - c.time);
                int bananaCount = std::max(3, duration / 80);
                float step = static_cast<float>(duration) / bananaCount;

                for (int b = 0; b < bananaCount; ++b) {
                    CatchFruit bf;
                    bf.type = FRUIT_BANANA;
                    // Distribución rítmica/pseudoaleatoria a lo ancho
                    float pseudoRand = std::fmod((c.time * 0.017f + b * 73.19f), 1.0f);
                    bf.x = 40.0f + pseudoRand * 432.0f;
                    bf.time = c.time + static_cast<int>(b * step);
                    bf.isHyper = false;
                    bf.state = NOT_HIT;
                    bf.isCaught = false;
                    fruits.push_back(bf);
                }
            } else if (c.isSlider) {
                // 1. Cabeza del slider: Fruta grande
                CatchFruit hf;
                hf.type = static_cast<CatchFruitType>(fruitTypeCounter % 4);
                fruitTypeCounter++;
                hf.x = std::max(0.0f, std::min(512.0f, c.x));
                hf.time = c.time;
                hf.colorIndex = c.colorIndex;
                hf.state = NOT_HIT;
                fruits.push_back(hf);

                // 2. Gotas de zumo a lo largo de los ticks del slider
                for (const auto& tick : c.sliderTicks) {
                    CatchFruit df;
                    df.type = FRUIT_DROPLET;
                    df.x = std::max(0.0f, std::min(512.0f, tick.x));
                    df.time = tick.time;
                    df.state = NOT_HIT;
                    fruits.push_back(df);
                }

                // 3. Gotitas intermedias (Tiny Droplets) para densidad de ritmo
                if (!c.sliderPath.empty() && c.endTime > c.time + 100) {
                    int subSteps = std::max(2, std::min(12, (c.endTime - c.time) / 120));
                    for (int s = 1; s < subSteps; ++s) {
                        float prog = static_cast<float>(s) / subSteps;
                        int sTime = c.time + static_cast<int>((c.endTime - c.time) * prog);
                        Vec2 pos = const_cast<HitCircle&>(c).getSliderBallPos(sTime);

                        CatchFruit tdf;
                        tdf.type = FRUIT_TINY_DROPLET;
                        tdf.x = std::max(0.0f, std::min(512.0f, pos.x));
                        tdf.time = sTime;
                        tdf.state = NOT_HIT;
                        fruits.push_back(tdf);
                    }
                }

                // 4. Cola del slider: Fruta grande final
                Vec2 endPos = c.getEndPosition();
                CatchFruit tf;
                tf.type = static_cast<CatchFruitType>(fruitTypeCounter % 4);
                fruitTypeCounter++;
                tf.x = std::max(0.0f, std::min(512.0f, endPos.x));
                tf.time = c.endTime;
                tf.colorIndex = c.colorIndex;
                tf.state = NOT_HIT;
                fruits.push_back(tf);
            } else {
                // Círculo normal: Fruta grande
                CatchFruit cf;
                cf.type = static_cast<CatchFruitType>(fruitTypeCounter % 4);
                fruitTypeCounter++;
                cf.x = std::max(0.0f, std::min(512.0f, c.x));
                cf.time = c.time;
                cf.colorIndex = c.colorIndex;
                cf.state = NOT_HIT;
                fruits.push_back(cf);
            }
        }

        // Ordenar todas las frutas cronológicamente
        std::sort(fruits.begin(), fruits.end(), [](const CatchFruit& a, const CatchFruit& b) {
            if (a.time != b.time) return a.time < b.time;
            return a.x < b.x;
        });

        // Calcular Hyperfruits: si dos frutas consecutivas requieren más velocidad
        // que la velocidad de dash estándar, la primera se convierte en Hyperfruit.
        for (size_t i = 0; i + 1 < fruits.size(); ++i) {
            auto& f1 = fruits[i];
            const auto& f2 = fruits[i + 1];

            if (f1.type == FRUIT_BANANA || f2.type == FRUIT_BANANA) continue;

            int dt = f2.time - f1.time;
            if (dt > 10 && dt < 800) {
                float dist = std::abs(f2.x - f1.x);
                float maxReach = (BASE_WALK_SPEED * DASH_FACTOR) * dt + (catcherPlateWidth * 0.45f);
                if (dist > maxReach) {
                    f1.isHyper = true;
                    f1.hyperTargetX = f2.x;
                }
            }
        }
    }

    // Actualiza movimiento del Catcher, estelas y temporizadores
    void update(float dtMs, float moveInput, bool dashPressed, float analogX, int songTime, float speedRate) {
        isDashing = dashPressed;

        // 1. Manejo de Hyperdash activo
        if (isHyperDashing) {
            hyperDashTimer -= dtMs;
            float dir = (hyperTargetX > catcherX) ? 1.0f : -1.0f;
            float step = (BASE_WALK_SPEED * HYPER_FACTOR) * dtMs;

            if (std::abs(hyperTargetX - catcherX) <= step || hyperDashTimer <= 0.0f) {
                catcherX = hyperTargetX;
                isHyperDashing = false;
            } else {
                catcherX += dir * step;
                facingDir = static_cast<int>(dir);
            }

            // Generar estela fantasma roja brillante
            ghosts.push_back({ catcherX, 1.0f, true });
        } else {
            // 2. Movimiento Normal / Dash
            float move = 0.0f;
            if (std::abs(analogX) > 0.15f) {
                move = analogX;
            } else if (moveInput != 0.0f) {
                move = moveInput;
            }

            if (move != 0.0f) {
                float speed = BASE_WALK_SPEED * (isDashing ? DASH_FACTOR : 1.0f);
                catcherX += move * speed * dtMs;
                facingDir = (move > 0.0f) ? 1 : -1;

                if (isDashing) {
                    ghosts.push_back({ catcherX, 0.75f, false });
                }
            }
        }

        // Delimitar a los márgenes jugables (0 .. 512)
        catcherX = std::max(12.0f, std::min(500.0f, catcherX));

        // 3. Desvanecer estelas fantasma
        for (auto& g : ghosts) {
            g.alpha -= 0.06f;
        }
        ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(), [](const CatcherGhost& g) {
            return g.alpha <= 0.0f;
        }), ghosts.end());

        // 4. Desvanecer rebote del plato y partículas de salpicadura de jugo
        plateBounce = std::max(0.0f, plateBounce - dtMs * 0.007f);

        for (auto& s : splashes) {
            s.x += s.vx;
            s.y += s.vy;
            s.vy += 0.12f; // gravedad suave
            s.life -= dtMs * 0.0035f;
        }
        splashes.erase(std::remove_if(splashes.begin(), splashes.end(), [](const CatchSplash& s) {
            return s.life <= 0.0f;
        }), splashes.end());

        for (auto& p : popups) {
            p.y -= 0.55f;
            p.life -= dtMs * 0.003f;
        }
        popups.erase(std::remove_if(popups.begin(), popups.end(), [](const CatchPopup& p) {
            return p.life <= 0.0f;
        }), popups.end());

        // Limpiar pila del plato si se llena mucho
        if (plateFruits.size() > 10) {
            plateFruits.erase(plateFruits.begin(), plateFruits.begin() + 4);
        }
    }

    // Revisa aciertos y fallos de frutas con la posición actual del plato
    // Retorna: 300 (Fruta), 100 (Droplet), 10 (Tiny), 1100 (Banana), -1 (Miss de Fruta), 0 (Nada)
    int checkCatches(int songTime, float hit50, int& outScoreAdd, int& outComboAdd, float& outHpAdd) {
        outScoreAdd = 0;
        outComboAdd = 0;
        outHpAdd = 0.0f;

        float halfPlate = catcherPlateWidth * 0.5f;

        for (size_t i = nextFruitIndex; i < fruits.size(); ++i) {
            auto& f = fruits[i];
            if (f.state != NOT_HIT) continue;

            int dt = songTime - f.time;

            // Ventana de captura en el plato (+/- 28 ms)
            if (std::abs(dt) <= 28) {
                float dist = std::abs(f.x - catcherX);
                if (dist <= halfPlate) {
                    // ¡FRUTA ATRAPADA!
                    f.isCaught = true;
                    f.state = HIT_300;
                    plateBounce = 1.0f;

                    // Si es Hyperfruit, dispara el Hyperdash
                    if (f.isHyper) {
                        isHyperDashing = true;
                        hyperTargetX = f.hyperTargetX;
                        hyperDashTimer = 280.0f;
                    }

                    // Apilar en el plato si es fruta grande o plátano
                    if (f.type != FRUIT_TINY_DROPLET) {
                        float rX = (f.x - catcherX) * 0.45f;
                        float rY = static_cast<float>(plateFruits.size() * 5);
                        plateFruits.push_back({ rX, rY, f.type });
                    }

                    // Generar salpicadura de jugo (Juice Splash)
                    u32 sClr = (f.type == FRUIT_APPLE) ? CLR_APPLE : ((f.type == FRUIT_ORANGE) ? CLR_ORANGE : ((f.type == FRUIT_GRAPE) ? CLR_GRAPE : ((f.type == FRUIT_STRAWBERRY) ? CLR_STRAWBERRY : ((f.type == FRUIT_BANANA) ? CLR_BANANA : CLR_DROPLET))));
                    int splashCount = (f.type == FRUIT_TINY_DROPLET) ? 3 : (f.type == FRUIT_DROPLET ? 5 : 8);
                    float spCenter = toScreenX(catcherX);
                    for (int s = 0; s < splashCount; ++s) {
                        float ang = -static_cast<float>(M_PI) * (0.15f + 0.70f * (static_cast<float>(std::rand() % 100) / 100.0f));
                        float spd = 1.4f + static_cast<float>(std::rand() % 100) / 45.0f;
                        splashes.push_back({
                            spCenter + (static_cast<float>(std::rand() % 24) - 12.0f),
                            PLATE_Y - 3.0f,
                            std::cos(ang) * spd,
                            std::sin(ang) * spd,
                            (f.type == FRUIT_TINY_DROPLET ? 1.6f : 2.8f),
                            sClr,
                            1.0f
                        });
                    }

                    if (f.type == FRUIT_BANANA) {
                        outScoreAdd = 1100;
                        outComboAdd = 1;
                        outHpAdd = 0.02f;
                        popups.push_back({ spCenter, PLATE_Y - 14.0f, "+1100", CLR_BANANA, 1.0f });
                        return 1100;
                    } else if (f.type == FRUIT_DROPLET) {
                        outScoreAdd = 100;
                        outComboAdd = 1;
                        outHpAdd = 0.015f;
                        popups.push_back({ spCenter, PLATE_Y - 14.0f, "+100", CLR_DROPLET, 1.0f });
                        return 100;
                    } else if (f.type == FRUIT_TINY_DROPLET) {
                        outScoreAdd = 10;
                        outComboAdd = 0;
                        outHpAdd = 0.005f;
                        popups.push_back({ spCenter, PLATE_Y - 12.0f, "+10", Ui::rgba(220, 240, 255), 0.8f });
                        return 10;
                    } else {
                        outScoreAdd = 300;
                        outComboAdd = 1;
                        outHpAdd = 0.045f;
                        popups.push_back({ spCenter, PLATE_Y - 14.0f, "+300", Ui::rgba(255, 230, 80), 1.0f });
                        return 300;
                    }
                }
            }

            // Si la fruta cayó más allá del plato sin ser atrapada
            if (dt > 35) {
                f.state = HIT_MISS;
                if (f.type == FRUIT_BANANA || f.type == FRUIT_TINY_DROPLET) {
                    // No penalizan combo ni salud
                    continue;
                } else if (f.type == FRUIT_DROPLET) {
                    // Gota mediana perdida: pequeña penalización
                    outHpAdd = -0.015f;
                } else {
                    // ¡Fruta grande perdida -> MISS!
                    outHpAdd = -0.10f;
                    return -1;
                }
            }

            // No evaluar frutas que aún están muy arriba
            if (f.time > songTime + 45) {
                break;
            }
        }

        // Avanzar índice
        while (nextFruitIndex < fruits.size() && fruits[nextFruitIndex].state != NOT_HIT) {
            nextFruitIndex++;
        }

        return 0;
    }

    // Renderizado en Pantalla Superior (400x240) con soporte Estereoscópico 3D
    void renderTopScreen(C2D_TextBuf textBuf, int songTime, float speedRate, float eyeSign, float slider3DVal, bool inKiai) {
        auto depthShift = [&](float px) { return eyeSign * slider3DVal * px; };
        float dPlayfield = depthShift(-1.0f);
        float dCatcher   = depthShift(3.2f);
        float approachTime = BASE_APPROACH / std::max(0.5f, speedRate);

        // 1. Paredes y Fondo del Playfield de Catch
        float pfLeft = PLAYFIELD_X + dPlayfield;
        float pfRight = pfLeft + PLAYFIELD_W;

        u32 bgClrTop = inKiai ? Ui::rgba(30, 16, 42, 230) : Ui::rgba(14, 12, 24, 230);
        u32 bgClrBot = inKiai ? Ui::rgba(42, 20, 58, 245) : Ui::rgba(20, 16, 32, 245);
        Ui::gradV(pfLeft, 0.0f, PLAYFIELD_W, 240.0f, 0.15f, bgClrTop, bgClrBot);

        // Paredes laterales de bambú / neón
        u32 wallBorder = inKiai ? Ui::rgba(255, 120, 190) : Ui::rgba(95, 80, 140);
        C2D_DrawLine(pfLeft, 0.0f, wallBorder, pfLeft, 240.0f, wallBorder, 2.5f, 0.20f);
        C2D_DrawLine(pfRight, 0.0f, wallBorder, pfRight, 240.0f, wallBorder, 2.5f, 0.20f);

        // 2. Línea guía de captura (Platter Line)
        u32 guideClr = inKiai ? Ui::rgba(255, 140, 200, 90) : Ui::rgba(160, 150, 200, 55);
        C2D_DrawLine(pfLeft, PLATE_Y, guideClr, pfRight, PLATE_Y, guideClr, 1.0f, 0.22f);

        // 3. Estelas fantasma del Catcher al acelerar (Dash / Hyperdash)
        float dGhost = depthShift(2.8f);
        for (const auto& g : ghosts) {
            float gx = toScreenX(g.x) + dGhost;
            u32 gClr = g.isHyper ? Ui::rgba(255, 40, 50, static_cast<u8>(g.alpha * 220.0f))
                                 : Ui::rgba(80, 180, 255, static_cast<u8>(g.alpha * 130.0f));
            float pW = catcherPlateWidth * SCALE_X;
            Ui::pill(gx - (pW * 0.5f), PLATE_Y - 2.0f, pW, 5.0f, 0.25f, gClr);
        }

        // Líneas de velocidad cósmica durante Hyperdash (Warp Speed Lines)
        if (isHyperDashing) {
            for (int w = 0; w < 7; ++w) {
                float wy = PLATE_Y - 28.0f + (w * 8.5f);
                float wSpeed = std::fmod((songTime * 2.2f + w * 57.0f), 380.0f);
                float dirSign = (facingDir > 0) ? -1.0f : 1.0f;
                float wx0 = toScreenX(catcherX) + dCatcher + dirSign * wSpeed;
                float wx1 = wx0 + dirSign * 55.0f;
                u32 wClr = Ui::rgba(255, 35, 45, 175);
                C2D_DrawLine(wx0, wy, Ui::withAlpha(wClr, 0), wx1, wy, wClr, 1.8f, 0.26f);
            }
        }

        // 4. Renderizado de Frutas que caen con caída tridimensional hacia el frente
        for (size_t i = nextFruitIndex; i < fruits.size(); ++i) {
            auto& f = fruits[i];
            if (f.state != NOT_HIT) continue;

            if (f.time - songTime > static_cast<int>(approachTime)) {
                break; // Aún no visible
            }

            float dt = static_cast<float>(f.time - songTime);
            float fruitY = PLATE_Y - (dt / approachTime) * PLATE_Y;

            // Oscilación fluida suave para gotas de jugo (sensación líquida suspendida)
            float sway = 0.0f;
            if (f.type == FRUIT_DROPLET || f.type == FRUIT_TINY_DROPLET) {
                sway = std::sin((songTime * 0.007f) + (f.x * 0.045f)) * (f.type == FRUIT_DROPLET ? 3.0f : 1.8f);
            }

            float yRatio = std::max(0.0f, std::min(1.0f, fruitY / PLATE_Y));
            float fDepth = -2.2f + yRatio * 5.4f; // Cae desde el fondo (-2.2f) hacia el plato (+3.2f)
            float fruitX = toScreenX(f.x) + depthShift(fDepth) + sway;

            if (fruitY >= -20.0f && fruitY <= PLATE_Y + 15.0f) {
                // Línea guía / estela láser conectora para Hyperfruits hacia la siguiente fruta
                if (f.isHyper && fruitY >= -10.0f && fruitY <= PLATE_Y) {
                    float destX = toScreenX(f.hyperTargetX) + dCatcher;
                    float hPulse = 0.5f + 0.5f * std::sin((songTime * 0.02f) + f.time * 0.01f);
                    u32 guideLaser = Ui::rgba(255, 30, 40, static_cast<u8>(85 + 110 * hPulse));
                    C2D_DrawLine(fruitX, fruitY, guideLaser, destX, PLATE_Y, guideLaser, 1.6f, 0.24f);

                    // Flecha indicadora en el plato
                    C2D_DrawTriangle(destX - 4.5f, PLATE_Y - 8.0f, guideLaser, destX + 4.5f, PLATE_Y - 8.0f, guideLaser, destX, PLATE_Y - 1.5f, guideLaser, 0.25f);
                }

                renderFruit(fruitX, fruitY, f.type, f.isHyper, 0.45f);
            }
        }

        // 5. Renderizado del Catcher y su Plato en primer plano
        float pBounceY = std::sin(plateBounce * static_cast<float>(M_PI)) * 4.0f;
        float currentPlateY = PLATE_Y + pBounceY;
        float screenCatcherX = toScreenX(catcherX) + dCatcher;
        float plateW = catcherPlateWidth * SCALE_X;
        float halfW = plateW * 0.5f;

        // Aura de Hyperdash alrededor del Catcher si está activa
        if (isHyperDashing) {
            float pulse = 0.5f + 0.5f * std::sin(songTime * 0.05f);
            u32 auraClr = Ui::rgba(255, 30, 50, static_cast<u8>(150 + 90 * pulse));
            C2D_DrawCircleSolid(screenCatcherX, currentPlateY + 12.0f, 0.30f, halfW * 1.4f, auraClr);
            // Destellos de rayos alrededor
            for (int r = 0; r < 4; ++r) {
                float rAng = r * 1.57f + songTime * 0.01f;
                float rx = screenCatcherX + std::cos(rAng) * (halfW * 0.8f);
                float ry = currentPlateY + std::sin(rAng) * 12.0f;
                C2D_DrawCircleSolid(rx, ry, 0.32f, 2.2f, Ui::rgba(255, 230, 100));
            }
        } else if (isDashing) {
            u32 dashAura = Ui::rgba(70, 190, 255, 110);
            C2D_DrawCircleSolid(screenCatcherX, currentPlateY + 12.0f, 0.30f, halfW * 1.18f, dashAura);
        }

        // Plato metálico plateado con asas y rebote elástico
        u32 plateTop = Ui::rgba(240, 242, 255);
        u32 plateBot = Ui::rgba(130, 135, 160);
        Ui::gradV(screenCatcherX - halfW, currentPlateY - 2.0f, plateW, 5.0f, 0.50f, plateTop, plateBot);
        Ui::pill(screenCatcherX - halfW - 2.0f, currentPlateY - 3.0f, 4.0f, 7.0f, 0.52f, Ui::rgba(200, 205, 230));
        Ui::pill(screenCatcherX + halfW - 2.0f, currentPlateY - 3.0f, 4.0f, 7.0f, 0.52f, Ui::rgba(200, 205, 230));

        // Partículas de salpicadura de jugo (Juice Splashes proyectadas hacia el jugador)
        float dSplash = depthShift(4.2f);
        for (const auto& sp : splashes) {
            u8 spAlpha = static_cast<u8>(std::max(0.0f, std::min(1.0f, sp.life)) * 255.0f);
            C2D_DrawCircleSolid(sp.x + dSplash, sp.y, 0.54f, sp.size, Ui::withAlpha(sp.color, spAlpha));
            C2D_DrawCircleSolid(sp.x + dSplash - 0.6f, sp.y - 0.6f, 0.55f, sp.size * 0.4f, Ui::withAlpha(CLR_WHITE, spAlpha));
        }

        // Indicadores flotantes de puntos / combo de frutas (+300 / +100 / +10 / +1100)
        float dPopup = depthShift(4.6f);
        for (const auto& pp : popups) {
            u8 pAlpha = static_cast<u8>(std::max(0.0f, std::min(1.0f, pp.life)) * 255.0f);
            drawCenteredText(textBuf, pp.x + dPopup, pp.y, 0.36f, Ui::withAlpha(pp.color, pAlpha), pp.text, 0.60f);
        }

        // Frutas acumuladas en el plato
        for (const auto& pf : plateFruits) {
            float pfx = screenCatcherX + (pf.relX * SCALE_X);
            float pfy = currentPlateY - 6.0f - (pf.relY * 0.6f);
            if (pfy >= currentPlateY - 35.0f) {
                renderFruitMini(pfx, pfy, pf.type, 0.55f);
            }
        }

        // Personaje Catcher Chibi debajo del plato
        renderCatcherChibi(screenCatcherX, currentPlateY + 3.0f, facingDir, isDashing || isHyperDashing, 0.48f);
    }

    // Renderizado en Pantalla Inferior (320x240): Deck de Control Táctil y Radar
    void renderBottomDeck(C2D_TextBuf textBuf, float animTick, int songTime) {
        // 1. Radar de frutas que se aproximan en la parte superior
        Ui::panel(8.0f, 10.0f, 304.0f, 85.0f, 0.15f, Ui::rgba(20, 16, 32, 230), Ui::rgba(75, 60, 110));
        drawCenteredText(textBuf, 160.0f, 18.0f, 0.28f, Ui::rgba(180, 170, 215), "RADAR DE FRUTAS // CATCH DECK", 0.25f);

        // Representación de frutas en el radar
        float radarX = 14.0f;
        float radarW = 292.0f;
        for (size_t i = nextFruitIndex; i < fruits.size(); ++i) {
            const auto& f = fruits[i];
            if (f.state != NOT_HIT) continue;
            int dt = f.time - songTime;
            if (dt < 0) continue;
            if (dt > 1600) break;

            float rx = radarX + (f.x / 512.0f) * radarW;
            float ry = 85.0f - (static_cast<float>(dt) / 1600.0f) * 60.0f;
            u32 fClr = (f.type == FRUIT_BANANA) ? CLR_BANANA : ((f.type == FRUIT_DROPLET) ? CLR_DROPLET : CLR_APPLE);
            C2D_DrawCircleSolid(rx, ry, 0.30f, f.isHyper ? 3.5f : 2.0f, fClr);
        }

        // Indicador del Catcher en el radar
        float cRadarX = radarX + (catcherX / 512.0f) * radarW;
        Ui::pill(cRadarX - 10.0f, 82.0f, 20.0f, 4.0f, 0.35f, CLR_WHITE);

        // 2. Barra táctil de posicionamiento directo (Stylus Touch Slider)
        float barY = 105.0f;
        Ui::panel(8.0f, barY, 304.0f, 36.0f, 0.20f, Ui::rgba(26, 22, 42, 230), Ui::rgba(95, 75, 140));
        drawCenteredText(textBuf, 160.0f, barY + 9.0f, 0.30f, Ui::rgba(200, 190, 230), "TOCA O DESLIZA EL LAPIZ DIRECTAMENTE", 0.32f);

        // Cursor del Catcher en la barra táctil
        float touchBarX = 14.0f + (catcherX / 512.0f) * 292.0f;
        Ui::pill(touchBarX - 18.0f, barY + 18.0f, 36.0f, 12.0f, 0.38f, isDashing ? Ui::rgba(70, 190, 255) : Ui::rgba(255, 102, 171));
        drawCenteredText(textBuf, touchBarX, barY + 24.0f, 0.26f, CLR_WHITE, "CATCH", 0.42f);

        // 3. Botones táctiles inferiores: [ < IZQ ], [ DER > ] y [ DASH / ACELERAR ]
        // Botón Izquierda
        Ui::panel(10.0f, 150.0f, 80.0f, 38.0f, 0.25f, Ui::rgba(32, 26, 52, 230), Ui::rgba(95, 80, 140));
        drawCenteredText(textBuf, 50.0f, 162.0f, 0.46f, CLR_WHITE, "< IZQ", 0.35f);
        drawCenteredText(textBuf, 50.0f, 178.0f, 0.22f, Ui::rgba(180, 170, 200), "D-PAD < / Y", 0.35f);

        // Botón Derecha
        Ui::panel(98.0f, 150.0f, 80.0f, 38.0f, 0.25f, Ui::rgba(32, 26, 52, 230), Ui::rgba(95, 80, 140));
        drawCenteredText(textBuf, 138.0f, 162.0f, 0.46f, CLR_WHITE, "DER >", 0.35f);
        drawCenteredText(textBuf, 138.0f, 178.0f, 0.22f, Ui::rgba(180, 170, 200), "D-PAD > / A", 0.35f);

        // Botón DASH
        u32 dashBg = isDashing ? Ui::rgba(50, 160, 240, 240) : Ui::rgba(35, 75, 130, 230);
        Ui::panel(186.0f, 150.0f, 124.0f, 38.0f, 0.25f, dashBg, CLR_WHITE);
        drawCenteredText(textBuf, 248.0f, 162.0f, 0.44f, CLR_WHITE, "DASH >>", 0.35f);
        drawCenteredText(textBuf, 248.0f, 178.0f, 0.22f, Ui::rgba(210, 235, 255), "SOSTENER L / R / B", 0.35f);
    }

    // Procesa interacción táctil directa en la pantalla inferior
    void handleTouch(float px, float py, bool isDown) {
        if (!isDown) {
            isDashing = false;
            return;
        }

        // Barra deslizadora de posición directa (Y: 90 .. 145)
        if (py >= 90.0f && py <= 145.0f && px >= 12.0f && px <= 308.0f) {
            float targetOsuX = ((px - 14.0f) / 292.0f) * 512.0f;
            catcherX = std::max(12.0f, std::min(500.0f, targetOsuX));
        }
        // Botón < IZQUIERDA (x: 10 .. 90, y: 148 .. 190)
        else if (px >= 10.0f && px <= 90.0f && py >= 148.0f && py <= 190.0f) {
            catcherX = std::max(12.0f, catcherX - 18.0f);
            facingDir = -1;
        }
        // Botón DERECHA > (x: 98 .. 178, y: 148 .. 190)
        else if (px >= 98.0f && px <= 178.0f && py >= 148.0f && py <= 190.0f) {
            catcherX = std::min(500.0f, catcherX + 18.0f);
            facingDir = 1;
        }
        // Botón DASH >> (x: 186 .. 310, y: 148 .. 190)
        else if (px >= 186.0f && px <= 310.0f && py >= 148.0f && py <= 190.0f) {
            isDashing = true;
        }
    }

private:
    // Renderizado detallado de cada fruta con sombras y brillos
    void renderFruit(float x, float y, CatchFruitType type, bool isHyper, float z) {
        // Aura radiante de Hyperfruit pulsante
        if (isHyper) {
            float pulse = 0.5f + 0.5f * std::sin((x + y) * 0.14f);
            C2D_DrawCircleSolid(x, y, z - 0.05f, 18.0f + 4.0f * pulse, Ui::rgba(255, 30, 45, 130));
            C2D_DrawCircleSolid(x, y, z - 0.03f, 14.0f + 2.0f * pulse, Ui::rgba(255, 70, 75, 190));
            C2D_DrawCircleSolid(x, y, z - 0.01f, 10.5f, Ui::rgba(255, 230, 230, 220));
        }

        if (type == FRUIT_APPLE) {
            // Manzana roja
            C2D_DrawCircleSolid(x - 3.5f, y, z, 7.5f, CLR_APPLE);
            C2D_DrawCircleSolid(x + 3.5f, y, z, 7.5f, CLR_APPLE);
            C2D_DrawCircleSolid(x - 2.0f, y - 2.0f, z + 0.01f, 3.0f, CLR_WHITE); // Brillo
            // Tallo y hoja
            C2D_DrawLine(x, y - 6.0f, Ui::rgba(90, 50, 20), x + 1.0f, y - 10.0f, Ui::rgba(90, 50, 20), 1.8f, z + 0.02f);
            C2D_DrawCircleSolid(x + 3.5f, y - 9.0f, z + 0.02f, 2.5f, Ui::rgba(60, 200, 70));
        } else if (type == FRUIT_ORANGE) {
            // Naranja
            C2D_DrawCircleSolid(x, y, z, 8.5f, CLR_ORANGE);
            C2D_DrawCircleSolid(x - 2.5f, y - 2.5f, z + 0.01f, 3.2f, Ui::rgba(255, 230, 160));
            C2D_DrawCircleSolid(x, y - 8.0f, z + 0.02f, 2.0f, Ui::rgba(50, 180, 60)); // Hoja
        } else if (type == FRUIT_GRAPE) {
            // Racimo de uvas
            C2D_DrawCircleSolid(x - 3.5f, y - 3.0f, z, 4.5f, CLR_GRAPE);
            C2D_DrawCircleSolid(x + 3.5f, y - 3.0f, z, 4.5f, CLR_GRAPE);
            C2D_DrawCircleSolid(x, y + 2.0f, z, 4.5f, CLR_GRAPE);
            C2D_DrawCircleSolid(x - 2.0f, y - 4.0f, z + 0.01f, 1.8f, CLR_WHITE);
        } else if (type == FRUIT_STRAWBERRY) {
            // Fresa
            C2D_DrawCircleSolid(x, y, z, 7.8f, CLR_STRAWBERRY);
            C2D_DrawCircleSolid(x - 2.0f, y - 2.0f, z + 0.01f, 2.4f, CLR_WHITE);
            // Corona verde
            C2D_DrawCircleSolid(x - 3.0f, y - 7.0f, z + 0.02f, 2.2f, Ui::rgba(50, 200, 60));
            C2D_DrawCircleSolid(x + 3.0f, y - 7.0f, z + 0.02f, 2.2f, Ui::rgba(50, 200, 60));
        } else if (type == FRUIT_BANANA) {
            // Plátano curvado
            C2D_DrawCircleSolid(x, y, z, 8.0f, CLR_BANANA);
            C2D_DrawCircleSolid(x - 2.0f, y - 2.0f, z + 0.01f, 3.0f, CLR_WHITE);
            C2D_DrawCircleSolid(x + 6.0f, y - 5.0f, z + 0.02f, 2.2f, Ui::rgba(120, 90, 40)); // Punta
        } else if (type == FRUIT_DROPLET) {
            // Gota mediana fluida de jugo en forma de lágrima
            C2D_DrawCircleSolid(x, y + 1.0f, z, 4.6f, CLR_DROPLET);
            C2D_DrawTriangle(x - 4.2f, y + 1.0f, CLR_DROPLET, x + 4.2f, y + 1.0f, CLR_DROPLET, x, y - 7.5f, CLR_DROPLET, z);
            C2D_DrawCircleSolid(x - 1.2f, y - 0.5f, z + 0.01f, 1.6f, CLR_WHITE);
        } else {
            // Gota diminuta fluida
            C2D_DrawCircleSolid(x, y + 0.5f, z, 2.6f, CLR_DROPLET);
            C2D_DrawTriangle(x - 2.4f, y + 0.5f, CLR_DROPLET, x + 2.4f, y + 0.5f, CLR_DROPLET, x, y - 4.5f, CLR_DROPLET, z);
        }
    }

    void renderFruitMini(float x, float y, CatchFruitType type, float z) {
        u32 clr = CLR_APPLE;
        if (type == FRUIT_ORANGE) clr = CLR_ORANGE;
        else if (type == FRUIT_GRAPE) clr = CLR_GRAPE;
        else if (type == FRUIT_STRAWBERRY) clr = CLR_STRAWBERRY;
        else if (type == FRUIT_BANANA) clr = CLR_BANANA;
        else if (type == FRUIT_DROPLET) clr = CLR_DROPLET;

        C2D_DrawCircleSolid(x, y, z, 3.8f, clr);
        C2D_DrawCircleSolid(x - 1.0f, y - 1.0f, z + 0.01f, 1.4f, CLR_WHITE);
    }

    // Mascota Catcher Chibi
    void renderCatcherChibi(float cx, float cy, int dir, bool dashing, float z) {
        // Cabeza Chibi
        C2D_DrawCircleSolid(cx, cy + 9.0f, z, 8.5f, Ui::rgba(255, 224, 195));

        // Gorro de cocinero / catcher
        Ui::pill(cx - 7.0f, cy + 1.0f, 14.0f, 5.0f, z + 0.02f, CLR_WHITE);
        C2D_DrawCircleSolid(cx, cy - 1.5f, z + 0.01f, 6.0f, CLR_WHITE);

        // Ojos orientados según movimiento
        float eyeOff = (dir > 0) ? 2.5f : -2.5f;
        C2D_DrawCircleSolid(cx + eyeOff - 2.5f, cy + 8.5f, z + 0.03f, 1.4f, Ui::rgba(30, 25, 45));
        C2D_DrawCircleSolid(cx + eyeOff + 2.5f, cy + 8.5f, z + 0.03f, 1.4f, Ui::rgba(30, 25, 45));

        // Sonrisa
        C2D_DrawCircleSolid(cx + eyeOff, cy + 12.0f, z + 0.03f, 1.2f, Ui::rgba(255, 100, 130));

        // Cuerpo / delantal
        Ui::pill(cx - 6.0f, cy + 17.0f, 12.0f, 11.0f, z - 0.01f, dashing ? Ui::rgba(255, 80, 140) : Ui::rgba(70, 160, 240));

        // Manos sosteniendo el plato hacia arriba
        C2D_DrawLine(cx - 5.0f, cy + 16.0f, CLR_WHITE, cx - 7.0f, cy + 2.0f, CLR_WHITE, 2.0f, z + 0.04f);
        C2D_DrawLine(cx + 5.0f, cy + 16.0f, CLR_WHITE, cx + 7.0f, cy + 2.0f, CLR_WHITE, 2.0f, z + 0.04f);
    }
};
