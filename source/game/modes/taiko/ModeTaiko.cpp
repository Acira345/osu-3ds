#include "ModeTaiko.hpp"

void ModeTaiko::reset() {
    notes.clear();
    nextNoteIndex = 0;
    drumHitDonLeft = drumHitDonRight = 0.0f;
    drumHitKatsuLeft = drumHitKatsuRight = 0.0f;
    targetBurstScale = 1.0f;
    targetBurstAlpha = 0.0f;
    mascotBounce = 0.0f;
    drumrollActiveCounter = 0;
    drumrollFanTimer = 0.0f;
    lastHitJudgement = 0;
    hitJudgementTimer = 0.0f;
    missFaceTimer = 0.0f;
    balloonPopTimer = 0.0f;
    totalDrumrollHits = 0;
}

// Conversión oficial de Beatmap a pistas de Taiko
void ModeTaiko::initFromBeatmap(const Beatmap& beatmap) {
    reset();
    notes.reserve(beatmap.circles.size());

    for (const auto& obj : beatmap.circles) {
        TaikoNote tn;
        tn.time = obj.time;
        tn.endTime = (obj.isSlider || obj.isSpinner) ? obj.endTime : obj.time;

        bool isFinish = (obj.hitSound & 4) != 0;
        bool isWhistle = (obj.hitSound & 2) != 0;
        bool isClap = (obj.hitSound & 8) != 0;

        if (obj.isSpinner) {
            tn.type = TAIKO_DENDEN;
            tn.dendenRequiredHits = std::max(4, static_cast<int>((obj.endTime - obj.time) / 110));
        } else if (obj.isSlider) {
            tn.type = TAIKO_DRUMROLL;
            tn.drumrollTotalTicks = std::max(2, static_cast<int>(obj.sliderTicks.size() + 2));
        } else if (isWhistle || isClap) {
            tn.type = isFinish ? TAIKO_BIG_KATSU : TAIKO_KATSU;
        } else {
            tn.type = isFinish ? TAIKO_BIG_DON : TAIKO_DON;
        }
        notes.push_back(tn);
    }

    std::stable_sort(notes.begin(), notes.end(), [](const TaikoNote& a, const TaikoNote& b) {
        return a.time < b.time;
    });
}

// Actualización de animaciones y decaimiento
void ModeTaiko::update(float bpmPulse) {
    mascotBounce = bpmPulse;
    if (drumHitDonLeft > 0.01f) drumHitDonLeft = Ui::approach(drumHitDonLeft, 0.0f, 0.12f);
    if (drumHitDonRight > 0.01f) drumHitDonRight = Ui::approach(drumHitDonRight, 0.0f, 0.12f);
    if (drumHitKatsuLeft > 0.01f) drumHitKatsuLeft = Ui::approach(drumHitKatsuLeft, 0.0f, 0.12f);
    if (drumHitKatsuRight > 0.01f) drumHitKatsuRight = Ui::approach(drumHitKatsuRight, 0.0f, 0.12f);
    if (targetBurstAlpha > 0.01f) {
        targetBurstAlpha = Ui::approach(targetBurstAlpha, 0.0f, 0.08f);
        targetBurstScale = Ui::approach(targetBurstScale, 1.45f, 0.05f);
    }
    if (drumrollFanTimer > 0.01f) {
        drumrollFanTimer = Ui::approach(drumrollFanTimer, 0.0f, 0.030f);
        if (drumrollFanTimer <= 0.01f) drumrollActiveCounter = 0;
    }
    if (hitJudgementTimer > 0.01f) {
        hitJudgementTimer = Ui::approach(hitJudgementTimer, 0.0f, 0.05f);
    }
    if (missFaceTimer > 0.001f) {
        missFaceTimer = std::max(0.0f, missFaceTimer - 0.016f);
    }
    if (balloonPopTimer > 0.01f) {
        balloonPopTimer = Ui::approach(balloonPopTimer, 0.0f, 0.025f);
    }
}

// Procesa un golpe de entrada (Don o Katsu)
int ModeTaiko::processHit(bool isDon, bool isBigAction, int songTime, float hit300, float hit100, float hit50, bool isAutoplay) {
    if (notes.empty()) return TAIKO_HIT_NONE;

    // Modo Autoplay: evaluación infalible sin corromper delta de notas futuras
    if (isAutoplay) {
        for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
            auto& n = notes[i];
            if (n.state != NOT_HIT) continue;
            if (n.type == TAIKO_DRUMROLL || n.type == TAIKO_DENDEN) continue;

            int delta = songTime - n.time;
            if (delta < -static_cast<int>(hit50 + 40)) break; // Nota futura lejana

            bool noteIsDon = (n.type == TAIKO_DON || n.type == TAIKO_BIG_DON);
            if (noteIsDon == isDon) {
                n.state = HIT_300;
                triggerBurst(isDon ? CLR_DON_INNER : CLR_KATSU_INNER);
                lastHitJudgement = 300;
                hitJudgementTimer = 1.0f;
                if (i == nextNoteIndex) nextNoteIndex++;
                return TAIKO_HIT_300;
            }
        }

        // Si no había nota de círculo, verificar drumroll / denden activo
        for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
            auto& n = notes[i];
            if (n.state != NOT_HIT) continue;
            if (n.type == TAIKO_DRUMROLL) {
                if (songTime >= n.time && songTime <= n.endTime) {
                    n.drumrollTicksHit++;
                    totalDrumrollHits++;
                    triggerBurst(CLR_DRUMROLL);
                    drumrollActiveCounter++;
                    drumrollFanTimer = 1.0f;
                    return TAIKO_HIT_TICK;
                }
            } else if (n.type == TAIKO_DENDEN) {
                if (songTime >= n.time && songTime <= n.endTime) {
                    n.dendenCurrentHits++;
                    totalDrumrollHits++;
                    triggerBurst(isDon ? CLR_DON_INNER : CLR_KATSU_INNER);
                    drumrollActiveCounter++;
                    drumrollFanTimer = 1.0f;
                    if (n.dendenCurrentHits >= n.dendenRequiredHits) {
                        n.state = HIT_300;
                        lastHitJudgement = 300;
                        hitJudgementTimer = 1.0f;
                        balloonPopTimer = 1.0f;
                        return TAIKO_HIT_300;
                    }
                    return TAIKO_HIT_DENDEN;
                }
            }
        }
        return TAIKO_HIT_NONE;
    }

    // 1. Prioridad: Buscar nota normal en la ventana de juicio
    for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
        auto& n = notes[i];
        if (n.state != NOT_HIT) continue;
        if (n.type == TAIKO_DRUMROLL || n.type == TAIKO_DENDEN) continue;

        int delta = songTime - n.time;
        if (delta < -static_cast<int>(hit50)) {
            break; // Aún no llega
        }

        if (std::abs(delta) <= static_cast<int>(hit50)) {
            bool noteIsDon = (n.type == TAIKO_DON || n.type == TAIKO_BIG_DON);
            if (noteIsDon == isDon) {
                int absD = std::abs(delta);
                if (absD <= static_cast<int>(hit300)) {
                    n.state = HIT_300;
                    triggerBurst(isDon ? CLR_DON_INNER : CLR_KATSU_INNER);
                    lastHitJudgement = 300;
                    hitJudgementTimer = 1.0f;
                    if (i == nextNoteIndex) nextNoteIndex++;
                    return TAIKO_HIT_300;
                } else if (absD <= static_cast<int>(hit100)) {
                    n.state = HIT_100;
                    triggerBurst(isDon ? CLR_DON_INNER : CLR_KATSU_INNER);
                    lastHitJudgement = 100;
                    hitJudgementTimer = 1.0f;
                    if (i == nextNoteIndex) nextNoteIndex++;
                    return TAIKO_HIT_100;
                } else {
                    n.state = HIT_MISS;
                    lastHitJudgement = -1;
                    hitJudgementTimer = 1.0f;
                    missFaceTimer = 1.2f;
                    if (i == nextNoteIndex) nextNoteIndex++;
                    return TAIKO_HIT_MISS;
                }
            } else {
                // Color incorrecto
                n.state = HIT_MISS;
                lastHitJudgement = -1;
                hitJudgementTimer = 1.0f;
                missFaceTimer = 1.2f;
                if (i == nextNoteIndex) nextNoteIndex++;
                return TAIKO_HIT_MISS;
            }
        }
    }

    // 2. Si no había nota normal, verificar Redoble o Globo
    for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
        auto& n = notes[i];
        if (n.state != NOT_HIT) continue;

        if (n.type == TAIKO_DRUMROLL) {
            if (songTime >= n.time && songTime <= n.endTime) {
                n.drumrollTicksHit++;
                totalDrumrollHits++;
                triggerBurst(CLR_DRUMROLL);
                drumrollActiveCounter++;
                drumrollFanTimer = 1.0f;
                return TAIKO_HIT_TICK;
            }
        } else if (n.type == TAIKO_DENDEN) {
            if (songTime >= n.time && songTime <= n.endTime) {
                n.dendenCurrentHits++;
                totalDrumrollHits++;
                triggerBurst(isDon ? CLR_DON_INNER : CLR_KATSU_INNER);
                drumrollActiveCounter++;
                drumrollFanTimer = 1.0f;
                if (n.dendenCurrentHits >= n.dendenRequiredHits) {
                    n.state = HIT_300;
                    lastHitJudgement = 300;
                    hitJudgementTimer = 1.0f;
                    balloonPopTimer = 1.0f;
                    return TAIKO_HIT_300;
                }
                return TAIKO_HIT_DENDEN;
            }
        }
    }

    return TAIKO_HIT_NONE;
}

// Revisa notas pasadas del receptor sin ser golpeadas
int ModeTaiko::checkMisses(int songTime, float hit50, bool isAutoplay) {
    if (isAutoplay) return 0; // Autoplay nunca deja escapar notas
    int misses = 0;
    for (size_t i = nextNoteIndex; i < notes.size(); ++i) {
        auto& n = notes[i];
        if (n.state != NOT_HIT) {
            if (i == nextNoteIndex) nextNoteIndex++;
            continue;
        }

        if (n.type == TAIKO_DRUMROLL || n.type == TAIKO_DENDEN) {
            if (songTime > n.endTime + 50) {
                n.state = (n.drumrollTicksHit > 0 || n.dendenCurrentHits >= n.dendenRequiredHits) ? HIT_300 : HIT_100;
                if (i == nextNoteIndex) nextNoteIndex++;
            }
            continue;
        }

        if (songTime > (n.time + static_cast<int>(hit50))) {
            n.state = HIT_MISS;
            lastHitJudgement = -1;
            hitJudgementTimer = 1.0f;
            missFaceTimer = 1.2f;
            if (i == nextNoteIndex) nextNoteIndex++;
            misses++;
        } else {
            break;
        }
    }
    return misses;
}

void ModeTaiko::triggerBurst(u32 color) {
    targetBurstColor = color;
    targetBurstAlpha = 1.0f;
    targetBurstScale = 1.0f;
}

// Entrada táctil en el tambor interactivo
int ModeTaiko::handleTouchDrum(float px, float py) {
    float cx = 160.0f;
    float cy = 134.0f;
    float outerRx = 136.0f;
    float outerRy = 86.0f;
    float donRx   = 96.0f;
    float donRy   = 58.0f;

    float dx = px - cx;
    float dy = py - cy;

    if (py >= cy && py <= 238.0f && std::abs(dx) <= outerRx) {
        float distOuterSq = (dx * dx) / (outerRx * outerRx) + (dy * dy) / (outerRy * outerRy);
        if (distOuterSq > 1.0f) {
            if (dx < 0.0f) {
                drumHitKatsuLeft = 1.0f;
                return 3;
            } else {
                drumHitKatsuRight = 1.0f;
                return 4;
            }
        }
    }

    float distOuterSq = (dx * dx) / (outerRx * outerRx) + (dy * dy) / (outerRy * outerRy);
    if (distOuterSq > 1.0f) return 0;

    bool isLeft = (dx < 0.0f);
    float distInnerSq = (dx * dx) / (donRx * donRx) + (dy * dy) / (donRy * donRy);

    if (distInnerSq <= 1.0f) {
        if (isLeft) {
            drumHitDonLeft = 1.0f;
            return 1;
        } else {
            drumHitDonRight = 1.0f;
            return 2;
        }
    } else {
        if (isLeft) {
            drumHitKatsuLeft = 1.0f;
            return 3;
        } else {
            drumHitKatsuRight = 1.0f;
            return 4;
        }
    }
}
