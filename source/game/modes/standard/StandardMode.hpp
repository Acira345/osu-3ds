#pragma once
#include "AppContext.hpp"

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
    );

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
    );

    void renderCursorAndSmoke(
        AppContext& ctx,
        float baseOffsetX,
        float baseOffsetY,
        float dCursor,
        bool anyHeld
    );

    void renderBottomTablet(AppContext& ctx);
}
