#include "StandardMode.hpp"

namespace StandardMode {

void updateObjects(
    AppContext& ctx,
    int songTime,
    float hitRadius,
    float hit300,
    float hit100,
    float hit50,
    float scoreMult,
    bool isAutoplay,
    bool isRelax,
    bool isSpunOut,
    bool isSuddenDeath,
    bool isPerfect,
    bool anyHeld,
    bool& kActionDown,
    bool& allObjectsDone,
    bool& anyActiveSpinner
) {
    const SkinProfile& activeSkin = ctx.getActiveSkin();
    for (auto& obj : ctx.beatmap.circles) {
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
        bool stickSpin = (!ctx.isTouching && (std::abs(ctx.cpad.dx) > 28 || std::abs(ctx.cpad.dy) > 28 ||
                                          std::abs(ctx.cstick.dx) > 25 || std::abs(ctx.cstick.dy) > 25));
        if (isSpunOut && !isAutoplay) {
            float dAng = 0.092f;
            obj.spinnerProgress += dAng;
            obj.spinnerRotAngle += dAng;
            instantRPM = 286.0f;
            ctx.cursorOsuX = 256.0f + std::cos(obj.spinnerRotAngle) * 52.0f;
            ctx.cursorOsuY = 192.0f + std::sin(obj.spinnerRotAngle) * 52.0f;
            if (ctx.animTick % 2 == 0) {
                float spkR = 56.0f;
                float sx = 256.0f + std::cos(obj.spinnerRotAngle) * spkR;
                float sy = 192.0f + std::sin(obj.spinnerRotAngle) * spkR;
                ctx.spawnParticles(sx, sy, obj.spinnerCleared ? ctx.clrCursor : ctx.clr300, 1, 0.6f, 1.8f, 2.1f, 12);
            }
        } else if (((anyHeld || !ctx.settings.tapWithStylus) && ctx.isTouching) || stickSpin || isAutoplay) {
            float ang = 0.0f;
            if (stickSpin && !isAutoplay) {
                float sx = (std::abs(ctx.cstick.dx) > 25 || std::abs(ctx.cstick.dy) > 25) ? static_cast<float>(ctx.cstick.dx) : static_cast<float>(ctx.cpad.dx);
                float sy = (std::abs(ctx.cstick.dx) > 25 || std::abs(ctx.cstick.dy) > 25) ? static_cast<float>(-ctx.cstick.dy) : static_cast<float>(-ctx.cpad.dy);
                ang = std::atan2(sy, sx);
                ctx.cursorOsuX = 256.0f + std::cos(ang) * 52.0f;
                ctx.cursorOsuY = 192.0f + std::sin(ang) * 52.0f;
            } else {
                ang = std::atan2(ctx.cursorOsuY - 192.0f, ctx.cursorOsuX - 256.0f);
            }
            if (obj.spinnerAngleInit) {
                float dAng = ang - obj.lastSpinnerAngle;
                while (dAng >  static_cast<float>(M_PI)) dAng -= 2.0f * static_cast<float>(M_PI);
                while (dAng < -static_cast<float>(M_PI)) dAng += 2.0f * static_cast<float>(M_PI);
                float absDAng = std::fabs(dAng);
                obj.spinnerProgress += absDAng;
                obj.spinnerRotAngle += dAng;
                instantRPM = std::min(480.0f, (absDAng / (2.0f * static_cast<float>(M_PI))) * 3600.0f);

                if (absDAng > 0.05f && (ctx.animTick % 2 == 0)) {
                    float spkR = 56.0f;
                    float sx = 256.0f + std::cos(obj.spinnerRotAngle) * spkR;
                    float sy = 192.0f + std::sin(obj.spinnerRotAngle) * spkR;
                    ctx.spawnParticles(sx, sy, obj.spinnerCleared ? ctx.clrCursor : ctx.clr300, 1, 0.6f, 1.8f, 2.1f, 12);
                }
            }
            obj.lastSpinnerAngle = ang;
            obj.spinnerAngleInit = true;
        } else {
            obj.spinnerAngleInit = false;
        }
        ctx.spinnerRPM = ctx.spinnerRPM * 0.84f + instantRPM * 0.16f;

        float clearTarget = std::max(3.14f, obj.spinnerRequiredRads * 0.65f);
        if (obj.spinnerProgress >= clearTarget || isAutoplay) {
            if (!obj.spinnerCleared) {
                obj.spinnerCleared = true;
                ctx.spinnerClearAnim = 1.0f;
                ctx.audio.playHitSound(2);
                ctx.spawnParticles(256.0f, 192.0f, ctx.clrCursor, 12, 1.2f, 3.2f, 2.6f, 18);
            }
            int extraSpins = static_cast<int>((obj.spinnerProgress - clearTarget) / static_cast<float>(M_PI));
            while (obj.spinnerBonusCount < extraSpins) {
                obj.spinnerBonusCount++;
                ctx.score += static_cast<int>(1000 * scoreMult);
                ctx.hp = std::min(1.0f, ctx.hp + 0.02f);
                ctx.lastSpinnerBonus = obj.spinnerBonusCount * 1000;
                ctx.spinnerBonusAnim = 1.0f;
                ctx.audio.playHitSound(2);
                ctx.spawnParticles(256.0f, 192.0f, ctx.clr300, 6, 1.0f, 2.6f, 2.2f, 14);
            }
        }

        if (songTime >= obj.endTime) {
            obj.sliderDone = true;
            if (obj.spinnerProgress >= clearTarget || isAutoplay) {
                obj.state = HIT_300;
                ctx.audio.playHitSound(obj.hitSound);
                ctx.combo++;
                if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                ctx.score += static_cast<int>((300 + 300 * ctx.combo * 0.05f) * scoreMult);
                ctx.count300++;
                ctx.hp = std::min(1.0f, ctx.hp + 0.08f);
                ctx.judgementHistory.push_back({obj.endTime, HIT_300, 0.0f});
            } else if (obj.spinnerProgress >= obj.spinnerRequiredRads * 0.35f) {
                obj.state = HIT_100;
                ctx.audio.playHitSound(obj.hitSound);
                ctx.combo++;
                if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                ctx.score += static_cast<int>(100 * scoreMult);
                ctx.count100++;
                ctx.hp = std::min(1.0f, ctx.hp + 0.04f);
                ctx.judgementHistory.push_back({obj.endTime, HIT_100, 0.0f});
                if (isPerfect) {
                    ctx.triggerSuddenDeathFail();
                    break;
                }
            } else {
                obj.state = HIT_MISS;
                ctx.combo = 0;
                ctx.countMiss++;
                ctx.hp = std::max(0.0f, ctx.hp - 0.12f);
                ctx.triggerShake(3.2f, 11.0f);
                ctx.judgementHistory.push_back({obj.endTime, HIT_MISS, 0.0f});
                if (isSuddenDeath || isPerfect) {
                    ctx.triggerSuddenDeathFail();
                    break;
                }
            }
            if (obj.state != HIT_300 || ctx.settings.showHit300) {
                ctx.popups.push_back(HitPopup(256.0f, 192.0f, obj.state, hitRadius * SCALE_FACTOR, objBurstClr, 20));
            }
        }
        continue;
    }

    // 2. Evaluar seguimiento, Slider Ticks, rebotes y finalización de Sliders activos
    if (obj.isSlider && !obj.sliderDone && songTime >= obj.time) {
        Vec2 ballPos = obj.getSliderBallPos(songTime);
        float dx = ctx.cursorOsuX - ballPos.x;
        float dy = ctx.cursorOsuY - ballPos.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        obj.sliderTracking = isAutoplay || (anyHeld && dist <= hitRadius * 2.4f) || (isRelax && dist <= hitRadius * 2.4f);

        if (obj.sliderTracking && (ctx.animTick % 2 == 0)) {
            u32 spkClr = (ctx.animTick % 4 == 0) ? ctx.clrCursor : objBurstClr;
            ctx.spawnParticles(ballPos.x, ballPos.y, spkClr, 1, 0.4f, 1.5f, 2.0f, 12);
        }

        // Evaluar Slider Ticks intermedios
        for (auto& tk : obj.sliderTicks) {
            if (!tk.hit && songTime >= tk.time) {
                tk.hit = true;
                if (obj.sliderTracking && obj.state != HIT_MISS) {
                    ctx.combo++;
                    if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                    ctx.score += static_cast<int>(10 * scoreMult);
                    ctx.hp = std::min(1.0f, ctx.hp + 0.015f);
                    ctx.spawnParticles(tk.x, tk.y, ctx.clr300, 5, 0.7f, 2.0f, 2.2f, 13);
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
                    ctx.audio.playHitSound(obj.getEdgeHitSound(curRep));
                    ctx.combo++;
                    if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                    ctx.score += static_cast<int>(30 * scoreMult);
                    ctx.hp = std::min(1.0f, ctx.hp + 0.02f);
                    ctx.spawnParticles(ballPos.x, ballPos.y, ctx.clrCursor, 7, 0.8f, 2.3f, 2.3f, 14);
                }
            }
        }

        if (songTime >= obj.endTime) {
            obj.sliderDone = true;
            if (obj.state != HIT_MISS && obj.sliderTracking) {
                ctx.audio.playHitSound(obj.getEdgeHitSound(obj.repeats));
                ctx.combo++;
                if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                ctx.score += static_cast<int>((300 + 300 * ctx.combo * 0.05f) * scoreMult);
                ctx.count300++;
                ctx.hp = std::min(1.0f, ctx.hp + 0.05f);
                ctx.judgementHistory.push_back({obj.endTime, HIT_300, 0.0f});
                if (ctx.settings.showHit300) {
                    ctx.popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_300, hitRadius * SCALE_FACTOR, objBurstClr, 18));
                    ctx.spawnParticles(ballPos.x, ballPos.y, objBurstClr, 7, 0.8f, 2.3f, 2.3f, 15);
                }
            } else if (obj.state == NOT_HIT) {
                obj.state = HIT_MISS;
                ctx.combo = 0;
                ctx.countMiss++;
                ctx.hp = std::max(0.0f, ctx.hp - 0.10f);
                ctx.judgementHistory.push_back({obj.endTime, HIT_MISS, 0.0f});
                ctx.popups.push_back(HitPopup(ballPos.x, ballPos.y, HIT_MISS, hitRadius * SCALE_FACTOR, ctx.clrMiss, 22));
                ctx.triggerShake(2.8f, 9.0f);
                if (isSuddenDeath || isPerfect) {
                    ctx.triggerSuddenDeathFail();
                    break;
                }
            }
        }
    }

    if (obj.state != NOT_HIT || obj.isSpinner) continue;

    // 3. Evaluar Miss de la cabeza de la nota
    if (songTime > (obj.time + hit50)) {
        obj.state = HIT_MISS;
        ctx.combo = 0;
        ctx.countMiss++;
        ctx.hp = std::max(0.0f, ctx.hp - 0.12f);
        ctx.judgementHistory.push_back({obj.time, HIT_MISS, 0.0f});
        ctx.popups.push_back(HitPopup(obj.x, obj.y, HIT_MISS, hitRadius * SCALE_FACTOR, ctx.clrMiss, 22));
        ctx.triggerShake(2.8f, 9.0f);
        if (isSuddenDeath || isPerfect) {
            ctx.triggerSuddenDeathFail();
            break;
        }
        continue;
    }

    // 4. Evaluar golpe (Tap) en la nota (con soporte para Mod Relax)
    if (isRelax && !isAutoplay && !kActionDown) {
        float dx = ctx.cursorOsuX - obj.x;
        float dy = ctx.cursorOsuY - obj.y;
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
            float dx = ctx.cursorOsuX - obj.x;
            float dy = ctx.cursorOsuY - obj.y;
            float dist = isAutoplay ? 0.0f : std::sqrt(dx * dx + dy * dy);

            if (dist <= hitRadius * 1.2f) {
                if (timeDelta <= hit300) {
                    obj.state = HIT_300;
                    ctx.score += static_cast<int>((300 + 300 * ctx.combo * 0.05f) * scoreMult);
                    ctx.count300++;
                    ctx.hp = std::min(1.0f, ctx.hp + 0.06f);
                } else if (timeDelta <= hit100) {
                    obj.state = HIT_100;
                    ctx.score += static_cast<int>((100 + 100 * ctx.combo * 0.05f) * scoreMult);
                    ctx.count100++;
                    ctx.hp = std::min(1.0f, ctx.hp + 0.025f);
                    if (isPerfect) {
                        ctx.triggerSuddenDeathFail();
                        break;
                    }
                } else {
                    obj.state = HIT_50;
                    ctx.score += static_cast<int>((50 + 50 * ctx.combo * 0.05f) * scoreMult);
                    ctx.count50++;
                    if (isPerfect) {
                        ctx.triggerSuddenDeathFail();
                        break;
                    }
                }
                int headSound = obj.isSlider ? obj.getEdgeHitSound(0) : obj.hitSound;
                ctx.audio.playHitSound(headSound);
                ctx.combo++;
                if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                if (obj.isSlider) obj.sliderTracking = true;
                ctx.judgementHistory.push_back({obj.time, obj.state, signedErrMs});
                if (obj.state != HIT_300 || ctx.settings.showHit300) {
                    ctx.popups.push_back(HitPopup(obj.x, obj.y, obj.state, hitRadius * SCALE_FACTOR, objBurstClr, 18));
                    ctx.spawnParticles(obj.x, obj.y, (obj.state == HIT_300) ? objBurstClr : ctx.clr100,
                                   (obj.state == HIT_300) ? 7 : 4, 0.8f, 2.4f, 2.3f, 15);
                }

                // Registrar marca en el Hit Error Meter y estadisticas de UR
                ctx.hitErrorTicks.push_back({signedErrMs, obj.state, 210});
                if (ctx.hitErrorTicks.size() > 28) ctx.hitErrorTicks.erase(ctx.hitErrorTicks.begin());
                ctx.allHitErrors.push_back(signedErrMs);
                ctx.hitErrorNeedle = ctx.hitErrorNeedle * 0.78f + signedErrMs * 0.22f;

                if (isAutoplay) ctx.k1PressCount++;
                kActionDown = false;
                break;
            }
        }
    }
}
}

} // namespace StandardMode
