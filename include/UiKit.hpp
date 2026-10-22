#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <algorithm>
#include <vector>

// Kit visual estilo osu! lazer: degradados, tarjetas inclinadas, triangulos flotantes, brillos y pildoras.
namespace Ui {

inline u8 clamp8(float v) { return static_cast<u8>(std::max(0.0f, std::min(255.0f, v))); }

inline u32 rgba(int r, int g, int b, int a = 255) {
    return C2D_Color32(static_cast<u8>(r), static_cast<u8>(g), static_cast<u8>(b), static_cast<u8>(a));
}

inline u32 withAlpha(u32 c, u8 a) { return (c & 0x00FFFFFF) | (static_cast<u32>(a) << 24); }

// C2D_Color32 empaqueta como 0xAABBGGRR
inline u32 lerpColor(u32 a, u32 b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    auto ch = [&](int sh) { return clamp8(((a >> sh) & 0xFF) * (1.0f - t) + ((b >> sh) & 0xFF) * t); };
    return (static_cast<u32>(ch(24)) << 24) | (static_cast<u32>(ch(16)) << 16) |
           (static_cast<u32>(ch(8)) << 8) | static_cast<u32>(ch(0));
}

inline u32 brighten(u32 c, float f) {
    auto ch = [&](int sh) { return clamp8(((c >> sh) & 0xFF) * f); };
    return (c & 0xFF000000) | (static_cast<u32>(ch(16)) << 16) | (static_cast<u32>(ch(8)) << 8) | static_cast<u32>(ch(0));
}

inline float approach(float cur, float target, float speed) {
    float d = target - cur;
    if (std::fabs(d) < 0.001f) return target;
    return cur + d * speed;
}

inline void gradV(float x, float y, float w, float h, float z, u32 top, u32 bottom) {
    C2D_DrawRectangle(x, y, z, w, h, top, top, bottom, bottom);
}

inline void gradH(float x, float y, float w, float h, float z, u32 left, u32 right) {
    C2D_DrawRectangle(x, y, z, w, h, left, right, left, right);
}

// Paralelogramo (tarjeta inclinada de osu! lazer)
inline void slantRect(float x, float y, float w, float h, float slant, float z, u32 color) {
    C2D_DrawTriangle(x + slant, y, color, x + w, y, color, x, y + h, color, z);
    C2D_DrawTriangle(x + w, y, color, x + w - slant, y + h, color, x, y + h, color, z);
}

inline void slantRectGrad(float x, float y, float w, float h, float slant, float z, u32 left, u32 right) {
    C2D_DrawTriangle(x + slant, y, left, x + w, y, right, x, y + h, left, z);
    C2D_DrawTriangle(x + w, y, right, x + w - slant, y + h, right, x, y + h, left, z);
}

// Rectangulo con sombra suave + borde superior brillante
inline void panel(float x, float y, float w, float h, float z, u32 color, u32 accent = 0) {
    C2D_DrawRectSolid(x + 2.0f, y + 3.0f, z - 0.01f, w, h, rgba(0, 0, 0, 90));
    gradV(x, y, w, h, z, brighten(color, 1.25f), color);
    C2D_DrawRectSolid(x, y, z + 0.005f, w, 1.0f, rgba(255, 255, 255, 40));
    if (accent) C2D_DrawRectSolid(x, y, z + 0.006f, 3.0f, h, accent);
}

// Pildora redondeada (estrellas, etiquetas)
inline void pill(float x, float y, float w, float h, float z, u32 color) {
    float r = h * 0.5f;
    C2D_DrawRectSolid(x + r, y, z, w - h, h, color);
    C2D_DrawCircleSolid(x + r, y + r, z, r, color);
    C2D_DrawCircleSolid(x + w - r, y + r, z, r, color);
}

// Brillo radial falso con circulos concentricos translucidos
inline void glow(float cx, float cy, float z, float radius, u32 color, int layers = 5) {
    u8 base = static_cast<u8>((color >> 24) & 0xFF);
    for (int i = layers; i >= 1; --i) {
        float t = static_cast<float>(i) / layers;
        C2D_DrawCircleSolid(cx, cy, z, radius * t, withAlpha(color, static_cast<u8>(base / (layers + 1) * (layers - i + 1) / 1.6f)));
    }
}

// Boton estilo tarjeta multijugador con degradado, sombra, brillo superior y acento luminoso
inline void button(float x, float y, float w, float h, float z, u32 bgColor, u32 accentColor = 0, bool active = false) {
    float rx = std::round(x);
    float ry = std::round(y);
    float rw = std::round(w);
    float rh = std::round(h);

    if (active) {
        glow(rx + rw * 0.5f, ry + rh * 0.5f, z - 0.02f, std::max(rw, rh) * 0.50f, withAlpha(accentColor ? accentColor : bgColor, 95), 3);
    }
    // Sombra suave proyectada
    C2D_DrawRectSolid(rx + 2.0f, ry + 3.0f, z - 0.01f, rw, rh, rgba(0, 0, 0, 95));
    // Fondo en gradiente vertical
    gradV(rx, ry, rw, rh, z, brighten(bgColor, active ? 1.35f : 1.20f), bgColor);
    // Borde superior brillante tipo cristal
    C2D_DrawRectSolid(rx, ry, z + 0.004f, rw, 1.0f, rgba(255, 255, 255, active ? 80 : 45));
    // Barra de acento y marco nitido pixel-perfect
    if (accentColor) {
        if (active) {
            pill(rx + 3.0f, ry + 3.0f, 4.0f, rh - 6.0f, z + 0.006f, accentColor);
            C2D_DrawRectSolid(rx, ry, z + 0.005f, rw, 1.0f, accentColor);
            C2D_DrawRectSolid(rx, ry + rh - 1.0f, z + 0.005f, rw, 1.0f, accentColor);
            C2D_DrawRectSolid(rx, ry, z + 0.005f, 1.0f, rh, accentColor);
            C2D_DrawRectSolid(rx + rw - 1.0f, ry, z + 0.005f, 1.0f, rh, accentColor);
        } else {
            C2D_DrawRectSolid(rx, ry, z + 0.005f, 3.5f, rh, accentColor);
            u32 bClr = rgba(50, 44, 72, 160);
            C2D_DrawRectSolid(rx, ry, z + 0.003f, rw, 1.0f, bClr);
            C2D_DrawRectSolid(rx, ry + rh - 1.0f, z + 0.003f, rw, 1.0f, bClr);
            C2D_DrawRectSolid(rx, ry, z + 0.003f, 1.0f, rh, bClr);
            C2D_DrawRectSolid(rx + rw - 1.0f, ry, z + 0.003f, 1.0f, rh, bClr);
        }
    }
}

// Pestana interactiva estilo sala multijugador
inline void tab(float x, float y, float w, float h, float z, bool active, u32 activeClr = rgba(40, 215, 235), u32 inactiveClr = rgba(26, 22, 40, 240)) {
    u32 bg = active ? activeClr : inactiveClr;
    panel(x, y, w, h, z, bg, active ? rgba(255, 255, 255, 220) : 0);
}

inline void ring(float x, float y, float z, float r, float th, u32 c, int seg = 32) {
    const float step = 6.2831853f / seg;
    for (int i = 0; i < seg; ++i) {
        float a0 = i * step, a1 = (i + 1) * step;
        C2D_DrawLine(x + std::cos(a0) * r, y + std::sin(a0) * r, c,
                     x + std::cos(a1) * r, y + std::sin(a1) * r, c, th, z);
    }
}

// Fondo animado: degradado + triangulos flotantes (como el menu de osu!)
inline void backdrop(float w, float h, float anim, u32 topClr, u32 botClr, u32 tint, float z = 0.01f) {
    gradV(0.0f, 0.0f, w, h, z, topClr, botClr);
    for (int i = 0; i < 16; ++i) {
        float seed = i * 12.9898f;
        float size = 14.0f + std::fmod(seed * 7.0f, 34.0f);
        float speed = 0.25f + std::fmod(seed * 3.0f, 0.7f);
        float x = std::fmod(seed * 37.0f, w);
        float y = h + size - std::fmod(anim * speed + seed * 19.0f, h + size * 2.0f);
        u8 a = static_cast<u8>(10 + (i % 4) * 6);
        u32 c = withAlpha(tint, a);
        C2D_DrawTriangle(x, y - size, c, x - size * 0.87f, y + size * 0.5f, c, x + size * 0.87f, y + size * 0.5f, c, z + 0.001f);
    }
}

// Barra de estadistica con degradado y marca
inline void statBar(float x, float y, float w, float h, float z, float ratio, u32 fillClr) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    C2D_DrawRectSolid(x, y, z, w, h, rgba(10, 9, 16, 255));
    if (ratio > 0.0f) gradH(x, y, w * ratio, h, z + 0.01f, brighten(fillClr, 0.65f), fillClr);
}

// Slider interactivo estilo osu!lazer con pista redondeada y pomo circular brillante
inline void sliderBar(float x, float y, float w, float h, float z, float ratio, u32 fillClr, bool active = false) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    pill(x, y, w, h, z, rgba(12, 10, 20, 240));
    float fillW = std::max(h, w * ratio);
    if (ratio > 0.01f) {
        pill(x, y, fillW, h, z + 0.01f, fillClr);
    }
    float knobX = x + h * 0.5f + (w - h) * ratio;
    float knobY = y + h * 0.5f;
    float knobR = h * 0.72f + (active ? 1.2f : 0.0f);
    if (active) {
        glow(knobX, knobY, z + 0.015f, knobR * 1.8f, withAlpha(fillClr, 140), 3);
    }
    C2D_DrawCircleSolid(knobX, knobY, z + 0.02f, knobR, active ? rgba(255, 255, 255, 255) : fillClr);
    C2D_DrawCircleSolid(knobX, knobY, z + 0.025f, knobR * 0.48f, active ? fillClr : rgba(255, 255, 255, 235));
}

// Interruptor ON/OFF estilo osu!lazer (pildora con circulo deslizante)
inline void toggleSwitch(float x, float y, float w, float h, float z, bool isOn, u32 onClr) {
    u32 trackClr = isOn ? onClr : rgba(38, 34, 58, 240);
    pill(x, y, w, h, z, trackClr);
    float r = (h * 0.5f) - 2.0f;
    float cx = isOn ? (x + w - (h * 0.5f)) : (x + (h * 0.5f));
    float cy = y + h * 0.5f;
    if (isOn) {
        glow(cx, cy, z + 0.01f, r * 1.9f, withAlpha(onClr, 130), 3);
    }
    C2D_DrawCircleSolid(cx, cy, z + 0.02f, r, isOn ? rgba(255, 255, 255, 255) : rgba(155, 150, 180, 255));
}

// Cortina / fundido suave de transicion entre pantallas estilo osu! lazer
inline void drawTransition(float w, float h, float fade, u32 accentClr) {
    if (fade <= 0.01f) return;
    float t = std::max(0.0f, std::min(1.0f, fade));
    u8 a = static_cast<u8>(t * 235.0f);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.995f, w, h, rgba(12, 10, 22, a));
    float barW = w * t * 0.45f;
    slantRectGrad(-24.0f, 0.0f, barW + 24.0f, h, 24.0f, 0.997f,
                  withAlpha(accentClr, static_cast<u8>(t * 180.0f)),
                  withAlpha(accentClr, 0));
}

// Gráfico de densidad / dificultad estilo osu!lazer
inline void strainGraph(float x, float y, float w, float h, float z,
                        const std::vector<float>& graph, u32 accentClr,
                        float playhead = -1.0f, float alphaMult = 1.0f) {
    if (graph.size() < 2 || w <= 4.0f || h <= 4.0f) return;

    // Panel de fondo sutil
    C2D_DrawRectSolid(x, y, z, w, h, rgba(14, 11, 24, clamp8(185.0f * alphaMult)));
    C2D_DrawRectSolid(x, y + h - 1.0f, z + 0.005f, w, 1.0f, rgba(48, 40, 72, clamp8(200.0f * alphaMult)));

    // Líneas de referencia sutiles (25%, 50%, 75%)
    for (int r = 1; r <= 3; ++r) {
        float ry = y + h - (h * 0.25f * r);
        C2D_DrawLine(x + 2.0f, ry, rgba(255, 255, 255, clamp8(18.0f * alphaMult)),
                     x + w - 2.0f, ry, rgba(255, 255, 255, clamp8(18.0f * alphaMult)), 1.0f, z + 0.002f);
    }

    size_t n = graph.size();
    float step = (w - 2.0f) / static_cast<float>(n - 1);
    float baseY = y + h - 1.0f;

    u32 topFillClr = withAlpha(brighten(accentClr, 1.15f), clamp8(180.0f * alphaMult));
    u32 botFillClr = withAlpha(accentClr, clamp8(35.0f * alphaMult));
    u32 ridgeClr   = withAlpha(brighten(accentClr, 1.45f), clamp8(245.0f * alphaMult));

    // Dibujar área rellena de la curva como quads/triángulos
    for (size_t i = 0; i + 1 < n; ++i) {
        float x0 = x + 1.0f + i * step;
        float x1 = x + 1.0f + (i + 1) * step;
        float y0 = baseY - (std::max(0.04f, std::min(1.0f, graph[i])) * (h - 4.0f));
        float y1 = baseY - (std::max(0.04f, std::min(1.0f, graph[i + 1])) * (h - 4.0f));

        // Gradiente vertical dentro de la silueta montañosa
        C2D_DrawTriangle(x0, y0, topFillClr, x1, y1, topFillClr, x0, baseY, botFillClr, z + 0.01f);
        C2D_DrawTriangle(x1, y1, topFillClr, x1, baseY, botFillClr, x0, baseY, botFillClr, z + 0.01f);

        // Línea de cresta superior brillante
        C2D_DrawLine(x0, y0, ridgeClr, x1, y1, ridgeClr, 1.6f, z + 0.02f);
    }

    // Indicador de posición / Playhead (si está activo)
    if (playhead >= 0.0f && playhead <= 1.0f) {
        float px = x + 1.0f + playhead * (w - 2.0f);
        float pPos = playhead * static_cast<float>(n - 1);
        size_t pIdx = static_cast<size_t>(pPos);
        float pFrac = pPos - static_cast<float>(pIdx);
        float v0 = graph[std::min(pIdx, n - 1)];
        float v1 = graph[std::min(pIdx + 1, n - 1)];
        float py = baseY - ((v0 * (1.0f - pFrac) + v1 * pFrac) * (h - 4.0f));

        C2D_DrawLine(px, y, rgba(255, 255, 255, clamp8(140.0f * alphaMult)),
                     px, baseY, rgba(255, 255, 255, clamp8(220.0f * alphaMult)), 1.2f, z + 0.03f);

        glow(px, py, z + 0.035f, 5.0f, withAlpha(accentClr, clamp8(180.0f * alphaMult)), 3);
        C2D_DrawCircleSolid(px, py, z + 0.04f, 2.5f, rgba(255, 255, 255, clamp8(255.0f * alphaMult)));
    }

    // Borde exterior
    C2D_DrawLine(x, y, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), x + w, y, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), 1.0f, z + 0.025f);
    C2D_DrawLine(x, baseY, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), x + w, baseY, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), 1.0f, z + 0.025f);
    C2D_DrawLine(x, y, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), x, baseY, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), 1.0f, z + 0.025f);
    C2D_DrawLine(x + w, y, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), x + w, baseY, rgba(60, 52, 90, clamp8(160.0f * alphaMult)), 1.0f, z + 0.025f);
}

} // namespace Ui
