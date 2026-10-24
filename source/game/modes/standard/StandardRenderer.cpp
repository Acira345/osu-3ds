#include "StandardMode.hpp"

namespace StandardMode {

void renderPlayfieldObjects(
    AppContext& ctx,
    float screenW,
    float baseOffsetX,
    float baseOffsetY,
    float eyeSign,
    float slider3DVal,
    int songTime,
    float preempt,
    float fadeIn,
    float hit50,
    float rScreen,
    float bpmPulse,
    bool isHidden,
    bool isFlashlight,
    const std::vector<RGBColor>& activePalette
) {
    const SkinProfile& activeSkin = ctx.getActiveSkin();
    auto depthShift = [&](float px) { return eyeSign * slider3DVal * px; };
    float dPlayfield = depthShift(0.4f);
    float dSlider    = depthShift(2.6f);
    float dCircle    = depthShift(2.6f);
    float dFrontHud  = depthShift(3.6f);
    float dCursor    = depthShift(4.6f);
    auto playX = [&](float osuX, float dOffset = 0.0f) { return baseOffsetX + (osuX * SCALE_FACTOR) + ctx.shakeX + dOffset; };
    auto playY = [&](float osuY) { return baseOffsetY + (osuY * SCALE_FACTOR) + ctx.shakeY; };
    // 0. Follow Points entre notas consecutivas del mismo combo
    for (size_t i = 0; i + 1 < ctx.beatmap.circles.size(); ++i) {
    const auto& a = ctx.beatmap.circles[i];
    const auto& b = ctx.beatmap.circles[i + 1];
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
for (int i = 0; i < static_cast<int>(ctx.beatmap.circles.size()); ++i) {
    const auto& obj = ctx.beatmap.circles[i];
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
    const auto& obj = ctx.beatmap.circles[i];
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
        C2D_DrawRectSolid(baseOffsetX + ctx.shakeX + dPlayfield, baseOffsetY + ctx.shakeY, 0.025f, PLAYFIELD_W, PLAYFIELD_H, Ui::rgba(8, 6, 18, 95));

        u32 spinClr = obj.spinnerCleared ? ctx.clrCursor : ((fillRatio > 0.6f) ? ctx.clr100 : ctx.clr300);

        // Anillo exterior de tiempo en 3D (converge desde el frente hacia el disco al cerrarse)
        float approachR = 14.0f + 74.0f * (1.0f - timeRatio);
        float dSpApp = depthShift(2.6f + (1.0f - timeRatio) * 3.0f);
        drawRingZ(playX(256.0f, dSpApp), spY, 0.22f, approachR, 2.2f, Ui::withAlpha(ctx.clrWhite, 190), 32);

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
            C2D_DrawLine(xIn, yIn, ctx.clrWhite, xOut, yOut, spinClr, 2.2f, 0.24f);
        }
        C2D_DrawCircleSolid(spX, spY, 0.25f, 9.0f + (ctx.spinnerRPM / 480.0f) * 4.0f, spinClr);
        C2D_DrawCircleSolid(spX, spY, 0.26f, 4.5f, ctx.clrWhite);

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
            float cScale = 0.62f + ctx.spinnerClearAnim * 0.25f;
            drawCenteredText(ctx.textBuf, playX(256.0f, dFrontHud), spY - 68.0f, cScale, ctx.clrCursor, "CLEAR!", 0.32f);
        } else if (songTime >= obj.time) {
            drawCenteredText(ctx.textBuf, playX(256.0f, dFrontHud), spY - 66.0f, 0.48f, ctx.clrWhite, LOC(STR_GAME_SPIN), 0.32f);
        }

        if (ctx.lastSpinnerBonus > 0 && ctx.spinnerBonusAnim > 0.01f) {
            char bonStr[32];
            std::snprintf(bonStr, sizeof(bonStr), "+%d", ctx.lastSpinnerBonus);
            float bScale = 0.52f + ctx.spinnerBonusAnim * 0.20f;
            drawCenteredText(ctx.textBuf, playX(256.0f, dFrontHud), spY + 56.0f - (1.0f - ctx.spinnerBonusAnim) * 8.0f, bScale, ctx.clr300, bonStr, 0.33f);
        }

        // Medidor digital de RPM abajo del Spinner
        char rpmStr[32];
        std::snprintf(rpmStr, sizeof(rpmStr), "%.0f RPM", ctx.spinnerRPM);
        Ui::pill(spX - 36.0f, spY + 68.0f, 72.0f, 16.0f, 0.28f, Ui::rgba(20, 16, 34, 230));
        drawCenteredText(ctx.textBuf, spX, spY + 76.0f, 0.38f, spinClr, rpmStr, 0.32f);
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
        if (ctx.settings.snakingSliders && songTime < obj.time) {
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
                    C2D_DrawCircleSolid(playX(tk.x, dSlider), playY(tk.y), objBaseZ + subStep * 3.0f, 2.2f, Ui::withAlpha(ctx.clrWhite, cAlpha));
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
                    rScreen * 0.65f, Ui::withAlpha(ctx.clrWhite, cAlpha), objBaseZ + subStep * 4.0f
                );
            } else {
                const Vec2& pTip = obj.sliderPath.front();
                const Vec2& pNext = obj.sliderPath[1];
                drawReverseArrow(
                    playX(pTip.x, dSlider), playY(pTip.y),
                    playX(pNext.x, dSlider), playY(pNext.y),
                    rScreen * 0.65f, Ui::withAlpha(ctx.clrWhite, cAlpha), objBaseZ + subStep * 4.0f
                );
            }
        }

        if (songTime >= obj.time && songTime <= obj.endTime) {
            Vec2 ballOsu = obj.getSliderBallPos(songTime);
            float bx = playX(ballOsu.x, dCircle);
            float by = playY(ballOsu.y);
            Ui::glow(bx, by, objBaseZ + subStep * 8.5f, rScreen * 1.45f, C2D_Color32(rgb.r, rgb.g, rgb.b, 120), 3);
            C2D_DrawCircleSolid(bx, by, objBaseZ + subStep * 9.0f, rScreen * 0.88f, C2D_Color32(rgb.r, rgb.g, rgb.b, 240));
            C2D_DrawCircleSolid(bx, by, objBaseZ + subStep * 9.5f, rScreen * 0.35f, ctx.clrWhite);
            float followR = obj.sliderTracking ? (rScreen * 1.9f) : (rScreen * 1.3f);
            u32 followClr = obj.sliderTracking ? ctx.clrCursor : ctx.clrWhite;
            drawRing(bx, by, followR, 1.5f, followClr, 20, objBaseZ + subStep * 9.8f);
        }
    }

    // 2. Cabeza del círculo y número (con soporte para Sprites PNG de Skin o Vectorial 60 FPS + Profundidad 3D)
    if (obj.state == NOT_HIT && songTime <= obj.time + hit50 && cAlpha > 8) {
        float cx = playX(obj.x, dCircle);
        float cy = playY(obj.y);

        // Resplandor radial suave alrededor de la nota (con boost durante Kiai Time)
        float kiaiGlowBoost = (ctx.kiaiGlow > 0.01f) ? (0.24f * ctx.kiaiGlow * bpmPulse) : 0.0f;
        u8 glowA = static_cast<u8>(std::min(255.0f, (85.0f + 50.0f * ctx.kiaiGlow * bpmPulse) * alphaMult));
        Ui::glow(cx, cy, objBaseZ + subStep * 4.5f, rScreen * (1.42f + kiaiGlowBoost), C2D_Color32(rgb.r, rgb.g, rgb.b, glowA), 3);

        if (ctx.texSkinCircle.loaded) {
            ctx.texSkinCircle.drawCenteredSprite(cx, cy, rScreen * 1.05f, objBaseZ + subStep * 5.0f, cColor);
            if (ctx.texSkinOverlay.loaded) {
                ctx.texSkinOverlay.drawCenteredSprite(cx, cy, rScreen * 1.05f, objBaseZ + subStep * 6.0f, C2D_Color32(255, 255, 255, cAlpha));
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
        drawCenteredText(ctx.textBuf, cx, cy, 0.55f, C2D_Color32(255, 255, 255, cAlpha), numStr, objBaseZ + subStep * 7.0f);

        // Anillo de aproximación 3D: nace más cerca del jugador y aterriza en el plano del círculo al llegar a 0 ms
        if (!isHidden) {
            float timeLeft = static_cast<float>(obj.time - songTime);
            if (timeLeft > 0.0f) {
                float progress = timeLeft / preempt;
                float approachRadius = rScreen * (1.0f + 2.5f * progress);
                float dApproach = depthShift(2.6f + progress * 3.4f);
                float ax = playX(obj.x, dApproach);
                if (ctx.texSkinApproach.loaded) {
                    ctx.texSkinApproach.drawCenteredSprite(ax, cy, approachRadius, objBaseZ + subStep * 8.0f, cColor);
                } else {
                    drawRing(ax, cy, approachRadius, 1.5f, cColor, 20, objBaseZ + subStep * 8.0f);
                }
            }
        }
    }
}

// Mod Flashlight (FL): oscurece la pantalla de juego dejando un haz de luz focal centrado en el cursor
if (isFlashlight) {
    float flX = baseOffsetX + ctx.cursorOsuX * SCALE_FACTOR + dCursor;
    float flY = baseOffsetY + ctx.cursorOsuY * SCALE_FACTOR;
    float flR = (ctx.combo >= 200) ? 46.0f : ((ctx.combo >= 100) ? 60.0f : 78.0f);
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

void renderCursorAndSmoke(
    AppContext& ctx,
    float baseOffsetX,
    float baseOffsetY,
    float dCursor,
    bool anyHeld
) {
    const SkinProfile& activeSkin = ctx.getActiveSkin();
    // 1. Partículas de Humo estilo osu! 'C'
    for (const auto& sp : ctx.smokeParticles) {
        float ratio = static_cast<float>(sp.life) / static_cast<float>(sp.maxLife);
        float fadeAlpha = std::pow(ratio, 1.35f);
        u8 aOuter = static_cast<u8>(90.0f * fadeAlpha);
        u8 aInner = static_cast<u8>(155.0f * fadeAlpha);
        float sRadius = sp.size * SCALE_FACTOR * ctx.settings.cursorScale;
        float sx = baseOffsetX + sp.x * SCALE_FACTOR + dCursor;
        float sy = baseOffsetY + sp.y * SCALE_FACTOR;

        u32 smokeOuterClr = Ui::rgba(210, 218, 235, aOuter);
        u32 smokeInnerClr = Ui::rgba(235, 240, 252, aInner);

        C2D_DrawCircleSolid(sx, sy, 0.84f, sRadius, smokeOuterClr);
        C2D_DrawCircleSolid(sx, sy, 0.845f, sRadius * 0.52f, smokeInnerClr);
    }

    // 2. Renderizado de Estela Dinámica y Fluida del Cursor (Cursor Trail)
    for (size_t t = 0; t < ctx.cursorTrail.size(); ++t) {
        float ratio = static_cast<float>(ctx.cursorTrail[t].life) / static_cast<float>(ctx.cursorTrail[t].maxLife);
        float tr = (5.2f * ratio + 0.8f) * ctx.settings.cursorScale;
        float tx = baseOffsetX + ctx.cursorTrail[t].x * SCALE_FACTOR + dCursor;
        float ty = baseOffsetY + ctx.cursorTrail[t].y * SCALE_FACTOR;

        u8 tAlpha = static_cast<u8>(145.0f * std::pow(ratio, 1.15f));
        u32 tClr = C2D_Color32(
            activeSkin.cursorTrailColor.r,
            activeSkin.cursorTrailColor.g,
            activeSkin.cursorTrailColor.b,
            tAlpha
        );

        if (ctx.texSkinCursorTrail.loaded) {
            ctx.texSkinCursorTrail.drawCenteredSprite(tx, ty, tr * 2.4f, 0.86f, tClr);
        } else {
            // Núcleo brillante y halo translúcido
            C2D_DrawCircleSolid(tx, ty, 0.86f, tr, tClr);
            C2D_DrawCircleSolid(tx, ty, 0.865f, tr * 0.42f, Ui::rgba(255, 255, 255, static_cast<u8>(160.0f * ratio)));
        }

        // Conectar con segmento hacia el punto siguiente para que la estela sea continua y sin huecos
        if (t + 1 < ctx.cursorTrail.size()) {
            float nextX = baseOffsetX + ctx.cursorTrail[t + 1].x * SCALE_FACTOR + dCursor;
            float nextY = baseOffsetY + ctx.cursorTrail[t + 1].y * SCALE_FACTOR;
            float lineW = std::max(1.5f, tr * 1.3f);
            C2D_DrawLine(tx, ty, tClr, nextX, nextY, tClr, lineW, 0.858f);
        }
    }

    // 3. Cursor principal
    float expandFactor = (activeSkin.cursorExpand && anyHeld) ? 7.5f : 5.5f;
    float cursorRadius = expandFactor * ctx.settings.cursorScale;
    float curScreenX = baseOffsetX + ctx.cursorOsuX * SCALE_FACTOR + dCursor;
    float curScreenY = baseOffsetY + ctx.cursorOsuY * SCALE_FACTOR;
    if (ctx.texSkinCursor.loaded) {
        ctx.texSkinCursor.drawCenteredSprite(curScreenX, curScreenY, cursorRadius * 1.4f, 0.88f, C2D_Color32(255, 255, 255, 255));
    } else {
        Ui::glow(curScreenX, curScreenY, 0.87f, cursorRadius * 1.8f, Ui::withAlpha(ctx.clrCursor, 110), 3);
        C2D_DrawCircleSolid(curScreenX, curScreenY, 0.88f, cursorRadius, ctx.clrCursor);
        C2D_DrawCircleSolid(curScreenX, curScreenY, 0.89f, 2.0f * ctx.settings.cursorScale, ctx.clrWhite);
    }
}

void renderBottomTablet(AppContext& ctx) {
    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(22, 16, 38), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);
    float rx, ry, rw, rh;
    ctx.settings.getTabletRect(rx, ry, rw, rh);

    Ui::panel(rx, ry, rw, rh, 0.1f, C2D_Color32(28, 24, 44, 235), ctx.clrPrimary);
    u32 gridClr = C2D_Color32(95, 80, 145, 70);
    C2D_DrawLine(rx + rw * 0.5f, ry, gridClr, rx + rw * 0.5f, ry + rh, gridClr, 1.0f, 0.14f);
    C2D_DrawLine(rx, ry + rh * 0.5f, gridClr, rx + rw, ry + rh * 0.5f, gridClr, 1.0f, 0.14f);
    drawRectOutline(rx, ry, rw, rh, 1.5f, C2D_Color32(120, 95, 185, 200), 0.16f);
    if (ctx.kiaiGlow > 0.01f) {
        float pulse = ctx.getBpmPulse();
        u8 tAlpha = static_cast<u8>(ctx.kiaiGlow * (45.0f + 110.0f * pulse));
        drawRectOutline(rx - 1.0f, ry - 1.0f, rw + 2.0f, rh + 2.0f, 2.0f, Ui::rgba(255, 195, 75, tAlpha), 0.17f);
    }
    drawRing(ctx.settings.tabletCenterX, ctx.settings.tabletCenterY, 12.0f, 1.2f, C2D_Color32(110, 95, 165, 180), 18, 0.18f);

    // Barra inferior de información rápida de la canción
    Ui::gradV(0.0f, 218.0f, 320.0f, 22.0f, 0.25f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 240));
    std::string nowPlaying = truncateString(ctx.beatmap.title + " [" + ctx.beatmap.version + "]", 42);
    drawCenteredText(ctx.textBuf, 160.0f, 229.0f, 0.36f, ctx.clrMuted, nowPlaying.c_str(), 0.3f);

    if (ctx.isTouching) {
        Ui::glow(ctx.touch.px, ctx.touch.py, 0.45f, 12.0f, Ui::withAlpha(ctx.clrKeyActive, 130), 3);
        C2D_DrawCircleSolid(ctx.touch.px, ctx.touch.py, 0.5f, 4.0f, ctx.clrKeyActive);
    }

    if (ctx.skipAvailable && !ctx.isPaused) {
        float skipPulse = 0.5f + 0.5f * std::sin(ctx.animTick * 0.25f);
        u32 sBg = Ui::lerpColor(Ui::rgba(255, 80, 140, 240), Ui::rgba(255, 120, 180, 250), skipPulse);
        Ui::panel(212.0f, 196.0f, 100.0f, 34.0f, 0.85f, sBg, ctx.clrWhite);
        drawCenteredText(ctx.textBuf, 262.0f, 208.0f, 0.44f, ctx.clrWhite, "SKIP >>", 0.90f);
        drawCenteredText(ctx.textBuf, 262.0f, 222.0f, 0.25f, Ui::rgba(255, 230, 242), LOC(STR_SKIP_HINT), 0.90f);
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
}

} // namespace StandardMode
