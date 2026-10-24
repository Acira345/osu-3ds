#include "ModeMania.hpp"

void ModeMania::adjustScrollSpeed(float delta) {
    userScrollSpeed = std::max(0.5f, std::min(3.5f, userScrollSpeed + delta));
    scrollOsdTimer = 1.8f;
}

void ModeMania::reset() {
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
void ModeMania::initFromBeatmap(const Beatmap& beatmap) {
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
void ModeMania::update(float deltaTime) {
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

void ModeMania::spawnHitParticles(int col, u32 baseClr) {
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
int ModeMania::checkMisses(int songTime, float hit50) {
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
int ModeMania::processKeyPress(int col, int songTime, float hit300, float hit100, float hit50) {
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
int ModeMania::processKeyRelease(int col, int songTime, float hit300, float hit100, float hit50) {
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
int ModeMania::updateActiveHolds(int songTime, int& tickHits) {
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

// Procesa interacción táctil directa en pantalla inferior
// Retorna la columna tocada (0..3), o -1 si no aplica
int ModeMania::handleTouch(float px, float py, bool isDown) {
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
