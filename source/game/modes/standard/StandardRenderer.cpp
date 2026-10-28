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
        visible = (songTime >= (obj.time - preempt) && (!obj.sliderDone || songTime <= obj.endTime + 150));
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

    // Calcular opacidad (Fade-In normal, Fade-Out en modo Hidden HD, y Fade-Out suave de 150ms al terminar sliders)
    float alphaMult = 1.0f;
    float timeSinceAppear = static_cast<float>(songTime - (obj.time - preempt));
    if (timeSinceAppear < fadeIn && fadeIn > 1.0f) {
        alphaMult = std::max(0.0f, std::min(1.0f, timeSinceAppear / fadeIn));
    }
    if (obj.isSlider && songTime > obj.endTime) {
        float fadeOutTime = static_cast<float>(songTime - obj.endTime);
        alphaMult *= std::max(0.0f, 1.0f - (fadeOutTime / 150.0f));
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
    u32 ringClr = C2D_Color32(activeSkin.sliderBorderColor.r, activeSkin.sliderBorderColor.g, activeSkin.sliderBorderColor.b, cAlpha);

    // Gradiente tubular 3D para el cuerpo del slider (sombreado Gouraud por hardware en la GPU PICA200)
    u32 sColorEdge, sColorCenter;
    if (activeSkin.hasSliderTrackOverride) {
        sColorEdge = C2D_Color32(
            static_cast<u8>(activeSkin.sliderTrackColor.r * 0.85f),
            static_cast<u8>(activeSkin.sliderTrackColor.g * 0.85f),
            static_cast<u8>(activeSkin.sliderTrackColor.b * 0.85f),
            sAlpha
        );
        u8 cenR = static_cast<u8>(std::min(255.0f, activeSkin.sliderTrackColor.r * 1.55f + 40.0f));
        u8 cenG = static_cast<u8>(std::min(255.0f, activeSkin.sliderTrackColor.g * 1.55f + 40.0f));
        u8 cenB = static_cast<u8>(std::min(255.0f, activeSkin.sliderTrackColor.b * 1.55f + 50.0f));
        u8 cenA = static_cast<u8>(std::min(255.0f, sAlpha * 1.22f));
        sColorCenter = C2D_Color32(cenR, cenG, cenB, cenA);
    } else {
        sColorEdge = C2D_Color32(
            static_cast<u8>(rgb.r * 0.46f),
            static_cast<u8>(rgb.g * 0.46f),
            static_cast<u8>(rgb.b * 0.46f),
            sAlpha
        );
        u8 cenR = static_cast<u8>(std::min(255.0f, rgb.r * 0.85f + 55.0f));
        u8 cenG = static_cast<u8>(std::min(255.0f, rgb.g * 0.85f + 55.0f));
        u8 cenB = static_cast<u8>(std::min(255.0f, rgb.b * 0.85f + 55.0f));
        u8 cenA = static_cast<u8>(std::min(255.0f, sAlpha * 1.25f));
        sColorCenter = C2D_Color32(cenR, cenG, cenB, cenA);
    }

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
        float rOuter = rScreen * 0.94f;
        float sBorder = std::max(1.8f, rOuter * 0.11f);
        float rInner = rOuter - sBorder;

        // 1. Cálculo de Snaking In y Snaking Out:
        // - Snaking In: al aparecer, la curva se va dibujando hacia adelante.
        // - Snaking Out: durante el deslizamiento, la bola va "comiendo" la pista por detrás
        //   de forma continua hasta llegar a su destino, sin desapariciones bruscas.
        float startFrac = 0.0f;
        float endFrac   = 1.0f;

        float totalDuration = static_cast<float>(std::max(1, obj.endTime - obj.time));
        float slideDur = totalDuration / static_cast<float>(std::max(1, obj.repeats));
        int curSpan = 0;
        float spanProgress = 0.0f;
        if (songTime > obj.time && obj.endTime > obj.time) {
            float elapsed = static_cast<float>(songTime - obj.time);
            curSpan = static_cast<int>(elapsed / slideDur);
            if (curSpan >= obj.repeats) curSpan = obj.repeats - 1;
            spanProgress = std::max(0.0f, std::min(1.0f, (elapsed - curSpan * slideDur) / slideDur));
        }
        bool isLastRepeat = (curSpan == obj.repeats - 1);

        if (ctx.settings.snakingSliders) {
            if (songTime < obj.time) {
                // Snaking In
                float timeSinceAppear = static_cast<float>(songTime - (obj.time - preempt));
                float snakeDuration = std::max(100.0f, preempt * 0.66f);
                float t = std::max(0.0f, std::min(1.0f, timeSinceAppear / snakeDuration));
                float sIn = std::sin(t * (static_cast<float>(M_PI) * 0.5f));
                endFrac = std::max(0.04f, std::min(1.0f, sIn));
            } else if (songTime <= obj.endTime) {
                // Snaking Out (comiendo la pista detrás de la bola en la repetición final o sliders de 1 solo pase)
                if (isLastRepeat) {
                    if (curSpan % 2 == 0) {
                        // Viaje hacia la cola: la bola come la pista desde la cabeza
                        startFrac = spanProgress;
                        endFrac   = 1.0f;
                    } else {
                        // Viaje de regreso hacia la cabeza: la bola come la pista desde la cola
                        startFrac = 0.0f;
                        endFrac   = 1.0f - spanProgress;
                    }
                }
            } else {
                // Después de endTime: completamente consumido por la bola
                startFrac = 1.0f;
                endFrac   = 1.0f;
            }
        }

        // Construir la lista de puntos visibles recortada entre startFrac y endFrac
        std::vector<Vec2> visiblePath;
        if (endFrac - startFrac <= 0.005f || fullPtsCount < 2) {
            visiblePath.clear();
        } else if (startFrac <= 0.001f && endFrac >= 0.999f) {
            visiblePath = obj.sliderPath;
        } else {
            visiblePath.reserve(fullPtsCount + 2);
            float startIdxFloat = startFrac * static_cast<float>(fullPtsCount - 1);
            float endIdxFloat   = endFrac   * static_cast<float>(fullPtsCount - 1);

            int sIdx = static_cast<int>(startIdxFloat);
            float sFrac = startIdxFloat - static_cast<float>(sIdx);
            Vec2 startPt = (sIdx + 1 < static_cast<int>(fullPtsCount))
                ? Vec2(obj.sliderPath[sIdx].x + (obj.sliderPath[sIdx + 1].x - obj.sliderPath[sIdx].x) * sFrac,
                       obj.sliderPath[sIdx].y + (obj.sliderPath[sIdx + 1].y - obj.sliderPath[sIdx].y) * sFrac)
                : obj.sliderPath.back();
            visiblePath.push_back(startPt);

            for (int p = sIdx + 1; p < static_cast<int>(fullPtsCount) && static_cast<float>(p) < endIdxFloat; ++p) {
                visiblePath.push_back(obj.sliderPath[p]);
            }

            int eIdx = static_cast<int>(endIdxFloat);
            float eFrac = endIdxFloat - static_cast<float>(eIdx);
            Vec2 endPt = (eIdx + 1 < static_cast<int>(fullPtsCount))
                ? Vec2(obj.sliderPath[eIdx].x + (obj.sliderPath[eIdx + 1].x - obj.sliderPath[eIdx].x) * eFrac,
                       obj.sliderPath[eIdx].y + (obj.sliderPath[eIdx + 1].y - obj.sliderPath[eIdx].y) * eFrac)
                : obj.sliderPath.back();

            float dLast = std::abs(endPt.x - visiblePath.back().x) + std::abs(endPt.y - visiblePath.back().y);
            if (dLast > 0.15f) {
                visiblePath.push_back(endPt);
            }
        }

        if (visiblePath.size() >= 2) {
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

                // Escala miter suave para curvas y codos sin estrangulamiento
                if (p > 0 && p < nPts - 1) {
                    Vec2 v0 = Vec2(pts[p].x - pts[p - 1].x, pts[p].y - pts[p - 1].y);
                    float l0 = std::sqrt(v0.x * v0.x + v0.y * v0.y);
                    if (l0 > 1e-4f) {
                        Vec2 n0 = Vec2(-v0.y / l0, v0.x / l0);
                        float dot = nrm[p].x * n0.x + nrm[p].y * n0.y;
                        if (dot > 0.45f) {
                            float miter = std::min(1.32f, 1.0f / dot);
                            nrm[p].x *= miter;
                            nrm[p].y *= miter;
                        }
                    }
                }
            }

            struct SliderVtx {
                Vec2 Lo, Li, C, Ri, Ro;
            };
            std::vector<SliderVtx> vSet(nPts);
            for (size_t p = 0; p < nPts; ++p) {
                vSet[p].C  = pts[p];
                vSet[p].Li = Vec2(pts[p].x + nrm[p].x * rInner, pts[p].y + nrm[p].y * rInner);
                vSet[p].Ri = Vec2(pts[p].x - nrm[p].x * rInner, pts[p].y - nrm[p].y * rInner);
                vSet[p].Lo = Vec2(pts[p].x + nrm[p].x * rOuter, pts[p].y + nrm[p].y * rOuter);
                vSet[p].Ro = Vec2(pts[p].x - nrm[p].x * rOuter, pts[p].y - nrm[p].y * rOuter);
            }

            float zBorder = objBaseZ + subStep * 1.0f;
            float zBody   = objBaseZ + subStep * 2.0f;

            // Pase 1: Bordes exteriores (ribetes laterales continuos y nítidos sin opacar la transparencia de la pista)
            for (size_t p = 0; p + 1 < nPts; ++p) {
                const auto& v0 = vSet[p];
                const auto& v1 = vSet[p + 1];
                // Ribete lateral izquierdo
                C2D_DrawTriangle(v0.Lo.x, v0.Lo.y, ringClr, v0.Li.x, v0.Li.y, ringClr, v1.Lo.x, v1.Lo.y, ringClr, zBorder);
                C2D_DrawTriangle(v0.Li.x, v0.Li.y, ringClr, v1.Li.x, v1.Li.y, ringClr, v1.Lo.x, v1.Lo.y, ringClr, zBorder);
                // Ribete lateral derecho
                C2D_DrawTriangle(v0.Ri.x, v0.Ri.y, ringClr, v0.Ro.x, v0.Ro.y, ringClr, v1.Ri.x, v1.Ri.y, ringClr, zBorder);
                C2D_DrawTriangle(v0.Ro.x, v0.Ro.y, ringClr, v1.Ro.x, v1.Ro.y, ringClr, v1.Ri.x, v1.Ri.y, ringClr, zBorder);
            }

            // Pase 2: Malla tubular 3D con sombreado Gouraud por hardware en la GPU
            for (size_t p = 0; p + 1 < nPts; ++p) {
                const auto& v0 = vSet[p];
                const auto& v1 = vSet[p + 1];
                // Mitad izquierda: v.Li (sColorEdge) -> v.C (sColorCenter)
                C2D_DrawTriangle(v0.Li.x, v0.Li.y, sColorEdge, v0.C.x, v0.C.y, sColorCenter, v1.Li.x, v1.Li.y, sColorEdge, zBody);
                C2D_DrawTriangle(v0.C.x, v0.C.y, sColorCenter, v1.C.x, v1.C.y, sColorCenter, v1.Li.x, v1.Li.y, sColorEdge, zBody);
                // Mitad derecha: v.C (sColorCenter) -> v.Ri (sColorEdge)
                C2D_DrawTriangle(v0.C.x, v0.C.y, sColorCenter, v0.Ri.x, v0.Ri.y, sColorEdge, v1.C.x, v1.C.y, sColorCenter, zBody);
                C2D_DrawTriangle(v0.Ri.x, v0.Ri.y, sColorEdge, v1.Ri.x, v1.Ri.y, sColorEdge, v1.C.x, v1.C.y, sColorCenter, zBody);
            }

            // Extremos redondeados (Caps) en cabeza y en la punta visible de la serpiente
            bool skinCircleActive = ctx.texSkinCircle.loaded && (obj.state == NOT_HIT);
            if (!skinCircleActive) {
                C2D_DrawCircleSolid(pts[0].x, pts[0].y, zBorder, rOuter, ringClr);
                C2D_DrawCircleSolid(pts[0].x, pts[0].y, zBody, rInner, sColorEdge);
                C2D_DrawCircleSolid(pts[0].x, pts[0].y, zBody + subStep * 0.1f, rInner * 0.52f, sColorCenter);
            } else {
                // Cuando la skin está activa y la cabeza aún no ha sido golpeada, rellenamos el cuerpo interior
                C2D_DrawCircleSolid(pts[0].x, pts[0].y, zBody, rInner, sColorEdge);
                C2D_DrawCircleSolid(pts[0].x, pts[0].y, zBody + subStep * 0.1f, rInner * 0.52f, sColorCenter);
            }
            C2D_DrawCircleSolid(pts.back().x, pts.back().y, zBorder, rOuter, ringClr);
            C2D_DrawCircleSolid(pts.back().x, pts.back().y, zBody, rInner, sColorEdge);
            C2D_DrawCircleSolid(pts.back().x, pts.back().y, zBody + subStep * 0.1f, rInner * 0.52f, sColorCenter);
        }

        // Anillo de la cola o destino: visible mientras el extremo no haya sido consumido por la bola
        if (!isLastRepeat || (curSpan % 2 == 0)) {
            Vec2 finalTail = obj.sliderPath.back();
            float tailX = playX(finalTail.x, dSlider);
            float tailY = playY(finalTail.y);
            u8 tailAlpha = static_cast<u8>(cAlpha * (endFrac >= 0.95f ? 1.0f : (endFrac * 0.85f)));
            if (tailAlpha > 5 && (!isLastRepeat || spanProgress < 0.98f)) {
                drawRing(tailX, tailY, rInner, sBorder, Ui::withAlpha(ringClr, tailAlpha), 28, objBaseZ + subStep * 3.0f);
            }
        }

        // Dibujar Slider Ticks pendientes (solo aquellos que no han sido consumidos ni golpeados)
        for (const auto& tk : obj.sliderTicks) {
            if (!tk.hit && tk.time >= songTime - 50) {
                float tkProgress = static_cast<float>(tk.time - obj.time) / std::max(1.0f, slideDur);
                bool visibleTick = false;
                if (songTime < obj.time) {
                    visibleTick = (endFrac >= tkProgress);
                } else if (!isLastRepeat) {
                    visibleTick = true;
                } else {
                    if (curSpan % 2 == 0) {
                        visibleTick = (tkProgress >= startFrac);
                    } else {
                        visibleTick = ((1.0f - tkProgress) >= (1.0f - endFrac));
                    }
                }
                if (visibleTick) {
                    float tkX = playX(tk.x, dSlider);
                    float tkY = playY(tk.y);
                    C2D_DrawCircleSolid(tkX, tkY, objBaseZ + subStep * 3.0f, 2.5f, Ui::withAlpha(ctx.clrWhite, cAlpha));
                    C2D_DrawCircleSolid(tkX, tkY, objBaseZ + subStep * 3.2f, 1.2f, Ui::withAlpha(sColorCenter, cAlpha));
                }
            }
        }

        // Flecha de rebote (Reverse Arrow) con rotación tangencial y pulso rítmico
        if (obj.repeats > 1 && curSpan + 1 < obj.repeats && endFrac >= 0.92f) {
            bool arrowAtTail = (curSpan % 2 == 0);
            float revX, revY, targetX, targetY;
            if (arrowAtTail) {
                const Vec2& pTip = obj.sliderPath.back();
                const Vec2& pPrev = obj.sliderPath[obj.sliderPath.size() - 2];
                revX = playX(pTip.x, dSlider);
                revY = playY(pTip.y);
                targetX = playX(pPrev.x, dSlider);
                targetY = playY(pPrev.y);
            } else {
                const Vec2& pTip = obj.sliderPath.front();
                const Vec2& pNext = obj.sliderPath[1];
                revX = playX(pTip.x, dSlider);
                revY = playY(pTip.y);
                targetX = playX(pNext.x, dSlider);
                targetY = playY(pNext.y);
            }

            float arrowAngle = std::atan2(targetY - revY, targetX - revX);
            float arrowPulse = 1.0f + 0.08f * std::sin(static_cast<float>(songTime) * 0.014f);
            float arrowSize = rScreen * 1.8f * arrowPulse;

            if (ctx.texSkinReverseArrow.loaded) {
                ctx.texSkinReverseArrow.drawCenteredSpriteRotated(
                    revX, revY,
                    arrowSize, objBaseZ + subStep * 4.0f,
                    arrowAngle, C2D_Color32(255, 255, 255, cAlpha), 0.0f
                );
            } else {
                drawReverseArrow(
                    revX, revY,
                    targetX, targetY,
                    rScreen * 0.72f * arrowPulse, Ui::withAlpha(ctx.clrWhite, cAlpha), objBaseZ + subStep * 4.0f
                );
            }
        }

        if (songTime >= obj.time && songTime <= obj.endTime) {
            Vec2 ballOsu = obj.getSliderBallPos(songTime);
            float bx = playX(ballOsu.x, dCircle);
            float by = playY(ballOsu.y);

            // Dirección tangencial de la bola a lo largo de la curva
            Vec2 ballNext = obj.getSliderBallPos(std::min(obj.endTime, songTime + 20));
            float ballAngle = 0.0f;
            float bdx = ballNext.x - ballOsu.x;
            float bdy = ballNext.y - ballOsu.y;
            if (bdx * bdx + bdy * bdy > 0.001f) {
                ballAngle = std::atan2(bdy, bdx);
            }

            if (ctx.texSkinSliderBall.loaded) {
                ctx.texSkinSliderBall.drawCenteredSpriteRotated(bx, by, rScreen * 1.85f, objBaseZ + subStep * 9.0f, ballAngle, ctx.clrWhite, 0.0f);
            } else {
                Ui::glow(bx, by, objBaseZ + subStep * 8.5f, rScreen * 1.50f, C2D_Color32(rgb.r, rgb.g, rgb.b, 130), 4);
                C2D_DrawCircleSolid(bx, by, objBaseZ + subStep * 9.0f, rScreen * 0.88f, C2D_Color32(rgb.r, rgb.g, rgb.b, 240));
                C2D_DrawCircleSolid(bx, by, objBaseZ + subStep * 9.2f, rScreen * 0.55f, sColorCenter);
                C2D_DrawCircleSolid(bx, by, objBaseZ + subStep * 9.5f, rScreen * 0.28f, ctx.clrWhite);
            }

            if (obj.sliderTracking) {
                float trackPulse = 1.0f + 0.04f * std::sin(static_cast<float>(songTime) * 0.015f);
                float followR = rScreen * 1.92f * trackPulse;
                u32 followClr = ctx.clrCursor;
                if (ctx.texSkinSliderFollow.loaded) {
                    ctx.texSkinSliderFollow.drawCenteredSprite(bx, by, followR * 2.0f, objBaseZ + subStep * 9.8f, followClr, 0.0f);
                } else {
                    drawRing(bx, by, followR, 2.2f, followClr, 32, objBaseZ + subStep * 9.8f);
                    drawRing(bx, by, followR - 1.2f, 1.2f, Ui::withAlpha(followClr, 140), 32, objBaseZ + subStep * 9.7f);
                }
            }
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

        // Escala estándar de osu!: en los skins el gráfico del círculo ocupa 118px de un lienzo de 128px (margen de 5px).
        // Para que el radio visible del círculo coincida exactamente con rScreen (diámetro 2*rScreen), escalamos por 128/118.
        constexpr float SKIN_CIRCLE_SCALE = 128.0f / 118.0f;
        float circleSize = (rScreen * 2.0f) * SKIN_CIRCLE_SCALE;
        if (ctx.texSkinCircle.loaded) {
            ctx.texSkinCircle.drawCenteredSprite(cx, cy, circleSize, objBaseZ + subStep * 5.0f, cColor, 1.0f);
            if (ctx.texSkinOverlay.loaded) {
                float overlayScale = (ctx.texSkinCircle.getNativeW() > 0.0f)
                    ? (ctx.texSkinOverlay.getNativeW() / ctx.texSkinCircle.getNativeW())
                    : 1.0f;
                float overlayZ = activeSkin.hitCircleOverlayAboveNumber
                    ? (objBaseZ + subStep * 7.5f)
                    : (objBaseZ + subStep * 6.0f);
                ctx.texSkinOverlay.drawCenteredSprite(cx, cy, circleSize * overlayScale, overlayZ, C2D_Color32(255, 255, 255, cAlpha), 0.0f);
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
                    float approachScale = (ctx.texSkinApproach.getNativeW() > 0.0f)
                        ? (ctx.texSkinApproach.getNativeW() / 128.0f)
                        : 1.0f;
                    float approachSize = (approachRadius * 2.0f) * SKIN_CIRCLE_SCALE * approachScale;
                    ctx.texSkinApproach.drawCenteredSprite(ax, cy, approachSize, objBaseZ + subStep * 8.0f, cColor, 1.0f);
                } else {
                    drawRing(ax, cy, approachRadius, 1.5f, cColor, 20, objBaseZ + subStep * 8.0f);
                }
            }
        }
    }
}

// Mod Flashlight (FL): oscurece la pantalla de juego dejando un haz de luz focal centrado en el cursor
if (isFlashlight && !ctx.inBreakPeriod && !ctx.isPaused) {
    float curX = std::max(0.0f, std::min(512.0f, ctx.cursorOsuX));
    float curY = std::max(0.0f, std::min(384.0f, ctx.cursorOsuY));
    float flX = baseOffsetX + curX * SCALE_FACTOR + dCursor;
    float flY = baseOffsetY + curY * SCALE_FACTOR;
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
            0.50f
        );
        C2D_DrawTriangle(
            flX + cos1 * flRInner, flY + sin1 * flRInner, clrDarkTrans,
            flX + cos1 * flR,      flY + sin1 * flR,      clrDarkSolid,
            flX + cos0 * flR,      flY + sin0 * flR,      clrDarkSolid,
            0.50f
        );

        // Área exterior completamente a oscuras
        C2D_DrawTriangle(
            flX + cos0 * flR,      flY + sin0 * flR,      clrDarkSolid,
            flX + cos1 * flR,      flY + sin1 * flR,      clrDarkSolid,
            flX + cos0 * flROuter, flY + sin0 * flROuter, clrDarkSolid,
            0.51f
        );
        C2D_DrawTriangle(
            flX + cos1 * flR,      flY + sin1 * flR,      clrDarkSolid,
            flX + cos1 * flROuter, flY + sin1 * flROuter, clrDarkSolid,
            flX + cos0 * flROuter, flY + sin0 * flROuter, clrDarkSolid,
            0.51f
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
    if (ctx.isPaused) return;

    const SkinProfile& activeSkin = ctx.getActiveSkin();
    // Capas Z del cursor (0.62..0.68) por debajo de las pantallas de espera/descanso (0.82+) y pausa (0.90+)
    // para que el puntero nunca sobrepase los paneles de espera ni los menús superpuestos.
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

        C2D_DrawCircleSolid(sx, sy, 0.62f, sRadius, smokeOuterClr);
        C2D_DrawCircleSolid(sx, sy, 0.625f, sRadius * 0.52f, smokeInnerClr);
    }

    // 2. Renderizado de Estela Dinámica y Fluida del Cursor (Cursor Trail)
    for (size_t t = 0; t < ctx.cursorTrail.size(); ++t) {
        float ratio = static_cast<float>(ctx.cursorTrail[t].life) / static_cast<float>(ctx.cursorTrail[t].maxLife);
        float tr = (5.2f * ratio + 0.8f) * ctx.settings.cursorScale;
        float cxClamped = std::max(0.0f, std::min(512.0f, ctx.cursorTrail[t].x));
        float cyClamped = std::max(0.0f, std::min(384.0f, ctx.cursorTrail[t].y));
        float tx = baseOffsetX + cxClamped * SCALE_FACTOR + dCursor;
        float ty = baseOffsetY + cyClamped * SCALE_FACTOR;

        u8 tAlpha = static_cast<u8>(145.0f * std::pow(ratio, 1.15f));
        u32 tClr = C2D_Color32(
            activeSkin.cursorTrailColor.r,
            activeSkin.cursorTrailColor.g,
            activeSkin.cursorTrailColor.b,
            tAlpha
        );

        if (ctx.texSkinCursorTrail.loaded) {
            float trailSize = tr * 4.6f;
            ctx.texSkinCursorTrail.drawCenteredSprite(tx, ty, trailSize, 0.64f, tClr, 0.0f);
        } else {
            C2D_DrawCircleSolid(tx, ty, 0.64f, tr, tClr);
            C2D_DrawCircleSolid(tx, ty, 0.645f, tr * 0.42f, Ui::rgba(255, 255, 255, static_cast<u8>(160.0f * ratio)));
        }

        if (t + 1 < ctx.cursorTrail.size()) {
            float nxClamped = std::max(0.0f, std::min(512.0f, ctx.cursorTrail[t + 1].x));
            float nyClamped = std::max(0.0f, std::min(384.0f, ctx.cursorTrail[t + 1].y));
            float nextX = baseOffsetX + nxClamped * SCALE_FACTOR + dCursor;
            float nextY = baseOffsetY + nyClamped * SCALE_FACTOR;
            float lineW = std::max(1.5f, tr * 1.3f);
            C2D_DrawLine(tx, ty, tClr, nextX, nextY, tClr, lineW, 0.638f);
        }
    }

    // 3. Cursor principal acotado al área de juego
    float expandFactor = (activeSkin.cursorExpand && anyHeld) ? 7.5f : 5.5f;
    float cursorRadius = expandFactor * ctx.settings.cursorScale;
    float curOsuX = std::max(0.0f, std::min(512.0f, ctx.cursorOsuX));
    float curOsuY = std::max(0.0f, std::min(384.0f, ctx.cursorOsuY));
    float curScreenX = baseOffsetX + curOsuX * SCALE_FACTOR + dCursor;
    float curScreenY = baseOffsetY + curOsuY * SCALE_FACTOR;
    if (ctx.texSkinCursor.loaded) {
        float cursorSpriteSize = cursorRadius * 4.4f;
        ctx.texSkinCursor.drawCenteredSprite(curScreenX, curScreenY, cursorSpriteSize, 0.67f, ctx.clrWhite, 0.0f);
    } else {
        Ui::glow(curScreenX, curScreenY, 0.66f, cursorRadius * 1.8f, Ui::withAlpha(ctx.clrCursor, 110), 3);
        C2D_DrawCircleSolid(curScreenX, curScreenY, 0.67f, cursorRadius, ctx.clrCursor);
        drawRing(curScreenX, curScreenY, cursorRadius, 1.3f, ctx.clrWhite, 20, 0.675f);
        C2D_DrawCircleSolid(curScreenX, curScreenY, 0.68f, 2.2f * ctx.settings.cursorScale, ctx.clrWhite);
    }
}

void renderBottomTablet(AppContext& ctx) {
    u32 pinkAccent = Ui::rgba(255, 102, 171);
    u32 cyanAccent = Ui::rgba(56, 210, 255);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick),
                 Ui::rgba(18, 12, 32), Ui::rgba(10, 6, 20), pinkAccent, 0.01f);

    float rx, ry, rw, rh;
    ctx.settings.getTabletRect(rx, ry, rw, rh);

    // Superficie del área activa de Tablet estilo osu!lazer
    Ui::panel(rx, ry, rw, rh, 0.10f, Ui::rgba(20, 14, 34, 215), pinkAccent);
    drawRectOutline(rx, ry, rw, rh, 1.4f, Ui::withAlpha(pinkAccent, 165), 0.15f);

    // Líneas guía centrales y esquinas iluminadas
    u32 gridClr = Ui::rgba(255, 102, 171, 35);
    C2D_DrawLine(rx + rw * 0.5f, ry + 2.0f, gridClr, rx + rw * 0.5f, ry + rh - 2.0f, gridClr, 1.0f, 0.12f);
    C2D_DrawLine(rx + 2.0f, ry + rh * 0.5f, gridClr, rx + rw - 2.0f, ry + rh * 0.5f, gridClr, 1.0f, 0.12f);

    // Medallón sutil de marca de agua en el centro del tablet
    float pulse = ctx.getBpmPulse();
    float mR = 22.0f + pulse * 3.0f;
    Ui::ring(ctx.settings.tabletCenterX, ctx.settings.tabletCenterY, 0.13f, mR, 1.5f, Ui::rgba(255, 102, 171, 55), 28);
    drawCenteredText(ctx.textBuf, ctx.settings.tabletCenterX, ctx.settings.tabletCenterY - 1.0f,
                     0.48f, Ui::rgba(255, 102, 171, 55), "osu!", 0.14f);

    if (ctx.kiaiGlow > 0.01f) {
        u8 tAlpha = static_cast<u8>(ctx.kiaiGlow * (55.0f + 110.0f * pulse));
        drawRectOutline(rx - 1.0f, ry - 1.0f, rw + 2.0f, rh + 2.0f, 2.2f, Ui::rgba(255, 195, 75, tAlpha), 0.17f);
    }

    // Barra inferior de información rápida estilo osu!lazer
    C2D_DrawRectSolid(0.0f, 216.0f, 0.25f, 320.0f, 24.0f, Ui::rgba(12, 8, 22, 230));
    C2D_DrawRectSolid(0.0f, 216.0f, 0.26f, 320.0f, 1.0f, Ui::withAlpha(pinkAccent, 95));
    std::string nowPlaying = truncateString(ctx.beatmap.title + " [" + ctx.beatmap.version + "]", 38);
    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 227.5f, 0.36f, Ui::rgba(220, 215, 240), nowPlaying.c_str(), 0.30f);

    // Indicador de toque acotado estrictamente al rectángulo del tablet
    if (ctx.isTouching && !ctx.isPaused) {
        bool touchingSkip = ctx.skipAvailable && (ctx.touch.px >= 195 && ctx.touch.px <= 318 && ctx.touch.py >= 185 && ctx.touch.py <= 240);
        if (!touchingSkip) {
            float tx = std::max(rx + 4.0f, std::min(rx + rw - 4.0f, static_cast<float>(ctx.touch.px)));
            float ty = std::max(ry + 4.0f, std::min(ry + rh - 4.0f, static_cast<float>(ctx.touch.py)));
            Ui::glow(tx, ty, 0.45f, 14.0f, Ui::withAlpha(cyanAccent, 130), 3);
            C2D_DrawCircleSolid(tx, ty, 0.48f, 4.5f, cyanAccent);
            C2D_DrawCircleSolid(tx, ty, 0.49f, 2.0f, ctx.clrWhite);
        }
    }

    // Botón inclinado SKIP >> estilo osu!lazer
    if (ctx.skipAvailable && !ctx.isPaused) {
        Ui::slantButton(204.0f, 182.0f, 108.0f, 30.0f, -5.0f, pinkAccent,
                        "SKIP >>", "Saltar Intro", ctx.textBuf, true, 0.85f);
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
}

} // namespace StandardMode
