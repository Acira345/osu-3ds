#pragma once
#include <3ds.h>
#include <citro2d.h>

// Playfield seguro centrado con márgenes para que ningún círculo ni slider se salga de la pantalla (Safe Area)
constexpr float SCALE_FACTOR = 0.5208333f; // 200.0f / 384.0f
constexpr float PLAYFIELD_W  = 266.6667f;  // 512.0f * SCALE_FACTOR
constexpr float PLAYFIELD_H  = 200.0f;     // 384.0f * SCALE_FACTOR

constexpr float PLAYFIELD_BOT_OFFSET_X = 26.6667f; // (320.0f - PLAYFIELD_W) * 0.5f
constexpr float PLAYFIELD_BOT_OFFSET_Y = 20.0f;    // (240.0f - PLAYFIELD_H) * 0.5f

constexpr float PLAYFIELD_TOP_OFFSET_X = 66.6667f; // (400.0f - PLAYFIELD_W) * 0.5f
constexpr float PLAYFIELD_TOP_OFFSET_Y = 20.0f;    // (240.0f - PLAYFIELD_H) * 0.5f

inline float osuToTopX(float osuX) { return PLAYFIELD_TOP_OFFSET_X + (osuX * SCALE_FACTOR); }
inline float osuToTopY(float osuY) { return PLAYFIELD_TOP_OFFSET_Y + (osuY * SCALE_FACTOR); }

inline u32 getStarRatingColor(float stars) {
    if (stars < 2.0f) return C2D_Color32(100, 240, 120, 255); // Easy (Verde)
    if (stars < 2.7f) return C2D_Color32(70, 220, 255, 255);  // Normal (Cian)
    if (stars < 4.0f) return C2D_Color32(255, 224, 85, 255);  // Hard (Oro)
    if (stars < 5.3f) return C2D_Color32(255, 100, 170, 255); // Insane (Rosa)
    return C2D_Color32(205, 110, 255, 255);                   // Expert+ (Violeta)
}
