#pragma once
#include <3ds.h>
#include "Beatmap.hpp"

struct HitPopup {
    float x;
    float y;
    HitState state;
    float burstRadius;
    u32 burstColor;
    int timer;
    HitPopup(float _x = 0.0f, float _y = 0.0f, HitState _s = HIT_300, float _r = 16.0f, u32 _c = 0, int _t = 0)
        : x(_x), y(_y), state(_s), burstRadius(_r), burstColor(_c), timer(_t) {}
};

struct TrailPoint {
    float x, y;
    int life;
    int maxLife;
};

struct SmokeParticle {
    float x, y;
    float vx, vy;
    float size;
    int life;
    int maxLife;
};

struct HitErrorTick {
    float errMs;
    HitState state;
    int life;
};

struct GameplayParticle {
    float x, y;
    float vx, vy;
    float size;
    u32 color;
    int life, maxLife;
};
