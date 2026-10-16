#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <algorithm>

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

} // namespace Ui
