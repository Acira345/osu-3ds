#pragma once
#include <3ds.h>
#include <cstdint>
#include "Beatmap.hpp"

struct ModDef {
    const char* code;
    const char* shortName;
    const char* fullName;
    const char* desc;
    uint32_t bit;
    u32 color;
};

extern const ModDef modTable[15];
