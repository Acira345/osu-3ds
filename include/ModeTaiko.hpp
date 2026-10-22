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

enum TaikoNoteType {
    TAIKO_DON = 0,       // Centro Rojo (Normal)
    TAIKO_KATSU = 1,     // Borde Azul (Normal)
    TAIKO_BIG_DON = 2,   // Centro Rojo Grande
    TAIKO_BIG_KATSU = 3, // Borde Azul Grande
    TAIKO_DRUMROLL = 4,  // Barra Amarilla de Redoble
    TAIKO_DENDEN = 5     // Globo / Spinner Taiko
};

struct TaikoNote {
    TaikoNoteType type = TAIKO_DON;
    int time = 0;
    int endTime = 0;
    HitState state = NOT_HIT;
    bool isHit = false;
    float scrollSpeed = 1.0f;
    int drumrollTicksHit = 0;
    int drumrollTotalTicks = 0;
    int dendenRequiredHits = 0;
    int dendenCurrentHits = 0;
};

class ModeTaiko {
public:
    std::vector<TaikoNote> notes;
    size_t nextNoteIndex = 0;

    // Estados visuales del tambor en pantalla inferior
    float drumHitDonLeft = 0.0f;
    float drumHitDonRight = 0.0f;
    float drumHitKatsuLeft = 0.0f;
    float drumHitKatsuRight = 0.0f;

    // Animación de impacto en el receptor (Hit Target)
    float targetBurstScale = 1.0f;
    u32 targetBurstColor = 0;
    float targetBurstAlpha = 0.0f;

    // Animación de mascota Don-chan
    float mascotBounce = 0.0f;

    // Indicador de Redoble (Fan / Abanico "Drum Roll")
    int drumrollActiveCounter = 0;
    float drumrollFanTimer = 0.0f;

    // Calificación temporal para popup sobre el receptor
    int lastHitJudgement = 0; // 300 (GOOD), 100 (OK), -1 (MISS)
    float hitJudgementTimer = 0.0f;

    // Emociones dinámicas de Don-chan y animaciones de celebración
    float missFaceTimer = 0.0f;
    float balloonPopTimer = 0.0f;
    int totalDrumrollHits = 0;

    // Constantes de resultado de golpe
    static constexpr int TAIKO_HIT_NONE   = 0;
    static constexpr int TAIKO_HIT_300    = 300;
    static constexpr int TAIKO_HIT_100    = 100;
    static constexpr int TAIKO_HIT_MISS   = -1;
    static constexpr int TAIKO_HIT_TICK   = 1; // Tick de redoble (da puntos y combo, NO penaliza precisión)
    static constexpr int TAIKO_HIT_DENDEN = 2; // Tick de denden (da puntos y combo, NO penaliza precisión)

    // Constantes de diseño del playfield de Taiko (Alineado con Arcade / 3DS)
    static constexpr float TRACK_Y = 89.0f;
    static constexpr float TARGET_X = 114.0f; // Receptor desplazado para dar espacio al combo drum & score
    static constexpr float NOTE_RADIUS = 18.0f;
    static constexpr float BIG_NOTE_RADIUS = 26.0f;

    // Colores oficiales estilo Taiko no Tatsujin 3DS
    static constexpr u32 CLR_DON_INNER = 0xFF3535FF;   // Rojo brillante C2D ABGR
    static constexpr u32 CLR_DON_OUTER = 0xFF1010CC;   // Rojo oscuro contorno
    static constexpr u32 CLR_KATSU_INNER = 0xFFFF9922; // Azul celeste brillante
    static constexpr u32 CLR_KATSU_OUTER = 0xFFCC5500; // Azul profundo contorno
    static constexpr u32 CLR_DRUMROLL = 0xFF22D8FF;    // Amarillo oro 3DS
    static constexpr u32 CLR_DRUMROLL_DARK = 0xFF0090EE; // Oro ámbar para sombras
    static constexpr u32 CLR_DENDEN = 0xFF33CCFF;
    static constexpr u32 CLR_WHITE = 0xFFFFFFFF;

private:
    static void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24, float z = 0.5f) {
        const float step = (2.0f * static_cast<float>(M_PI)) / segments;
        for (int i = 0; i < segments; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;
            C2D_DrawLine(
                x + std::cos(a0) * radius, y + std::sin(a0) * radius, color,
                x + std::cos(a1) * radius, y + std::sin(a1) * radius, color,
                thickness, z
            );
        }
    }

    static void drawEllipseFilled(float cx, float cy, float rx, float ry, u32 color, float z = 0.25f, int segs = 28) {
        const float step = (2.0f * static_cast<float>(M_PI)) / segs;
        for (int i = 0; i < segs; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;
            C2D_DrawTriangle(
                cx, cy, color,
                cx + std::cos(a0) * rx, cy + std::sin(a0) * ry, color,
                cx + std::cos(a1) * rx, cy + std::sin(a1) * ry, color,
                z
            );
        }
    }

    static void drawEllipseRing(float cx, float cy, float rx, float ry, float thickness, u32 color, float z = 0.26f, int segs = 28) {
        const float step = (2.0f * static_cast<float>(M_PI)) / segs;
        for (int i = 0; i < segs; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;
            C2D_DrawLine(
                cx + std::cos(a0) * rx, cy + std::sin(a0) * ry, color,
                cx + std::cos(a1) * rx, cy + std::sin(a1) * ry, color,
                thickness, z
            );
        }
    }

    static void drawSunburst(float cx, float cy, float rInner, float rOuter, int rays, u32 color, float z = 0.23f, float rot = 0.0f) {
        const float step = (2.0f * static_cast<float>(M_PI)) / rays;
        for (int i = 0; i < rays; ++i) {
            float aMid = i * step + rot;
            float a0 = aMid - step * 0.38f;
            float a1 = aMid + step * 0.38f;
            C2D_DrawTriangle(
                cx + std::cos(a0) * rInner, cy + std::sin(a0) * rInner, color,
                cx + std::cos(a1) * rInner, cy + std::sin(a1) * rInner, color,
                cx + std::cos(aMid) * rOuter, cy + std::sin(aMid) * rOuter, Ui::withAlpha(color, 90),
                z
            );
        }
    }

    // Remaches dorados tradicionales (Byo - 鋲)
    static void drawWadaikoByo(float cx, float cy, float rx, float ry, int count, u32 rivetClr, float z = 0.13f) {
        const float step = (2.0f * static_cast<float>(M_PI)) / count;
        for (int i = 0; i < count; ++i) {
            float ang = i * step;
            float bx = cx + std::cos(ang) * rx;
            float by = cy + std::sin(ang) * ry;
            C2D_DrawCircleSolid(bx, by + 0.6f, z, 2.2f, Ui::rgba(25, 18, 12, 220));
            C2D_DrawCircleSolid(bx, by, z + 0.002f, 1.8f, rivetClr);
            C2D_DrawCircleSolid(bx - 0.5f, by - 0.5f, z + 0.004f, 0.6f, CLR_WHITE);
        }
    }

public:
    // Carita auténtica de Taiko con emociones dinámicas
    // emotion: 0 = Normal feliz, 1 = Estrellitas / Go-Go Time, 2 = Llorando / Miss, 3 = Guiño festivo
    static void drawTaikoFace(float cx, float cy, float r, bool isBig, bool isDon, float z = 0.285f, int emotion = 0) {
        float eyeOffX = r * 0.28f;
        float eyeOffY = r * 0.08f;
        float eyeRx   = isBig ? 2.5f : 1.8f;
        float eyeRy   = isBig ? 3.8f : 2.7f;
        u32 eyeClr    = Ui::rgba(20, 16, 26);

        if (emotion == 2) {
            // Emoción 2: LLORANDO / MISS (Ojos "> <" apretados y lágrimas cómicas de frustración)
            float eyeW = isBig ? 3.8f : 2.6f;
            float eyeH = isBig ? 2.8f : 2.0f;
            C2D_DrawLine(cx - eyeOffX - eyeW, cy - eyeOffY - eyeH, eyeClr, cx - eyeOffX + eyeW * 0.3f, cy - eyeOffY, eyeClr, 1.6f, z + 0.005f);
            C2D_DrawLine(cx - eyeOffX + eyeW * 0.3f, cy - eyeOffY, eyeClr, cx - eyeOffX - eyeW, cy - eyeOffY + eyeH, eyeClr, 1.6f, z + 0.005f);
            C2D_DrawLine(cx + eyeOffX + eyeW, cy - eyeOffY - eyeH, eyeClr, cx + eyeOffX - eyeW * 0.3f, cy - eyeOffY, eyeClr, 1.6f, z + 0.005f);
            C2D_DrawLine(cx + eyeOffX - eyeW * 0.3f, cy - eyeOffY, eyeClr, cx + eyeOffX + eyeW, cy - eyeOffY + eyeH, eyeClr, 1.6f, z + 0.005f);

            // Lágrimas cómicas azules salpicando hacia afuera
            u32 tearClr = Ui::rgba(95, 205, 255, 230);
            C2D_DrawCircleSolid(cx - eyeOffX - eyeW - 3.5f, cy - eyeOffY + 1.0f, z + 0.006f, 1.8f, tearClr);
            C2D_DrawCircleSolid(cx + eyeOffX + eyeW + 3.5f, cy - eyeOffY + 1.0f, z + 0.006f, 1.8f, tearClr);

            // Boca ondulada triste "~"
            float mouthY = cy + r * 0.22f;
            C2D_DrawLine(cx - 4.0f, mouthY + 1.0f, eyeClr, cx, mouthY - 1.0f, eyeClr, 1.5f, z + 0.005f);
            C2D_DrawLine(cx, mouthY - 1.0f, eyeClr, cx + 4.0f, mouthY + 1.0f, eyeClr, 1.5f, z + 0.005f);

            // Mejillas sonrosadas bajas
            float chkOffX = r * 0.52f;
            drawEllipseFilled(cx - chkOffX, cy + r * 0.16f, 2.4f, 1.4f, Ui::rgba(255, 120, 150, 180), z + 0.003f);
            drawEllipseFilled(cx + chkOffX, cy + r * 0.16f, 2.4f, 1.4f, Ui::rgba(255, 120, 150, 180), z + 0.003f);
            return;
        }

        // 1. Cejas negras arqueadas
        float browOffY = eyeOffY + eyeRy + (isBig ? 2.6f : 1.9f);
        float browW    = isBig ? 3.6f : 2.4f;
        C2D_DrawLine(cx - eyeOffX - browW, cy - browOffY + 1.0f, eyeClr, cx - eyeOffX + browW * 0.6f, cy - browOffY, eyeClr, 1.4f, z + 0.004f);
        C2D_DrawLine(cx + eyeOffX + browW, cy - browOffY + 1.0f, eyeClr, cx + eyeOffX - browW * 0.6f, cy - browOffY, eyeClr, 1.4f, z + 0.004f);

        // 2. Ojitos
        if (emotion == 1) {
            // Emoción 1: ESTRELLITAS / GO-GO TIME (Estrellas doradas en los ojos)
            u32 starClr = Ui::rgba(255, 235, 80);
            drawEllipseFilled(cx - eyeOffX, cy - eyeOffY, eyeRx, eyeRy, eyeClr, z + 0.004f);
            drawEllipseFilled(cx + eyeOffX, cy - eyeOffY, eyeRx, eyeRy, eyeClr, z + 0.004f);

            // Estrella central de 4 puntas en cada ojo
            float sR = isBig ? 1.6f : 1.2f;
            C2D_DrawLine(cx - eyeOffX - sR, cy - eyeOffY, starClr, cx - eyeOffX + sR, cy - eyeOffY, starClr, 1.2f, z + 0.007f);
            C2D_DrawLine(cx - eyeOffX, cy - eyeOffY - sR, starClr, cx - eyeOffX, cy - eyeOffY + sR, starClr, 1.2f, z + 0.007f);
            C2D_DrawLine(cx + eyeOffX - sR, cy - eyeOffY, starClr, cx + eyeOffX + sR, cy - eyeOffY, starClr, 1.2f, z + 0.007f);
            C2D_DrawLine(cx + eyeOffX, cy - eyeOffY - sR, starClr, cx + eyeOffX, cy - eyeOffY + sR, starClr, 1.2f, z + 0.007f);
        } else {
            // Normal (emotion == 0 o 3)
            drawEllipseFilled(cx - eyeOffX, cy - eyeOffY, eyeRx, eyeRy, eyeClr, z + 0.004f);
            drawEllipseFilled(cx + eyeOffX, cy - eyeOffY, eyeRx, eyeRy, eyeClr, z + 0.004f);

            // Destellos especulares blancos en los ojos
            float spOffX = eyeRx * 0.35f;
            float spOffY = eyeRy * 0.35f;
            float spR    = isBig ? 0.95f : 0.70f;
            C2D_DrawCircleSolid(cx - eyeOffX - spOffX, cy - eyeOffY - spOffY, z + 0.007f, spR, CLR_WHITE);
            C2D_DrawCircleSolid(cx + eyeOffX - spOffX, cy - eyeOffY - spOffY, z + 0.007f, spR, CLR_WHITE);
        }

        // 3. Mejillas sonrosadas (colorete ovalado rosa alegre)
        float chkOffX = r * 0.52f;
        float chkOffY = r * 0.12f;
        float chkRx   = isBig ? 3.6f : 2.6f;
        float chkRy   = isBig ? 2.2f : 1.6f;
        u32 chkClr    = Ui::rgba(255, 120, 150, 220);
        drawEllipseFilled(cx - chkOffX, cy + chkOffY, chkRx, chkRy, chkClr, z + 0.003f);
        drawEllipseFilled(cx + chkOffX, cy + chkOffY, chkRx, chkRy, chkClr, z + 0.003f);

        // 4. Boquita sonriente abierta con lengua
        float mouthY = cy + r * 0.22f;
        float mouthR = isBig ? (emotion == 1 ? 4.0f : 3.4f) : (emotion == 1 ? 3.0f : 2.4f);
        u32 mouthInside = isDon ? Ui::rgba(135, 20, 30) : Ui::rgba(12, 55, 105);
        C2D_DrawCircleSolid(cx, mouthY, z + 0.004f, mouthR, mouthInside);
        // Lengua sonrosada
        C2D_DrawCircleSolid(cx, mouthY + mouthR * 0.35f, z + 0.005f, mouthR * 0.65f, Ui::rgba(255, 115, 130));
        // Labio superior sutil para curvatura sonriente
        C2D_DrawCircleSolid(cx, mouthY - mouthR * 0.45f, z + 0.006f, mouthR * 0.85f, isDon ? CLR_DON_INNER : CLR_KATSU_INNER);
        // Diente blanco superior
        C2D_DrawRectSolid(cx - mouthR * 0.35f, mouthY - mouthR * 0.12f, z + 0.007f, mouthR * 0.70f, 0.9f, CLR_WHITE);
    }

private:
    // Patrón geométrico hexagonal tradicional japonés (Kikkō - 亀甲) con motivos de flor de ciruelo
    static void drawKikkoPattern(float startX, float startY, float w, float h, float z = 0.03f) {
        const float r = 16.0f;
        const float hStep = r * 1.5f;
        const float vStep = r * 1.73205f;
        u32 lineClr = Ui::rgba(240, 208, 120, 55);
        u32 flowerClr = Ui::rgba(255, 235, 160, 80);

        int cols = static_cast<int>(w / hStep) + 2;
        int rows = static_cast<int>(h / vStep) + 2;

        for (int row = 0; row < rows; ++row) {
            float cy = startY + row * vStep;
            for (int col = 0; col < cols; ++col) {
                float cx = startX + col * hStep;
                if (col % 2 == 1) cy += vStep * 0.5f;

                // Dibujar hexágono
                const float aStep = static_cast<float>(M_PI) / 3.0f;
                for (int i = 0; i < 6; ++i) {
                    float a0 = i * aStep;
                    float a1 = (i + 1) * aStep;
                    C2D_DrawLine(
                        cx + std::cos(a0) * (r * 0.95f), cy + std::sin(a0) * (r * 0.95f), lineClr,
                        cx + std::cos(a1) * (r * 0.95f), cy + std::sin(a1) * (r * 0.95f), lineClr,
                        1.0f, z
                    );
                }

                // Pequeña flor de ciruelo central en celdas alternas
                if ((col + row) % 3 == 0) {
                    C2D_DrawCircleSolid(cx, cy, z + 0.001f, 1.8f, flowerClr);
                    for (int f = 0; f < 5; ++f) {
                        float fa = f * (2.0f * static_cast<float>(M_PI) / 5.0f);
                        C2D_DrawCircleSolid(cx + std::cos(fa) * 3.4f, cy + std::sin(fa) * 3.4f, z + 0.001f, 1.3f, flowerClr);
                    }
                }
            }
        }
    }

    // Dibuja una nube tradicional japonesa curvada (Yamato-e Kumogata)
    static void drawJapaneseCloud(float cx, float cy, float scale, u32 bodyClr, u32 borderClr, float z = 0.11f) {
        float r1 = 18.0f * scale;
        float r2 = 24.0f * scale;
        float r3 = 16.0f * scale;

        // Borde
        C2D_DrawCircleSolid(cx - 22.0f * scale, cy, z, r1 + 1.8f, borderClr);
        C2D_DrawCircleSolid(cx, cy - 6.0f * scale, z, r2 + 1.8f, borderClr);
        C2D_DrawCircleSolid(cx + 22.0f * scale, cy, z, r3 + 1.8f, borderClr);
        C2D_DrawRectSolid(cx - 26.0f * scale, cy - 2.0f * scale, z, 52.0f * scale, 16.0f * scale, borderClr);

        // Cuerpo interior
        C2D_DrawCircleSolid(cx - 22.0f * scale, cy, z + 0.002f, r1, bodyClr);
        C2D_DrawCircleSolid(cx, cy - 6.0f * scale, z + 0.002f, r2, bodyClr);
        C2D_DrawCircleSolid(cx + 22.0f * scale, cy, z + 0.002f, r3, bodyClr);
        C2D_DrawRectSolid(cx - 26.0f * scale, cy - 2.0f * scale, z + 0.002f, 52.0f * scale, 15.0f * scale, bodyClr);
    }

    // Dibuja un bailarín tradicional festivo japonés (Hyottoko / Okame con tamborcito Mitsudomoe)
    static void drawFestivalDancer(float cx, float baseY, float bounce, int variant, float z = 0.12f) {
        float jumpY = baseY - bounce * 6.5f;

        // Sombra en el suelo
        drawEllipseFilled(cx, baseY + 20.0f, 14.0f, 4.0f, Ui::rgba(180, 140, 60, 110), z - 0.002f);

        // Traje / Kimono festivo tradicional
        u32 robeClr = (variant % 2 == 0) ? Ui::rgba(45, 95, 60) : Ui::rgba(85, 45, 65);
        u32 sleeveClr = (variant % 2 == 0) ? Ui::rgba(35, 75, 48) : Ui::rgba(65, 32, 50);

        // Cuerpo
        C2D_DrawTriangle(
            cx - 11.0f, jumpY + 18.0f, robeClr,
            cx + 11.0f, jumpY + 18.0f, robeClr,
            cx, jumpY + 2.0f, robeClr,
            z + 0.002f
        );

        // Mangas abiertas sujetando el tambor
        C2D_DrawTriangle(cx - 16.0f, jumpY + 12.0f, sleeveClr, cx - 6.0f, jumpY + 4.0f, sleeveClr, cx - 10.0f, jumpY + 16.0f, sleeveClr, z + 0.003f);
        C2D_DrawTriangle(cx + 16.0f, jumpY + 12.0f, sleeveClr, cx + 6.0f, jumpY + 4.0f, sleeveClr, cx + 10.0f, jumpY + 16.0f, sleeveClr, z + 0.003f);

        // Manitas sujetando tamborcito o farolillo
        C2D_DrawCircleSolid(cx - 9.0f, jumpY + 14.0f, z + 0.005f, 2.2f, Ui::rgba(255, 230, 210));
        C2D_DrawCircleSolid(cx + 9.0f, jumpY + 14.0f, z + 0.005f, 2.2f, Ui::rgba(255, 230, 210));

        // Tambor de mano festivo (Den-den daiko / Linterna) en el centro
        float drumY = jumpY + 15.0f;
        C2D_DrawCircleSolid(cx, drumY, z + 0.006f, 6.5f, Ui::rgba(20, 16, 20));
        C2D_DrawCircleSolid(cx, drumY, z + 0.007f, 5.5f, Ui::rgba(240, 90, 45));
        C2D_DrawCircleSolid(cx, drumY, z + 0.008f, 4.2f, Ui::rgba(255, 240, 220));
        // Tomoe central
        C2D_DrawCircleSolid(cx, drumY, z + 0.009f, 2.0f, Ui::rgba(20, 16, 20));

        // Cabeza con capucha negra
        C2D_DrawCircleSolid(cx, jumpY - 4.0f, z + 0.004f, 10.0f, Ui::rgba(24, 20, 28));

        // Máscara blanca tradicional (Hyottoko / Okame)
        float maskY = jumpY - 3.5f;
        C2D_DrawCircleSolid(cx, maskY, z + 0.006f, 8.2f, Ui::rgba(255, 253, 248));

        // Cejas pintadas negras
        C2D_DrawLine(cx - 5.0f, maskY - 4.5f, Ui::rgba(20, 16, 20), cx - 1.5f, maskY - 3.5f, Ui::rgba(20, 16, 20), 1.2f, z + 0.008f);
        C2D_DrawLine(cx + 5.0f, maskY - 4.5f, Ui::rgba(20, 16, 20), cx + 1.5f, maskY - 3.5f, Ui::rgba(20, 16, 20), 1.2f, z + 0.008f);

        // Ojos
        C2D_DrawCircleSolid(cx - 3.2f, maskY - 1.5f, z + 0.008f, 1.2f, Ui::rgba(20, 16, 20));
        C2D_DrawCircleSolid(cx + 3.2f, maskY - 1.5f, z + 0.008f, 1.2f, Ui::rgba(20, 16, 20));

        // Mejillas rojas cómicas
        C2D_DrawCircleSolid(cx - 5.0f, maskY + 1.8f, z + 0.007f, 2.0f, Ui::rgba(255, 80, 95, 210));
        C2D_DrawCircleSolid(cx + 5.0f, maskY + 1.8f, z + 0.007f, 2.0f, Ui::rgba(255, 80, 95, 210));

        // Boquita redonda Hyottoko
        C2D_DrawCircleSolid(cx, maskY + 3.8f, z + 0.008f, 1.5f, Ui::rgba(210, 30, 40));
        C2D_DrawCircleSolid(cx, maskY + 3.8f, z + 0.009f, 0.7f, Ui::rgba(30, 10, 10));
    }

    // Peonza tradicional japonesa (Koma - 独楽) decorativa flotante
    static void drawKomaTop(float cx, float cy, float scale, float rot, float z = 0.115f) {
        float r = 10.0f * scale;
        // Mango superior
        C2D_DrawLine(cx, cy - r * 1.5f, Ui::rgba(190, 140, 80), cx, cy - r * 0.6f, Ui::rgba(190, 140, 80), 2.0f, z + 0.001f);
        // Cuerpo de platillo con bandas concéntricas
        C2D_DrawCircleSolid(cx, cy, z + 0.002f, r, Ui::rgba(240, 60, 50));
        C2D_DrawCircleSolid(cx, cy, z + 0.003f, r * 0.75f, Ui::rgba(255, 220, 70));
        C2D_DrawCircleSolid(cx, cy, z + 0.004f, r * 0.50f, Ui::rgba(45, 140, 210));
        C2D_DrawCircleSolid(cx, cy, z + 0.005f, r * 0.25f, CLR_WHITE);
        // Punta inferior
        C2D_DrawTriangle(cx - 2.5f, cy + r * 0.7f, Ui::rgba(180, 130, 70), cx + 2.5f, cy + r * 0.7f, Ui::rgba(180, 130, 70), cx, cy + r * 1.6f, Ui::rgba(180, 130, 70), z + 0.001f);
    }

    static void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f) {
        C2D_Text text;
        C2D_TextParse(&text, buf, str);
        C2D_TextOptimize(&text);
        float w = 0.0f, h = 0.0f;
        C2D_TextGetDimensions(&text, scale, scale, &w, &h);
        C2D_DrawText(&text, C2D_WithColor, cx - (w * 0.5f), cy - (h * 0.5f), z, scale, scale, color);
    }

    static void drawRightAlignedText(C2D_TextBuf buf, float rx, float cy, float scale, u32 color, const char* str, float z = 0.85f) {
        C2D_Text text;
        C2D_TextParse(&text, buf, str);
        C2D_TextOptimize(&text);
        float w = 0.0f, h = 0.0f;
        C2D_TextGetDimensions(&text, scale, scale, &w, &h);
        C2D_DrawText(&text, C2D_WithColor, rx - w, cy - (h * 0.5f), z, scale, scale, color);
    }

    // Calificación flotante estilo cómic auténtico Taiko (GOOD! en dorado/bermellón, OK en celeste, MISS en violeta)
    static void drawTaikoJudgementBadge(float cx, float cy, int judgement, float timer, C2D_TextBuf textBuf, float z = 0.45f) {
        if (timer <= 0.01f || judgement == 0) return;

        float progress = 1.0f - timer; // 0.0 -> 1.0
        float bounce = std::sin(progress * static_cast<float>(M_PI)) * 7.0f;
        float popY = cy - progress * 9.0f - bounce;

        // Escala de impacto inicial elástica
        float scale = (timer > 0.85f) ? (1.0f + (timer - 0.85f) * 1.6f) : 1.0f;
        u8 alpha = static_cast<u8>(std::min(1.0f, timer * 1.5f) * 255.0f);

        if (judgement == 300) {
            // "GOOD!" (良) - Placa vermellón brillante con borde dorado y chispas
            float pw = 54.0f * scale;
            float ph = 17.0f * scale;
            Ui::pill(cx - pw * 0.5f, popY - ph * 0.5f, pw, ph, z, Ui::rgba(235, 42, 45, alpha));
            drawEllipseRing(cx, popY, pw * 0.5f, ph * 0.5f, 1.8f * scale, Ui::rgba(255, 230, 60, alpha), z + 0.002f);

            float sparkDist = pw * 0.56f;
            float sparkR = 2.4f * scale;
            C2D_DrawCircleSolid(cx - sparkDist, popY, z + 0.003f, sparkR, Ui::rgba(255, 240, 100, alpha));
            C2D_DrawCircleSolid(cx + sparkDist, popY, z + 0.003f, sparkR, Ui::rgba(255, 240, 100, alpha));

            drawCenteredText(textBuf, cx + 0.8f, popY + 0.8f, 0.44f * scale, Ui::rgba(45, 12, 10, alpha), "GOOD!", z + 0.004f);
            drawCenteredText(textBuf, cx, popY, 0.44f * scale, Ui::rgba(255, 245, 130, alpha), "GOOD!", z + 0.006f);

        } else if (judgement == 100) {
            // "OK" (可) - Píldora celeste / cian brillante
            float pw = 42.0f * scale;
            float ph = 16.0f * scale;
            Ui::pill(cx - pw * 0.5f, popY - ph * 0.5f, pw, ph, z, Ui::rgba(30, 145, 220, alpha));
            drawEllipseRing(cx, popY, pw * 0.5f, ph * 0.5f, 1.6f * scale, Ui::rgba(210, 248, 255, alpha), z + 0.002f);

            drawCenteredText(textBuf, cx + 0.8f, popY + 0.8f, 0.42f * scale, Ui::rgba(10, 35, 65, alpha), "OK", z + 0.004f);
            drawCenteredText(textBuf, cx, popY, 0.42f * scale, CLR_WHITE, "OK", z + 0.006f);

        } else if (judgement == -1) {
            // "MISS" (不可) - Píldora violeta/magenta oscuro con temblor cómico
            float shakeX = std::sin(timer * 28.0f) * 2.2f * timer;
            float pw = 46.0f * scale;
            float ph = 15.0f * scale;
            Ui::pill(cx + shakeX - pw * 0.5f, popY - ph * 0.5f, pw, ph, z, Ui::rgba(95, 20, 68, alpha));
            drawEllipseRing(cx + shakeX, popY, pw * 0.5f, ph * 0.5f, 1.4f * scale, Ui::rgba(215, 80, 140, alpha), z + 0.002f);

            drawCenteredText(textBuf, cx + shakeX + 0.8f, popY + 0.8f, 0.38f * scale, Ui::rgba(25, 8, 20, alpha), "MISS", z + 0.004f);
            drawCenteredText(textBuf, cx + shakeX, popY, 0.38f * scale, Ui::rgba(255, 210, 230, alpha), "MISS", z + 0.006f);
        }
    }

    // Pétalo de cerezo (Sakura) flotante para Go-Go Time (Kiai)
    static void drawSakuraPetal(float cx, float cy, float angle, float z = 0.26f) {
        float rMajor = 4.8f;
        float rMinor = 2.6f;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        u32 pClr = Ui::rgba(255, 185, 210, 215);
        u32 pEdge = Ui::rgba(255, 230, 240, 230);

        float tipX = cx + cosA * rMajor;
        float tipY = cy + sinA * rMajor;
        float baseLX = cx - cosA * rMajor * 0.6f - sinA * rMinor;
        float baseLY = cy - sinA * rMajor * 0.6f + cosA * rMinor;
        float baseRX = cx - cosA * rMajor * 0.6f + sinA * rMinor;
        float baseRY = cy - sinA * rMajor * 0.6f - cosA * rMinor;

        C2D_DrawTriangle(tipX, tipY, pEdge, baseLX, baseLY, pClr, baseRX, baseRY, pClr, z);
        C2D_DrawCircleSolid(cx - cosA * 1.2f, cy - sinA * 1.2f, z + 0.001f, rMinor, pClr);
    }

    // Dibuja una baqueta tradicional de madera de hinoki (Bachi - 桴) golpeando el tambor en pantalla inferior
    static void drawBachi(float handleX, float handleY, float tipX, float tipY, float hitIntensity, bool isDonHit, float z = 0.225f) {
        float dx = tipX - handleX;
        float dy = tipY - handleY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0f) return;

        float ux = dx / len;
        float uy = dy / len;
        float nx = -uy;
        float ny = ux;

        // Efecto rebote elástico (spring recoil) tras el golpe
        float recoil = std::sin(hitIntensity * static_cast<float>(M_PI)) * 12.0f;
        float dispY = -recoil * 0.6f;
        float actualTipY = tipY + dispY;
        float actualHandleY = handleY + dispY * 0.3f;

        // 1. Sombra suave de la baqueta sobre el parche del tambor
        float shadowYOff = 12.0f - dispY * 0.5f;
        u32 shadowClr = Ui::rgba(20, 14, 28, static_cast<u8>(70 + 60 * hitIntensity));
        C2D_DrawLine(handleX, actualHandleY + shadowYOff, shadowClr, tipX, tipY + shadowYOff * 0.4f, shadowClr, 5.0f, z - 0.01f);

        // 2. Cuerpo cónico de madera de ciprés/hinoki
        float rBase = 3.0f;
        float rTip  = 4.4f;

        float p0x = handleX + nx * rBase;
        float p0y = actualHandleY + ny * rBase;
        float p1x = handleX - nx * rBase;
        float p1y = actualHandleY - ny * rBase;

        float p2x = tipX - nx * rTip;
        float p2y = actualTipY - ny * rTip;
        float p3x = tipX + nx * rTip;
        float p3y = actualTipY + ny * rTip;

        u32 woodLit   = Ui::rgba(252, 238, 204);
        u32 woodShade = Ui::rgba(205, 168, 122);

        C2D_DrawTriangle(p0x, p0y, woodLit, p1x, p1y, woodShade, p2x, p2y, woodShade, z);
        C2D_DrawTriangle(p0x, p0y, woodLit, p2x, p2y, woodShade, p3x, p3y, woodLit, z);

        // Franja de brillo central especular
        u32 hlClr = Ui::rgba(255, 255, 245, 170);
        C2D_DrawLine(handleX + nx * 0.5f, actualHandleY + ny * 0.5f, hlClr,
                     tipX + nx * 0.8f, actualTipY + ny * 0.8f, hlClr, 1.4f, z + 0.002f);

        // 3. Cabeza redondeada en la punta (Tip)
        C2D_DrawCircleSolid(tipX, actualTipY, z + 0.003f, rTip, woodLit);
        C2D_DrawCircleSolid(tipX - 1.0f, actualTipY - 1.0f, z + 0.004f, 1.4f, CLR_WHITE);

        // 4. Agarre tradicional encordado rojo en el mango
        float gripLen = len * 0.28f;
        float gx = handleX + ux * gripLen;
        float gy = actualHandleY + uy * gripLen;
        u32 gripClr = Ui::rgba(182, 42, 38);
        C2D_DrawLine(handleX, actualHandleY, gripClr, gx, gy, gripClr, 7.2f, z + 0.005f);
        C2D_DrawCircleSolid(handleX, actualHandleY, z + 0.006f, 3.8f, Ui::rgba(255, 215, 60));
        C2D_DrawCircleSolid(gx, gy, z + 0.006f, 3.8f, Ui::rgba(255, 215, 60));

        // 5. Destello de impacto / Estallido luminoso al conectar el golpe
        if (hitIntensity > 0.15f) {
            u8 fAlpha = static_cast<u8>(255 * hitIntensity);
            float fR = 9.0f * hitIntensity;
            C2D_DrawCircleSolid(tipX, actualTipY, z + 0.010f, fR, Ui::withAlpha(CLR_WHITE, fAlpha));
            u32 toneClr = isDonHit ? Ui::rgba(255, 120, 100, fAlpha) : Ui::rgba(140, 225, 255, fAlpha);
            C2D_DrawCircleSolid(tipX, actualTipY, z + 0.011f, fR * 0.65f, toneClr);

            float spLen = (12.0f + 6.0f * hitIntensity);
            for (int s = 0; s < 4; ++s) {
                float sAng = s * (static_cast<float>(M_PI) / 2.0f) + (1.0f - hitIntensity) * 0.5f;
                C2D_DrawLine(tipX, actualTipY, Ui::withAlpha(CLR_WHITE, fAlpha),
                             tipX + std::cos(sAng) * spLen, actualTipY + std::sin(sAng) * spLen,
                             Ui::withAlpha(toneClr, 0), 1.5f, z + 0.012f);
            }
        }
    }

public:

    void reset() {
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
    void initFromBeatmap(const Beatmap& beatmap) {
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
    void update(float bpmPulse) {
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
    int processHit(bool isDon, bool isBigAction, int songTime, float hit300, float hit100, float hit50, bool isAutoplay = false) {
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
    int checkMisses(int songTime, float hit50, bool isAutoplay = false) {
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

    void triggerBurst(u32 color) {
        targetBurstColor = color;
        targetBurstAlpha = 1.0f;
        targetBurstScale = 1.0f;
    }

    // --- RENDERIZADO PANTALLA SUPERIOR (1:1 con Taiko no Tatsujin Arcade / 3DS) ---
    void renderTopScreen(C2D_TextBuf textBuf, int songTime, float scrollRate, float eyeSign, float slider3D, bool inKiai, float bpmPulse,
                         int combo = 0, int score = 0, float hp = 1.0f, float acc = 100.0f,
                         const char* songTitle = nullptr, const char* diffName = nullptr,
                         int bgMode = 0) {
        auto depthShift = [&](float px) { return eyeSign * slider3D * px; };
        float dTarget = depthShift(2.5f);
        float tgX = TARGET_X + dTarget;
        float tgY = TRACK_Y;
        bool isFullSoul = (hp >= 0.70f);

        // =====================================================================
        // 1. ZONA SUPERIOR: PATRÓN KIKKO DORADO O PLACAS DE VIDEO/IMAGEN (Y: 0..60)
        // =====================================================================
        if (bgMode == 0) {
            // Fondo dorado festivo tradicional con degradado sutil
            Ui::gradV(0.0f, 0.0f, 400.0f, 62.0f, 0.02f, Ui::rgba(255, 238, 168), Ui::rgba(244, 210, 115));
            drawKikkoPattern(0.0f, 0.0f, 400.0f, 62.0f, 0.03f);
        }

        // Mascota Don-chan tocando su batería festiva en la esquina superior izquierda
        float mascotX = 40.0f + depthShift(3.5f);
        float mascotBounceVal = inKiai ? (mascotBounce * 1.6f + 0.15f * std::sin(songTime * 0.015f)) : mascotBounce;
        float mascotY = 30.0f - mascotBounceVal * 5.0f;

        if (bgMode != 0) {
            // Placa oscura translúcida para que Don-chan resalte sobre el video o imagen de fondo
            Ui::pill(mascotX - 24.0f, 8.0f, 48.0f, 46.0f, 0.05f, Ui::rgba(14, 10, 22, 185));
        }

        // Aura de fuego y energía durante Go-Go Time (Kiai)
        if (inKiai) {
            drawSunburst(mascotX, mascotY, 19.0f, 29.0f + 3.5f * bpmPulse, 16, Ui::rgba(255, 130, 20, 210), 0.072f, songTime * 0.007f);
            drawSunburst(mascotX, mascotY, 16.0f, 24.0f + 2.0f * bpmPulse, 12, Ui::rgba(255, 230, 45, 230), 0.075f, -songTime * 0.009f);

            // Badge / Cinta "GO-GO TIME!" estilo Taiko Arcade
            float gogoX = 114.0f;
            float gogoY = 19.0f;
            float gScale = 1.0f + 0.08f * bpmPulse;
            Ui::pill(gogoX - 38.0f * gScale, gogoY - 9.0f * gScale, 76.0f * gScale, 18.0f * gScale, 0.40f, Ui::rgba(235, 45, 30, 240));
            drawEllipseRing(gogoX, gogoY, 38.0f * gScale, 9.0f * gScale, 1.8f, Ui::rgba(255, 225, 60, 240), 0.402f);
            drawSunburst(gogoX, gogoY, 7.0f, 14.0f * gScale, 8, Ui::rgba(255, 220, 50, 180), 0.398f, songTime * 0.008f);
            drawCenteredText(textBuf, gogoX, gogoY, 0.32f * gScale, CLR_WHITE, "GO-GO TIME!", 0.405f);
        }

        // Batería Taiko tradicional de Don-chan (Platillo y tambor lateral)
        C2D_DrawLine(mascotX - 22.0f, 52.0f, Ui::rgba(40, 30, 25), mascotX - 22.0f, mascotY + 12.0f, Ui::rgba(40, 30, 25), 2.0f, 0.06f);
        drawEllipseFilled(mascotX - 22.0f, mascotY + 8.0f, 10.0f, 3.5f, Ui::rgba(255, 215, 60), 0.065f); // Platillo dorado

        // Cuerpo de Don-chan
        C2D_DrawCircleSolid(mascotX, mascotY, 0.08f, 18.0f, Ui::rgba(45, 20, 16));
        C2D_DrawCircleSolid(mascotX, mascotY, 0.082f, 17.0f, Ui::rgba(255, 55, 65));
        drawWadaikoByo(mascotX, mascotY, 17.0f, 17.0f, 10, Ui::rgba(255, 218, 70), 0.084f);
        int donEmotion = 0;
        if (missFaceTimer > 0.01f) {
            donEmotion = 2; // Cara de llanto cómica tras Miss
        } else if (inKiai || combo >= 50) {
            donEmotion = 1; // Ojos de estrellitas en Go-Go Time o racha alta
        }
        drawTaikoFace(mascotX, mascotY, 13.5f, false, true, 0.09f, donEmotion);

        // Notas musicales flotantes alrededor de Don-chan si el alma está llena
        if (isFullSoul) {
            float nBob = std::sin(songTime * 0.006f) * 4.0f;
            drawCenteredText(textBuf, mascotX + 22.0f, mascotY - 14.0f + nBob, 0.38f, Ui::rgba(255, 220, 60), "*", 0.10f);
            drawCenteredText(textBuf, mascotX - 22.0f, mascotY - 16.0f - nBob, 0.38f, Ui::rgba(255, 140, 180), "*", 0.10f);
        }

        // Hachimaki blanco y brazos alzados con baquetas (Bachi)
        C2D_DrawRectSolid(mascotX - 16.0f, mascotY - 10.0f, 0.092f, 32.0f, 2.6f, CLR_WHITE);
        // Brazo izquierdo con baqueta
        C2D_DrawLine(mascotX - 14.0f, mascotY - 2.0f, CLR_WHITE, mascotX - 24.0f, mascotY - 12.0f, CLR_WHITE, 3.2f, 0.095f);
        C2D_DrawLine(mascotX - 24.0f, mascotY - 12.0f, Ui::rgba(215, 175, 115), mascotX - 30.0f, mascotY - 18.0f, Ui::rgba(215, 175, 115), 2.2f, 0.096f);
        // Brazo derecho con baqueta
        C2D_DrawLine(mascotX + 14.0f, mascotY - 2.0f, CLR_WHITE, mascotX + 24.0f, mascotY - 12.0f, CLR_WHITE, 3.2f, 0.095f);
        C2D_DrawLine(mascotX + 24.0f, mascotY - 12.0f, Ui::rgba(215, 175, 115), mascotX + 30.0f, mascotY - 18.0f, Ui::rgba(215, 175, 115), 2.2f, 0.096f);

        // Abanico desplegable de Redoble ("Drum Roll") cuando está activo
        if (drumrollFanTimer > 0.01f) {
            float fanX = tgX - 5.0f;
            float fanY = 22.0f;
            float fScale = std::min(1.0f, drumrollFanTimer * 1.5f);

            // Abanico japonés tradicional (Sensu)
            const int RIBS = 7;
            float rInner = 8.0f * fScale;
            float rOuter = 26.0f * fScale;
            for (int r = 0; r < RIBS; ++r) {
                float a0 = -1.2f + (r * 2.4f) / RIBS;
                float a1 = -1.2f + ((r + 1) * 2.4f) / RIBS;
                u32 ribClr = (r % 2 == 0) ? Ui::rgba(255, 110, 30) : Ui::rgba(255, 240, 220);
                C2D_DrawTriangle(
                    fanX + std::sin(a0) * rInner, fanY - std::cos(a0) * rInner, ribClr,
                    fanX + std::sin(a1) * rInner, fanY - std::cos(a1) * rInner, ribClr,
                    fanX + std::sin((a0 + a1) * 0.5f) * rOuter, fanY - std::cos((a0 + a1) * 0.5f) * rOuter, ribClr,
                    0.40f
                );
            }
            // Borde dorado
            drawRing(fanX, fanY, rOuter, 1.5f, Ui::rgba(255, 215, 60), 20, 0.41f);

            // Número gigante de redobles
            char rollBuf[16];
            std::snprintf(rollBuf, sizeof(rollBuf), "%d", drumrollActiveCounter);
            drawCenteredText(textBuf, fanX, fanY - 15.0f * fScale, 0.52f * fScale, Ui::rgba(20, 16, 26), rollBuf, 0.43f);
            drawCenteredText(textBuf, fanX, fanY - 16.0f * fScale, 0.52f * fScale, CLR_WHITE, rollBuf, 0.44f);
            drawCenteredText(textBuf, fanX, fanY - 3.0f * fScale, 0.22f * fScale, Ui::rgba(255, 220, 80), "Drum Roll", 0.44f);

            // Mini Taiko sonriente al lado del abanico
            C2D_DrawCircleSolid(fanX + 28.0f * fScale, fanY - 8.0f, 0.42f, 7.5f, Ui::rgba(255, 205, 45));
            C2D_DrawCircleSolid(fanX + 28.0f * fScale, fanY - 8.0f, 0.43f, 6.0f, CLR_WHITE);
            drawTaikoFace(fanX + 28.0f * fScale, fanY - 8.0f, 6.0f, false, true, 0.44f);
        }

        // Título de la Canción en la esquina superior derecha
        const char* titleTxt = (songTitle && songTitle[0]) ? songTitle : "MEGALOVANIA";
        if (bgMode != 0) {
            Ui::pill(275.0f, 2.0f, 122.0f, 15.0f, 0.12f, Ui::rgba(14, 10, 22, 175));
        }
        drawRightAlignedText(textBuf, 392.0f, 9.0f, 0.40f, (bgMode == 0) ? Ui::rgba(30, 20, 35) : CLR_WHITE, titleTxt, 0.15f);

        // Badge de Género / Dificultad (Píldora índigo / púrpura)
        const char* diffTxt = (diffName && diffName[0]) ? diffName : "Hard";
        Ui::pill(328.0f, 18.0f, 64.0f, 13.0f, 0.14f, Ui::rgba(145, 90, 195));
        drawCenteredText(textBuf, 360.0f, 24.5f, 0.24f, CLR_WHITE, diffTxt, 0.16f);

        // =====================================================================
        // 2. BARRA DE ALMA / TAMASHII (CLEAR GAUGE OFICIAL ESTILO TAIKO 3DS)
        // =====================================================================
        float gaugeX = 168.0f;
        float gaugeY = 36.0f;
        float gaugeW = 208.0f;
        float gaugeH = 15.0f;

        // Marco ranurado exterior con bisel de laca dorada
        Ui::panel(gaugeX - 2.0f, gaugeY - 1.5f, gaugeW + 4.0f, gaugeH + 3.0f, 0.18f, Ui::rgba(20, 16, 26, 255), Ui::rgba(255, 215, 60, 210));

        // 26 Bloques ranurados
        const int TOTAL_BLOCKS = 26;
        const int CLEAR_BLOCK = 18; // ~70% threshold
        float blockW = (gaugeW - (TOTAL_BLOCKS - 1) * 2.0f) / TOTAL_BLOCKS;
        float clampedHp = std::max(0.0f, std::min(1.0f, hp));
        int activeBlocks = static_cast<int>(clampedHp * TOTAL_BLOCKS);

        for (int b = 0; b < TOTAL_BLOCKS; ++b) {
            float bx = gaugeX + b * (blockW + 2.0f);
            bool isActive = (b < activeBlocks);

            if (isActive) {
                u32 bClr;
                if (b < CLEAR_BLOCK) {
                    float t = static_cast<float>(b) / CLEAR_BLOCK;
                    bClr = Ui::lerpColor(Ui::rgba(30, 210, 160), Ui::rgba(255, 230, 45), t);
                } else {
                    float t = static_cast<float>(b - CLEAR_BLOCK) / (TOTAL_BLOCKS - CLEAR_BLOCK);
                    bClr = Ui::lerpColor(Ui::rgba(255, 220, 45), Ui::rgba(255, 55, 45), t);
                }
                C2D_DrawRectSolid(bx, gaugeY, 0.20f, blockW, gaugeH, bClr);
                C2D_DrawRectSolid(bx, gaugeY, 0.205f, blockW, 2.5f, Ui::rgba(255, 255, 255, 160));
            } else {
                C2D_DrawRectSolid(bx, gaugeY, 0.19f, blockW, gaugeH, Ui::rgba(34, 28, 42, 230));
            }
        }

        // Banderín indicador "CLEAR" (クリア)
        float clearMarkerX = gaugeX + CLEAR_BLOCK * (blockW + 2.0f);
        C2D_DrawLine(clearMarkerX, gaugeY - 2.5f, Ui::rgba(255, 220, 60), clearMarkerX, gaugeY + gaugeH + 2.5f, Ui::rgba(255, 220, 60), 1.8f, 0.21f);
        Ui::pill(clearMarkerX - 14.0f, gaugeY - 10.0f, 28.0f, 9.0f, 0.22f, Ui::rgba(255, 200, 45, 255));
        drawCenteredText(textBuf, clearMarkerX, gaugeY - 5.5f, 0.20f, Ui::rgba(25, 16, 10), "CLEAR", 0.23f);

        // Icono de Alma (Tamashii / 魂) al final derecho
        float flameX = gaugeX + gaugeW + 10.0f;
        float flameY = gaugeY + 7.5f;
        u32 fClr = isFullSoul ? Ui::rgba(255, 130, 25) : Ui::rgba(75, 60, 90);
        C2D_DrawCircleSolid(flameX, flameY, 0.22f, 9.0f, fClr);
        C2D_DrawCircleSolid(flameX, flameY, 0.225f, 7.5f, isFullSoul ? Ui::rgba(255, 65, 40) : Ui::rgba(55, 45, 70));
        if (isFullSoul) {
            drawSunburst(flameX, flameY, 7.0f, 13.0f + 2.5f * bpmPulse, 10, Ui::rgba(255, 220, 60), 0.215f, songTime * 0.005f);
        }
        drawCenteredText(textBuf, flameX, flameY, 0.28f, CLR_WHITE, "魂", 0.24f);

        // =====================================================================
        // 3. PISTA HORIZONTAL DE TAIKO (Y: 60..118, Centro Y: 89.0f)
        // =====================================================================
        float trackH = 58.0f;
        float trackTopY = tgY - (trackH * 0.5f); // 89 - 29 = 60.0f

        // Fondo de tatami carbón/borgoña profundo con sutil degradado
        Ui::gradV(0.0f, trackTopY, 400.0f, trackH, 0.15f, Ui::rgba(42, 12, 18), Ui::rgba(18, 5, 8));

        // Riel superior lacado bermellón con ribete dorado
        C2D_DrawRectSolid(0.0f, trackTopY, 0.16f, 400.0f, 4.0f, Ui::rgba(195, 36, 36, 255));
        C2D_DrawRectSolid(0.0f, trackTopY + 3.0f, 0.17f, 400.0f, 1.4f, Ui::rgba(255, 218, 70, 240));

        // Riel inferior lacado bermellón con ribete dorado
        C2D_DrawRectSolid(0.0f, trackTopY + trackH - 4.0f, 0.16f, 400.0f, 4.0f, Ui::rgba(195, 36, 36, 255));
        C2D_DrawRectSolid(0.0f, trackTopY + trackH - 4.0f, 0.17f, 400.0f, 1.4f, Ui::rgba(255, 218, 70, 240));

        // Resplandor cálido durante Kiai Time y lluvia de pétalos de flor de cerezo (Sakura Fubuki)
        if (inKiai) {
            u8 kAlpha = static_cast<u8>(40.0f + 60.0f * bpmPulse);
            C2D_DrawRectSolid(0.0f, trackTopY + 4.0f, 0.175f, 400.0f, trackH - 8.0f, Ui::rgba(255, 195, 50, kAlpha));

            // Lluvia de pétalos de flor de cerezo (Sakura Fubuki) flotando suavemente de derecha a izquierda
            for (int p = 0; p < 12; ++p) {
                float px = std::fmod(songTime * 0.12f + p * 38.0f, 440.0f);
                float flowX = 420.0f - px;
                float waveY = std::sin(songTime * 0.0035f + p * 1.3f) * 11.0f;
                float flowY = tgY + waveY + ((p % 5) - 2) * 6.5f;
                float angle = songTime * 0.003f + p * 0.8f;
                drawSakuraPetal(flowX, flowY, angle, 0.27f);
            }
        }

        // =====================================================================
        // 4. CINTA DE SUBTÍTULOS DE GOLPE (Y: 118..130)
        // =====================================================================
        float subY = trackTopY + trackH;
        float subH = 13.0f;
        C2D_DrawRectSolid(0.0f, subY, 0.16f, 400.0f, subH, Ui::rgba(65, 22, 28, 250));
        C2D_DrawRectSolid(0.0f, subY + subH - 1.0f, 0.17f, 400.0f, 1.0f, Ui::rgba(255, 218, 70, 120));

        // =====================================================================
        // 5. NOTAS Y REDOBLES EN MOVIMIENTO (Z: 0.20f..0.25f - DESLIZAN BAJO EL RECEPTOR)
        // =====================================================================
        float pxPerMs = 0.36f * scrollRate;
        for (const auto& n : notes) {
            if (n.state != NOT_HIT && n.type != TAIKO_DRUMROLL) continue;

            float dt = static_cast<float>(n.time - songTime);
            float baseNx = TARGET_X + dt * pxPerMs;
            float distFactor = std::max(0.0f, std::min(1.0f, (baseNx - TARGET_X) / 320.0f));
            float noteParallax = 2.6f - distFactor * 5.2f; // De -2.6f (fondo lejano) a +2.6f (receptor en primer plano)
            float nx = baseNx + depthShift(noteParallax);

            float endDt = static_cast<float>(n.endTime - songTime);
            float baseEndX = TARGET_X + endDt * pxPerMs;
            float endDistFactor = std::max(0.0f, std::min(1.0f, (baseEndX - TARGET_X) / 320.0f));
            float endParallax = 2.6f - endDistFactor * 5.2f;
            float endX = (n.type == TAIKO_DRUMROLL || n.type == TAIKO_DENDEN) ? (baseEndX + depthShift(endParallax)) : nx;

            if (endX < -80.0f) continue;
            if (nx > 420.0f) continue;

            // Subtítulos debajo de la nota
            if (nx >= tgX - 20.0f && nx <= 400.0f) {
                const char* subTxt = "DON";
                u32 subClr = Ui::rgba(255, 120, 120);
                if (n.type == TAIKO_KATSU) {
                    subTxt = "KAT";
                    subClr = Ui::rgba(140, 210, 255);
                } else if (n.type == TAIKO_BIG_DON) {
                    subTxt = "DON (Big)";
                    subClr = Ui::rgba(255, 220, 75);
                } else if (n.type == TAIKO_BIG_KATSU) {
                    subTxt = "KAT (Big)";
                    subClr = Ui::rgba(255, 220, 75);
                } else if (n.type == TAIKO_DRUMROLL) {
                    subTxt = "ROLL";
                    subClr = Ui::rgba(255, 230, 80);
                } else if (n.type == TAIKO_DENDEN) {
                    subTxt = "DEN";
                    subClr = Ui::rgba(255, 210, 60);
                }
                drawCenteredText(textBuf, nx, subY + 6.5f, 0.23f, subClr, subTxt, 0.18f);
            }

            if (n.type == TAIKO_DRUMROLL) {
                // =============================================================
                // REDOBLE CÁPSULA CONTINUA SUAVE (Pill capsule geometry)
                // =============================================================
                float rollH = 26.0f;
                float halfH = rollH * 0.5f;

                // 1. Cuerpo exterior ámbar redondeado
                C2D_DrawLine(nx, tgY, Ui::rgba(225, 135, 15), endX, tgY, Ui::rgba(225, 135, 15), rollH + 3.0f, 0.22f);
                C2D_DrawCircleSolid(nx, tgY, 0.222f, halfH + 1.5f, Ui::rgba(225, 135, 15));
                C2D_DrawCircleSolid(endX, tgY, 0.222f, halfH + 1.5f, Ui::rgba(225, 135, 15));

                // 2. Núcleo amarillo brillante oro
                C2D_DrawLine(nx, tgY, Ui::rgba(255, 225, 55), endX, tgY, Ui::rgba(255, 225, 55), rollH - 2.5f, 0.224f);
                C2D_DrawCircleSolid(nx, tgY, 0.225f, halfH - 1.25f, Ui::rgba(255, 225, 55));
                C2D_DrawCircleSolid(endX, tgY, 0.225f, halfH - 1.25f, Ui::rgba(255, 225, 55));

                // 3. Franja superior de brillo especular
                C2D_DrawLine(nx, tgY - halfH * 0.40f, Ui::rgba(255, 255, 230, 190), endX, tgY - halfH * 0.40f, Ui::rgba(255, 255, 230, 190), 2.5f, 0.227f);

                // 4. Ticks de ritmo internos
                float span = endX - nx;
                if (span > 32.0f) {
                    int tickCount = std::min(12, static_cast<int>(span / 24.0f));
                    for (int t = 1; t <= tickCount; ++t) {
                        float tx = nx + t * (span / (tickCount + 1));
                        C2D_DrawCircleSolid(tx, tgY, 0.228f, 2.5f, Ui::rgba(255, 255, 255, 230));
                        C2D_DrawCircleSolid(tx, tgY, 0.229f, 1.2f, Ui::rgba(215, 130, 10));
                    }
                }

                // 5. Cabeza del redoble con carita Taiko
                float capR = NOTE_RADIUS;
                C2D_DrawCircleSolid(nx, tgY, 0.231f, capR + 2.0f, Ui::rgba(20, 16, 26));
                C2D_DrawCircleSolid(nx, tgY, 0.233f, capR + 1.0f, CLR_WHITE);
                C2D_DrawCircleSolid(nx, tgY, 0.235f, capR, Ui::rgba(225, 135, 15));
                C2D_DrawCircleSolid(nx, tgY, 0.237f, capR * 0.88f, Ui::rgba(255, 225, 55));
                drawTaikoFace(nx, tgY, capR * 0.88f, false, true, 0.239f);

            } else if (n.type == TAIKO_DENDEN) {
                float durMs = static_cast<float>(n.endTime - n.time);
                float endDendenX = nx + durMs * pxPerMs;

                C2D_DrawLine(nx, tgY, Ui::rgba(240, 180, 60), endDendenX, tgY, Ui::rgba(240, 180, 60), 2.2f, 0.22f);

                // Progreso de inflado del globo tradicional (Kamifūsen)
                float progress = (n.dendenRequiredHits > 0) ? (static_cast<float>(n.dendenCurrentHits) / n.dendenRequiredHits) : 0.0f;
                progress = std::min(1.0f, progress);
                float bR = (NOTE_RADIUS + 3.0f) * (1.0f + 0.35f * progress);

                // Contorno negro exterior
                C2D_DrawCircleSolid(nx, tgY, 0.23f, bR + 2.2f, Ui::rgba(20, 16, 26));

                // Fondo blanco base del globo de papel japonés
                C2D_DrawCircleSolid(nx, tgY, 0.232f, bR, CLR_WHITE);

                // Gajos verticales curvos tradicionales: Rojo, Amarillo, Celeste, Verde
                drawEllipseFilled(nx - bR * 0.50f, tgY, bR * 0.50f, bR * 0.96f, Ui::rgba(235, 50, 55), 0.234f);
                drawEllipseFilled(nx - bR * 0.16f, tgY, bR * 0.34f, bR * 0.98f, Ui::rgba(255, 215, 45), 0.235f);
                drawEllipseFilled(nx + bR * 0.16f, tgY, bR * 0.34f, bR * 0.98f, Ui::rgba(45, 155, 235), 0.235f);
                drawEllipseFilled(nx + bR * 0.50f, tgY, bR * 0.50f, bR * 0.96f, Ui::rgba(45, 185, 105), 0.234f);

                // Cuerda inferior atada con nudo festivo
                C2D_DrawLine(nx, tgY + bR, Ui::rgba(180, 40, 40), nx, tgY + bR + 7.0f, Ui::rgba(180, 40, 40), 1.8f, 0.236f);
                C2D_DrawCircleSolid(nx, tgY + bR + 1.0f, 0.237f, 2.0f, Ui::rgba(220, 50, 50));

                // Placa circular blanca en el centro para el número de golpes restantes
                float badgeR = bR * 0.50f;
                C2D_DrawCircleSolid(nx, tgY, 0.238f, badgeR + 1.4f, Ui::rgba(20, 16, 26));
                C2D_DrawCircleSolid(nx, tgY, 0.239f, badgeR, Ui::rgba(255, 252, 245));

                int remaining = std::max(0, n.dendenRequiredHits - n.dendenCurrentHits);
                char dBuf[16];
                std::snprintf(dBuf, sizeof(dBuf), "%d", remaining);
                drawCenteredText(textBuf, nx, tgY, 0.38f + 0.08f * progress, Ui::rgba(25, 12, 10), dBuf, 0.242f);
            } else {
                bool isBig = (n.type == TAIKO_BIG_DON || n.type == TAIKO_BIG_KATSU);
                bool isDon = (n.type == TAIKO_DON || n.type == TAIKO_BIG_DON);
                float r = isBig ? BIG_NOTE_RADIUS : NOTE_RADIUS;

                u32 innerClr = isDon ? CLR_DON_INNER : CLR_KATSU_INNER;
                u32 outerClr = isDon ? CLR_DON_OUTER : CLR_KATSU_OUTER;

                // Anillo de aura dorada pulsante para notas grandes
                if (isBig) {
                    drawRing(nx, tgY, r + 4.5f, 2.4f, Ui::rgba(255, 218, 70, static_cast<u8>(180 + 75 * bpmPulse)), 32, 0.225f);
                    drawSunburst(nx, tgY, r + 2.5f, r + 7.5f, 10, Ui::rgba(255, 215, 60, static_cast<u8>(95 + 75 * bpmPulse)), 0.222f, songTime * 0.005f);
                }

                // 1. Contorno negro carbón exterior
                C2D_DrawCircleSolid(nx, tgY, 0.23f, r + 2.6f, Ui::rgba(20, 16, 26));

                // 2. Aro blanco brillante
                C2D_DrawCircleSolid(nx, tgY, 0.233f, r + 1.2f, CLR_WHITE);

                // 3. Borde exterior oscuro
                C2D_DrawCircleSolid(nx, tgY, 0.236f, r, outerClr);

                // 4. Parche interior de color Don / Kat vibrante
                C2D_DrawCircleSolid(nx, tgY, 0.239f, r * 0.90f, innerClr);

                // 5. Destello especular 3D superior
                C2D_DrawCircleSolid(nx - r * 0.30f, tgY - r * 0.30f, 0.241f, r * 0.24f, Ui::rgba(255, 255, 255, 130));

                // 6. Carita auténtica anime de Taiko
                drawTaikoFace(nx, tgY, r, isBig, isDon, 0.243f);
            }
        }

        // =====================================================================
        // 6. RECEPTOR DE IMPACTO EN LLAMAS (Z: 0.28f..0.32f - SOBRE LAS NOTAS)
        // =====================================================================
        // Aro solar flamígero con rayos dorados giratorios (se intensifica en Go-Go Time)
        float burstRInner = NOTE_RADIUS + 3.0f;
        float burstROuter = NOTE_RADIUS + (inKiai ? 13.5f : 8.5f) + (inKiai ? 3.5f : 2.5f) * bpmPulse;
        u32 burstColor = inKiai ? Ui::rgba(255, 75, 25, static_cast<u8>(195 + 60 * bpmPulse)) : Ui::rgba(255, 175, 40, static_cast<u8>(160 + 80 * bpmPulse));
        drawSunburst(tgX, tgY, burstRInner, burstROuter, inKiai ? 16 : 12, burstColor, 0.28f, songTime * (inKiai ? 0.008f : 0.004f));
        if (inKiai) {
            drawSunburst(tgX, tgY, burstRInner, burstROuter * 0.72f, 12, Ui::rgba(255, 230, 50, 225), 0.282f, -songTime * 0.010f);
        }

        // Aro exterior de madera con remaches dorados
        C2D_DrawCircleSolid(tgX, tgY, 0.285f, NOTE_RADIUS + 5.0f, Ui::rgba(70, 32, 18, 245));
        drawRing(tgX, tgY, NOTE_RADIUS + 4.0f, 1.6f, Ui::rgba(225, 180, 60, 240), 28, 0.29f);

        // Parche de cuero blanco del receptor
        C2D_DrawCircleSolid(tgX, tgY, 0.295f, NOTE_RADIUS + 1.5f, Ui::rgba(240, 236, 226, 245));
        C2D_DrawCircleSolid(tgX, tgY, 0.298f, NOTE_RADIUS, Ui::rgba(255, 254, 250, 255));

        // Anillo de pulso rítmico dorado
        float pulseR = NOTE_RADIUS * (0.80f + 0.20f * bpmPulse);
        drawRing(tgX, tgY, pulseR, 1.6f, Ui::rgba(255, 130, 60, static_cast<u8>(140 + 90 * bpmPulse)), 24, 0.30f);
        drawRing(tgX, tgY, BIG_NOTE_RADIUS + 1.0f, 1.2f, Ui::rgba(255, 215, 75, static_cast<u8>(75 + 75 * bpmPulse)), 32, 0.299f);

        // Calificación flotante sobre el receptor ("GOOD!" / "OK" / "MISS" estilo Taiko)
        if (hitJudgementTimer > 0.01f) {
            drawTaikoJudgementBadge(tgX, tgY - NOTE_RADIUS - 12.0f, lastHitJudgement, hitJudgementTimer, textBuf, 0.45f);
        }

        // Explosión festiva de confeti tras reventar el globo tradicional (Kamifūsen / Kusudama)
        if (balloonPopTimer > 0.01f) {
            float bProg = 1.0f - balloonPopTimer; // 0.0 -> 1.0
            u8 bAlpha = static_cast<u8>(balloonPopTimer * 255.0f);
            const u32 confettiClrs[5] = {
                Ui::rgba(255, 60, 70), Ui::rgba(255, 220, 50), Ui::rgba(45, 180, 245),
                Ui::rgba(50, 215, 120), Ui::rgba(255, 120, 200)
            };
            for (int c = 0; c < 28; ++c) {
                float cAngle = c * (static_cast<float>(M_PI) * 2.0f / 28.0f) + c * 0.4f;
                float cDist = (15.0f + c * 3.5f) * std::sqrt(bProg);
                float cX = tgX + std::cos(cAngle) * cDist;
                float cY = tgY + std::sin(cAngle) * (cDist * 0.7f) + (bProg * bProg * 35.0f);
                u32 cClr = Ui::withAlpha(confettiClrs[c % 5], bAlpha);
                C2D_DrawRectSolid(cX, cY, 0.45f, 4.0f, 3.0f, cClr);
            }
            // Badge central "POPPED!"
            float popScale = 1.0f + (1.0f - balloonPopTimer) * 0.4f;
            Ui::pill(tgX - 36.0f * popScale, tgY - 26.0f, 72.0f * popScale, 16.0f * popScale, 0.46f, Ui::rgba(255, 215, 45, bAlpha));
            drawCenteredText(textBuf, tgX, tgY - 18.0f, 0.35f * popScale, Ui::rgba(25, 14, 10, bAlpha), "POPPED!", 0.47f);
        }

        // Efecto expansivo de impacto al acertar nota
        if (targetBurstAlpha > 0.01f) {
            u8 aVal = static_cast<u8>(255.0f * targetBurstAlpha);
            float bRadius = (NOTE_RADIUS + 10.0f) * targetBurstScale;
            drawRing(tgX, tgY, bRadius, 3.2f, Ui::withAlpha(targetBurstColor, aVal), 32, 0.32f);
            drawRing(tgX, tgY, bRadius * 0.65f, 2.0f, Ui::withAlpha(CLR_WHITE, aVal), 24, 0.322f);
            drawSunburst(tgX, tgY, bRadius * 0.6f, bRadius * 1.35f, 14, Ui::withAlpha(targetBurstColor, static_cast<u8>(aVal * 0.75f)), 0.31f, targetBurstScale * 3.0f);
        }

        // =====================================================================
        // 7. PANEL IZQUIERDO: COMBO DRUM Y PUNTAJE (Z: 0.35f - TAPA NOTAS A LA IZQ)
        // =====================================================================
        float hudLeftW = tgX - 18.0f; // Bloque que cubre desde X: 0 hasta el receptor
        C2D_DrawRectSolid(0.0f, trackTopY, 0.34f, hudLeftW, trackH, Ui::rgba(38, 10, 16, 255));
        Ui::gradH(hudLeftW - 14.0f, trackTopY, 14.0f, trackH, 0.345f, Ui::rgba(38, 10, 16, 255), Ui::rgba(38, 10, 16, 0));

        // Tag del Jugador "DON-chan"
        drawCenteredText(textBuf, 38.0f, trackTopY + 9.0f, 0.28f, CLR_WHITE, "DON-chan", 0.36f);

        // Icono de Pino / Linterna y Dificultad
        C2D_DrawCircleSolid(14.0f, trackTopY + 22.0f, 0.36f, 5.0f, Ui::rgba(35, 110, 50));
        C2D_DrawCircleSolid(14.0f, trackTopY + 20.0f, 0.362f, 4.0f, Ui::rgba(45, 140, 65));
        drawCenteredText(textBuf, 40.0f, trackTopY + 21.0f, 0.26f, Ui::rgba(255, 220, 80), diffTxt, 0.36f);

        // Marcador de Puntaje grande en números nítidos
        char scBuf[32];
        std::snprintf(scBuf, sizeof(scBuf), "%d", score);
        drawCenteredText(textBuf, 42.0f, trackTopY + 36.0f, 0.44f, CLR_WHITE, scBuf, 0.37f);
        // Barra cian de progreso bajo el puntaje
        C2D_DrawRectSolid(10.0f, trackTopY + 44.0f, 0.365f, 64.0f, 2.0f, Ui::rgba(30, 190, 220, 220));

        // Tambor de Combo (Wadaiko Combo Drum oficial)
        float comboDrumX = hudLeftW - 14.0f;
        float comboDrumY = tgY;
        float cScale = 1.0f + 0.08f * bpmPulse;

        C2D_DrawCircleSolid(comboDrumX, comboDrumY, 0.36f, 18.0f * cScale, Ui::rgba(30, 20, 25));
        C2D_DrawCircleSolid(comboDrumX, comboDrumY, 0.362f, 17.0f * cScale, Ui::rgba(255, 218, 70));
        C2D_DrawCircleSolid(comboDrumX, comboDrumY, 0.365f, 15.0f * cScale, Ui::rgba(255, 252, 245));

        char cbBuf[16];
        std::snprintf(cbBuf, sizeof(cbBuf), "%d", combo);
        drawCenteredText(textBuf, comboDrumX, comboDrumY - 3.5f * cScale, 0.46f * cScale, Ui::rgba(20, 16, 26), cbBuf, 0.375f);
        drawCenteredText(textBuf, comboDrumX, comboDrumY + 8.0f * cScale, 0.20f * cScale, Ui::rgba(225, 80, 45), "Combo", 0.375f);

        // =====================================================================
        // 8. ESCENARIO INFERIOR: FIESTA TRADICIONAL JAPONESA Y BAILARINES (Y: 130..240)
        // =====================================================================
        if (bgMode == 0) {
            // Fondo festivo dorado cálido con nubes tradicionales
            Ui::gradV(0.0f, 130.0f, 400.0f, 110.0f, 0.05f, Ui::rgba(250, 220, 120), Ui::rgba(235, 180, 70));

            // Nubes japonesas en capas
            drawJapaneseCloud(60.0f, 155.0f, 1.1f, Ui::rgba(255, 242, 195, 220), Ui::rgba(240, 195, 85, 240), 0.06f);
            drawJapaneseCloud(210.0f, 148.0f, 1.3f, Ui::rgba(255, 245, 205, 220), Ui::rgba(240, 195, 85, 240), 0.06f);
            drawJapaneseCloud(340.0f, 156.0f, 1.0f, Ui::rgba(255, 242, 195, 220), Ui::rgba(240, 195, 85, 240), 0.06f);

            // Peonzas tradicionales japonesas (Koma) decorativas flotando
            drawKomaTop(85.0f, 140.0f, 1.0f, songTime * 0.005f, 0.08f);
            drawKomaTop(320.0f, 142.0f, 0.9f, -songTime * 0.005f, 0.08f);

            // 5 Bailarines tradicionales festivos (Hyottoko / Okame) bailando al ritmo
            const float dancerX[5] = { 55.0f, 125.0f, 200.0f, 275.0f, 345.0f };
            for (int d = 0; d < 5; ++d) {
                float individualBounce = std::sin(songTime * 0.006f + d * 1.1f) * 0.5f + 0.5f;
                float jumpScale = inKiai ? 1.65f : 1.0f;
                drawFestivalDancer(dancerX[d], 195.0f, individualBounce * mascotBounce * jumpScale, d, 0.12f);
                if (inKiai) {
                    // Chispitas doradas festivas ascendentes sobre cada bailarín
                    float sparkY = 175.0f - std::fmod(songTime * 0.045f + d * 22.0f, 36.0f);
                    C2D_DrawCircleSolid(dancerX[d] + ((d % 2 == 0) ? 6.0f : -6.0f), sparkY, 0.125f, 1.5f, Ui::rgba(255, 225, 75, 210));
                }
            }
        }
    }

    // --- RENDERIZADO PANTALLA INFERIOR (Tambor Wadaiko 3DS en perspectiva e interactivo 1:1) ---
    void renderBottomDrum(C2D_TextBuf textBuf, float animTick, const GameSettings* pSettings = nullptr, float guideAlpha = 0.0f) {
        float screenW = 320.0f;
        float screenH = 240.0f;

        // 1. Fondo tradicional japonés: Tatami menta/turquesa y patrón asanoha
        C2D_DrawRectSolid(0.0f, 0.0f, 0.02f, screenW, screenH, Ui::rgba(110, 204, 188, 255));

        const float spacing = 32.0f;
        u32 gridClr = Ui::rgba(150, 230, 218, 95);
        u32 dotClr  = Ui::rgba(170, 245, 235, 140);

        for (float x = -screenH; x < screenW + screenH; x += spacing) {
            C2D_DrawLine(x, 0.0f, gridClr, x + screenH, screenH, gridClr, 1.0f, 0.025f);
            C2D_DrawLine(x, screenH, gridClr, x + screenH, 0.0f, gridClr, 1.0f, 0.025f);
        }
        for (float y = 0.0f; y <= screenH; y += spacing) {
            for (float x = 0.0f; x <= screenW; x += spacing) {
                C2D_DrawCircleSolid(x, y, 0.028f, 1.6f, dotClr);
            }
        }

        // 2. Tambor Wadaiko 3D en perspectiva
        float cx = 160.0f;
        float cy = 134.0f;
        float outerRx = 136.0f;
        float outerRy = 86.0f;
        float katsuRx = 124.0f;
        float katsuRy = 76.0f;
        float donRx   = 96.0f;
        float donRy   = 58.0f;
        float innerCoralRx = 54.0f;
        float innerCoralRy = 33.0f;
        float barrelBottom = 238.0f;

        // Sombra suave en el tatami bajo el tambor
        drawEllipseFilled(cx, barrelBottom - 2.0f, outerRx + 8.0f, 18.0f, Ui::rgba(25, 60, 52, 160), 0.07f);

        // Soporte de madera tradicional japonés (Dai - 台)
        C2D_DrawRectSolid(cx - 128.0f, barrelBottom - 22.0f, 0.075f, 32.0f, 22.0f, Ui::rgba(24, 14, 10));
        C2D_DrawRectSolid(cx + 96.0f,  barrelBottom - 22.0f, 0.075f, 32.0f, 22.0f, Ui::rgba(24, 14, 10));
        C2D_DrawRectSolid(cx - 124.0f, barrelBottom - 18.0f, 0.078f, 24.0f, 3.5f, Ui::rgba(215, 175, 55));
        C2D_DrawRectSolid(cx + 100.0f, barrelBottom - 18.0f, 0.078f, 24.0f, 3.5f, Ui::rgba(215, 175, 55));

        // Pared cilíndrica de madera de cerezo lacado tradicional (Do - 胴)
        Ui::gradV(cx - outerRx + 4.0f, cy, (outerRx - 4.0f) * 2.0f, barrelBottom - cy, 0.08f, Ui::rgba(92, 34, 24), Ui::rgba(32, 12, 8));

        // Líneas de listones de madera
        for (float sx = -80.0f; sx <= 80.0f; sx += 40.0f) {
            float staveTopX = cx + sx;
            float staveBotX = cx + sx * 1.15f;
            C2D_DrawLine(staveTopX, cy + 20.0f, Ui::rgba(255, 180, 140, 25), staveBotX, barrelBottom - 6.0f, Ui::rgba(255, 180, 140, 15), 1.6f, 0.085f);
        }

        drawEllipseFilled(cx, barrelBottom - 4.0f, outerRx - 6.0f, 24.0f, Ui::rgba(28, 12, 8), 0.095f);

        // 3. Anillo exterior Katsu y remaches dorados (Byo - 鋲)
        drawEllipseFilled(cx, cy, outerRx, outerRy, Ui::rgba(28, 24, 32), 0.11f);

        u32 rivetBaseClr = Ui::rgba(255, 215, 65);
        if (drumHitKatsuLeft > 0.05f || drumHitKatsuRight > 0.05f) {
            float maxKat = std::max(drumHitKatsuLeft, drumHitKatsuRight);
            rivetBaseClr = Ui::lerpColor(rivetBaseClr, Ui::rgba(180, 245, 255), maxKat);
        }
        drawWadaikoByo(cx, cy, outerRx - 5.0f, outerRy - 5.0f, 28, rivetBaseClr, 0.12f);

        // Anillo interactivo Katsu
        u32 katsuBaseClr = Ui::rgba(42, 135, 148);
        u32 katsuLeftClr = Ui::lerpColor(katsuBaseClr, Ui::rgba(200, 250, 255), drumHitKatsuLeft);
        u32 katsuRightClr = Ui::lerpColor(katsuBaseClr, Ui::rgba(200, 250, 255), drumHitKatsuRight);

        const int SEGS = 36;
        for (int i = 0; i < SEGS; ++i) {
            float a0 = (i * static_cast<float>(M_PI) * 2.0f) / SEGS;
            float a1 = ((i + 1) * static_cast<float>(M_PI) * 2.0f) / SEGS;
            bool isLeft = (std::cos((a0 + a1) * 0.5f) < 0.0f);
            u32 clr = isLeft ? katsuLeftClr : katsuRightClr;

            C2D_DrawTriangle(
                cx + std::cos(a0) * donRx, cy + std::sin(a0) * donRy, clr,
                cx + std::cos(a1) * donRx, cy + std::sin(a1) * donRy, clr,
                cx + std::cos(a0) * katsuRx, cy + std::sin(a0) * katsuRy, clr,
                0.14f
            );
            C2D_DrawTriangle(
                cx + std::cos(a1) * donRx, cy + std::sin(a1) * donRy, clr,
                cx + std::cos(a1) * katsuRx, cy + std::sin(a1) * katsuRy, clr,
                cx + std::cos(a0) * katsuRx, cy + std::sin(a0) * katsuRy, clr,
                0.14f
            );
        }

        // 4. Parche central Don (Cuero melocotón)
        u32 donBasePeach = Ui::rgba(255, 168, 143);
        u32 donHitClr    = Ui::rgba(255, 50, 60);
        u32 donLeftClr   = drumHitDonLeft > 0.01f ? Ui::lerpColor(donHitClr, CLR_WHITE, drumHitDonLeft * 0.6f) : donBasePeach;
        u32 donRightClr  = drumHitDonRight > 0.01f ? Ui::lerpColor(donHitClr, CLR_WHITE, drumHitDonRight * 0.6f) : donBasePeach;

        for (int i = 0; i < SEGS; ++i) {
            float a0 = (i * static_cast<float>(M_PI) * 2.0f) / SEGS;
            float a1 = ((i + 1) * static_cast<float>(M_PI) * 2.0f) / SEGS;
            bool isLeft = (std::cos((a0 + a1) * 0.5f) < 0.0f);
            u32 clr = isLeft ? donLeftClr : donRightClr;

            C2D_DrawTriangle(
                cx, cy, clr,
                cx + std::cos(a0) * donRx, cy + std::sin(a0) * donRy, clr,
                cx + std::cos(a1) * donRx, cy + std::sin(a1) * donRy, clr,
                0.17f
            );
        }

        // Círculo concéntrico interior de cuero coral
        u32 coralBase    = Ui::rgba(230, 110, 85);
        u32 coralLeftClr = drumHitDonLeft > 0.01f ? Ui::lerpColor(Ui::rgba(255, 80, 80), CLR_WHITE, drumHitDonLeft) : coralBase;
        u32 coralRightClr = drumHitDonRight > 0.01f ? Ui::lerpColor(Ui::rgba(255, 80, 80), CLR_WHITE, drumHitDonRight) : coralBase;

        for (int i = 0; i < SEGS; ++i) {
            float a0 = (i * static_cast<float>(M_PI) * 2.0f) / SEGS;
            float a1 = ((i + 1) * static_cast<float>(M_PI) * 2.0f) / SEGS;
            bool isLeft = (std::cos((a0 + a1) * 0.5f) < 0.0f);
            u32 clr = isLeft ? coralLeftClr : coralRightClr;

            C2D_DrawTriangle(
                cx, cy, clr,
                cx + std::cos(a0) * innerCoralRx, cy + std::sin(a0) * innerCoralRy, clr,
                cx + std::cos(a1) * innerCoralRx, cy + std::sin(a1) * innerCoralRy, clr,
                0.18f
            );
        }

        // Costura central
        C2D_DrawLine(cx, cy - donRy + 2.0f, Ui::rgba(0, 0, 0, 36), cx, cy + donRy - 2.0f, Ui::rgba(0, 0, 0, 36), 1.5f, 0.19f);
        drawEllipseRing(cx, cy, donRx, donRy, 2.0f, Ui::rgba(255, 255, 255, 180), 0.20f, 44);
        drawEllipseRing(cx, cy, katsuRx, katsuRy, 2.0f, Ui::rgba(255, 255, 255, 120), 0.20f, 48);

        // 5. Ondas expansivas de golpe
        if (drumHitDonLeft > 0.05f) {
            float rP = 1.0f - drumHitDonLeft;
            drawEllipseRing(cx - 36.0f, cy + 4.0f, 22.0f + rP * 34.0f, 14.0f + rP * 22.0f, 2.4f, Ui::withAlpha(CLR_WHITE, static_cast<u8>(240 * drumHitDonLeft)), 0.22f, 28);
        }
        if (drumHitDonRight > 0.05f) {
            float rP = 1.0f - drumHitDonRight;
            drawEllipseRing(cx + 36.0f, cy + 4.0f, 22.0f + rP * 34.0f, 14.0f + rP * 22.0f, 2.4f, Ui::withAlpha(CLR_WHITE, static_cast<u8>(240 * drumHitDonRight)), 0.22f, 28);
        }
        if (drumHitKatsuLeft > 0.05f) {
            float rP = 1.0f - drumHitKatsuLeft;
            drawEllipseRing(cx - 78.0f, cy, 20.0f + rP * 30.0f, 15.0f + rP * 24.0f, 2.4f, Ui::withAlpha(Ui::rgba(180, 240, 255), static_cast<u8>(240 * drumHitKatsuLeft)), 0.22f, 28);
        }
        if (drumHitKatsuRight > 0.05f) {
            float rP = 1.0f - drumHitKatsuRight;
            drawEllipseRing(cx + 78.0f, cy, 20.0f + rP * 30.0f, 15.0f + rP * 24.0f, 2.4f, Ui::withAlpha(Ui::rgba(180, 240, 255), static_cast<u8>(240 * drumHitKatsuRight)), 0.22f, 28);
        }

        // 6. Baquetas interactivas tradicionales de madera (Bachi) golpeando el tambor
        {
            // Baqueta Izquierda
            float handleLX = 44.0f;
            float handleLY = 236.0f;
            float restTipLX = cx - 46.0f;
            float restTipLY = cy + 22.0f;
            float hitL = std::max(drumHitDonLeft, drumHitKatsuLeft);
            bool isDonL = (drumHitDonLeft >= drumHitKatsuLeft);
            float targetTipLX = isDonL ? (cx - 36.0f) : (cx - 80.0f);
            float targetTipLY = isDonL ? (cy + 4.0f) : (cy);

            float tipLX = restTipLX + (targetTipLX - restTipLX) * hitL;
            float tipLY = restTipLY + (targetTipLY - restTipLY) * hitL;
            drawBachi(handleLX, handleLY, tipLX, tipLY, hitL, isDonL, 0.225f);

            // Baqueta Derecha
            float handleRX = screenW - 44.0f;
            float handleRY = 236.0f;
            float restTipRX = cx + 46.0f;
            float restTipRY = cy + 22.0f;
            float hitR = std::max(drumHitDonRight, drumHitKatsuRight);
            bool isDonR = (drumHitDonRight >= drumHitKatsuRight);
            float targetTipRX = isDonR ? (cx + 36.0f) : (cx + 80.0f);
            float targetTipRY = isDonR ? (cy + 4.0f) : (cy);

            float tipRX = restTipRX + (targetTipRX - restTipRX) * hitR;
            float tipRY = restTipRY + (targetTipRY - restTipRY) * hitR;
            drawBachi(handleRX, handleRY, tipRX, tipRY, hitR, isDonR, 0.225f);
        }

        // 7. Guía interactiva de controles (si guideAlpha > 0.01f)
        if (guideAlpha > 0.01f && pSettings != nullptr) {
            u8 a = static_cast<u8>(255 * guideAlpha);
            u32 keyTextClr = Ui::rgba(255, 255, 255, a);

            std::string kDonL = GameSettings::formatKeyMask(pSettings->keyTaikoDonLeft);
            std::string kDonR = GameSettings::formatKeyMask(pSettings->keyTaikoDonRight);
            std::string kKatL = GameSettings::formatKeyMask(pSettings->keyTaikoKatsuLeft);
            std::string kKatR = GameSettings::formatKeyMask(pSettings->keyTaikoKatsuRight);

            Ui::pill(cx - 52.0f, cy + 10.0f, 34.0f, 15.0f, 0.25f, Ui::rgba(215, 60, 45, static_cast<u8>(220 * guideAlpha)));
            Ui::pill(cx + 18.0f, cy + 10.0f, 34.0f, 15.0f, 0.25f, Ui::rgba(215, 60, 45, static_cast<u8>(220 * guideAlpha)));
            Ui::pill(cx - 100.0f, cy + 6.0f, 34.0f, 15.0f, 0.25f, Ui::rgba(40, 140, 160, static_cast<u8>(220 * guideAlpha)));
            Ui::pill(cx + 66.0f, cy + 6.0f, 34.0f, 15.0f, 0.25f, Ui::rgba(40, 140, 160, static_cast<u8>(220 * guideAlpha)));

            drawCenteredText(textBuf, cx - 35.0f, cy + 17.5f, 0.28f, keyTextClr, kDonL.c_str(), 0.26f);
            drawCenteredText(textBuf, cx + 35.0f, cy + 17.5f, 0.28f, keyTextClr, kDonR.c_str(), 0.26f);
            drawCenteredText(textBuf, cx - 83.0f, cy + 13.5f, 0.26f, keyTextClr, kKatL.c_str(), 0.26f);
            drawCenteredText(textBuf, cx + 83.0f, cy + 13.5f, 0.26f, keyTextClr, kKatR.c_str(), 0.26f);

            Ui::panel(16.0f, 8.0f, 288.0f, 22.0f, 0.85f, Ui::rgba(14, 28, 36, static_cast<u8>(235 * guideAlpha)), Ui::rgba(255, 100, 140, a));
            drawCenteredText(textBuf, 160.0f, 19.0f, 0.32f, Ui::rgba(255, 235, 245, a), "CONTROLES TAIKO: ROJO = DON (Centro) | AZUL = KATSU (Borde)", 0.88f);
        }
    }

    // Entrada táctil en el tambor interactivo
    int handleTouchDrum(float px, float py) {
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
};
