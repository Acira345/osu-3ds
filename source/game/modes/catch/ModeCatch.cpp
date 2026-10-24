#include "ModeCatch.hpp"

void ModeCatch::reset() {
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
void ModeCatch::initFromBeatmap(const Beatmap& beatmap) {
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
void ModeCatch::update(float dtMs, float moveInput, bool dashPressed, float analogX, int songTime, float speedRate) {
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
int ModeCatch::checkCatches(int songTime, float hit50, int& outScoreAdd, int& outComboAdd, float& outHpAdd) {
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

// Procesa interacción táctil directa en la pantalla inferior
void ModeCatch::handleTouch(float px, float py, bool isDown) {
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
